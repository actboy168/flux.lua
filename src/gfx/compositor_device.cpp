#include "gfx/composition_renderer.h"
#include "gfx/composition_renderer_internal.h"

#include <d2d1_1helper.h>
#include <d2d1helper.h>
#include <DispatcherQueue.h>
#include <roapi.h>
#include <windows.graphics.directx.h>
#include <windows.ui.composition.desktop.h>
#include <winstring.h>

#include <cmath>
#include <cwchar>
#include <format>

#include "gfx/color.h"
#include "gfx/draw_replay.h"
#include "gfx/text_engine.h"
#include "gfx/effect_brush.h"
#include "platform/os_win32.h"

namespace flux::gfx {

bool composition_renderer::init(HWND hwnd, int client_width, int client_height, float dpi_scale,
                               backdrop_mode backdrop, std::string& error) {
    shutdown();
    hwnd_ = hwnd;
    client_width_ = client_width;
    client_height_ = client_height;
    dpi_scale_ = dpi_scale;
    backdrop_ = backdrop;
    software_ = false;
    for (int i = 0; i < flux::ui::k_render_layer_count; ++i) {
        surfaces_[i].layer = static_cast<flux::ui::render_layer>(i);
        base_transforms_[i] = flux::gfx::layer_transform{};
    }

    if (!init_device(error)) {
        shutdown();
        return false;
    }
    if (!init_compositor(error)) {
        shutdown();
        return false;
    }
    return true;
}
bool composition_renderer::recover(std::string& error) {
    HWND hwnd = hwnd_;
    int width = client_width_;
    int height = client_height_;
    float dpi_scale = dpi_scale_;
    backdrop_mode backdrop = backdrop_;
    if (hwnd == nullptr) {
        error = "window is not initialized";
        return false;
    }

    const auto transforms = base_transforms_;
    std::array<bool, flux::ui::k_render_layer_count> requested = {};
    for (int i = 0; i < flux::ui::k_render_layer_count; ++i) {
        requested[i] = surfaces_[i].created;
    }
    cancel_all_animations(false);
    const std::vector<uint64_t> completed = completed_animations_;

    shutdown();
    if (!init(hwnd, width, height, dpi_scale, backdrop, error)) {
        return false;
    }
    completed_animations_ = completed;

    base_transforms_ = transforms;
    for (int i = 0; i < flux::ui::k_render_layer_count; ++i) {
        if (!requested[i]) {
            continue;
        }
        const flux::ui::render_layer id = static_cast<flux::ui::render_layer>(i);
        if (!ensure_layer(id, last_error_)) {
            error = last_error_;
            return false;
        }
        apply_layer_transform(id);
    }
    return true;
}
bool composition_renderer::init_device(std::string& error) {
    const UINT flags = D3D11_CREATE_DEVICE_BGRA_SUPPORT;
    const D3D_FEATURE_LEVEL levels_with_11_1[] = {
        D3D_FEATURE_LEVEL_11_1,
        D3D_FEATURE_LEVEL_11_0,
        D3D_FEATURE_LEVEL_10_1,
        D3D_FEATURE_LEVEL_10_0,
    };
    const D3D_FEATURE_LEVEL levels[] = {
        D3D_FEATURE_LEVEL_11_0,
        D3D_FEATURE_LEVEL_10_1,
        D3D_FEATURE_LEVEL_10_0,
    };

    HRESULT hr = D3D11CreateDevice(
        nullptr,
        D3D_DRIVER_TYPE_HARDWARE,
        nullptr,
        flags,
        levels_with_11_1,
        ARRAYSIZE(levels_with_11_1),
        D3D11_SDK_VERSION,
        d3d_device_.GetAddressOf(),
        nullptr,
        d3d_context_.GetAddressOf());
    if (hr == E_INVALIDARG) {
        hr = D3D11CreateDevice(
            nullptr,
            D3D_DRIVER_TYPE_HARDWARE,
            nullptr,
            flags,
            levels,
            ARRAYSIZE(levels),
            D3D11_SDK_VERSION,
            d3d_device_.GetAddressOf(),
            nullptr,
            d3d_context_.GetAddressOf());
    }
    bool used_warp = false;
    if (FAILED(hr)) {
        used_warp = true;
        hr = D3D11CreateDevice(
            nullptr,
            D3D_DRIVER_TYPE_WARP,
            nullptr,
            flags,
            levels,
            ARRAYSIZE(levels),
            D3D11_SDK_VERSION,
            d3d_device_.GetAddressOf(),
            nullptr,
            d3d_context_.GetAddressOf());
    }
    if (FAILED(hr)) {
        return detail::composition_fail(error, "D3D11CreateDevice", hr);
    }
    software_ = used_warp;

    hr = d3d_device_.As(&dxgi_device_);
    if (FAILED(hr)) {
        return detail::composition_fail(error, "ID3D11Device::QueryInterface(IDXGIDevice)", hr);
    }

    hr = D2D1CreateFactory(D2D1_FACTORY_TYPE_SINGLE_THREADED, d2d_factory_.GetAddressOf());
    if (FAILED(hr)) {
        return detail::composition_fail(error, "D2D1CreateFactory", hr);
    }

    hr = d2d_factory_->CreateDevice(dxgi_device_.Get(), d2d_device_.GetAddressOf());
    if (FAILED(hr)) {
        return detail::composition_fail(error, "ID2D1Factory1::CreateDevice", hr);
    }

    if (!text_engine_instance().ready()) {
        error = "DirectWrite init failed";
        return false;
    }
    return true;
}
bool composition_renderer::init_compositor(std::string& error) {
    DispatcherQueueOptions options = {};
    options.dwSize = sizeof(options);
    options.threadType = DQTYPE_THREAD_CURRENT;
    options.apartmentType = DQTAT_COM_ASTA;
    PDISPATCHERQUEUECONTROLLER controller = nullptr;
    HRESULT dispatcher_hr = CreateDispatcherQueueController(options, &controller);
    if (SUCCEEDED(dispatcher_hr)) {
        dispatcher_controller_.Attach(controller);
    }

    HRESULT hr = OleInitialize(nullptr);
    if (SUCCEEDED(hr)) {
        ole_initialized_ = true;
    } else if (hr != RPC_E_CHANGED_MODE) {
        return detail::composition_fail(error, "OleInitialize", hr);
    }

    HSTRING_HEADER string_header = {};
    HSTRING class_name = nullptr;
    const wchar_t* class_name_text = RuntimeClass_Windows_UI_Composition_Compositor;
    hr = WindowsCreateStringReference(
        class_name_text,
        static_cast<UINT32>(std::wcslen(class_name_text)),
        &string_header,
        &class_name);
    if (FAILED(hr)) {
        return detail::composition_fail(error, "WindowsCreateStringReference", hr);
    }

    Microsoft::WRL::ComPtr<IInspectable> inspectable;
    hr = RoActivateInstance(class_name, inspectable.GetAddressOf());
    if (FAILED(hr)) {
        return detail::composition_fail(error, "RoActivateInstance(Compositor)", hr);
    }
    hr = inspectable.As(&compositor_);
    if (FAILED(hr)) {
        return detail::composition_fail(error, "QueryInterface(ICompositor)", hr);
    }

    hr = compositor_.As(&desktop_interop_);
    if (FAILED(hr)) {
        return detail::composition_fail(error, "QueryInterface(ICompositorDesktopInterop)", hr);
    }
    compositor2_.Reset();
    compositor_.As(&compositor2_);
    compositor5_.Reset();
    compositor_.As(&compositor5_);
    compositor6_.Reset();
    compositor_.As(&compositor6_);

    if (!animator_.init(hwnd_, compositor_.Get(), error)) {
        return false;
    }

    Microsoft::WRL::ComPtr<ABI::Windows::UI::Composition::Desktop::IDesktopWindowTarget> desktop_target;
    hr = desktop_interop_->CreateDesktopWindowTarget(hwnd_, FALSE, desktop_target.GetAddressOf());
    if (FAILED(hr)) {
        return detail::composition_fail(error, "ICompositorDesktopInterop::CreateDesktopWindowTarget", hr);
    }
    hr = desktop_target.As(&target_);
    if (FAILED(hr)) {
        return detail::composition_fail(error, "QueryInterface(ICompositionTarget)", hr);
    }

    hr = compositor_->CreateContainerVisual(root_container_.GetAddressOf());
    if (FAILED(hr)) {
        return detail::composition_fail(error, "ICompositor::CreateContainerVisual", hr);
    }
    hr = root_container_.As(&root_visual_);
    if (FAILED(hr)) {
        return detail::composition_fail(error, "QueryInterface(IVisual)", hr);
    }
    root_visual_->put_Size(visual_size());
    hr = target_->put_Root(root_visual_.Get());
    if (FAILED(hr)) {
        return detail::composition_fail(error, "ICompositionTarget::put_Root", hr);
    }

    Microsoft::WRL::ComPtr<ABI::Windows::UI::Composition::ICompositorInterop> compositor_interop;
    hr = compositor_.As(&compositor_interop);
    if (FAILED(hr)) {
        return detail::composition_fail(error, "QueryInterface(ICompositorInterop)", hr);
    }
    compositor_interop_ = compositor_interop;

    backdrop_native_ = backdrop_ == backdrop_mode::mica && platform::supports_system_backdrop();

    if (!create_layer(flux::ui::render_layer::content, error)) {
        return false;
    }

    const bool host_backdrop = backdrop_ == backdrop_mode::acrylic
        || (backdrop_ == backdrop_mode::mica && !backdrop_native_);
    if (host_backdrop && !create_backdrop(error)) {
        return false;
    }

    update_visual_sizes();
    return true;
}
bool composition_renderer::resize(int client_width, int client_height, float dpi_scale) {
    if (client_width < 1) {
        client_width = 1;
    }
    if (client_height < 1) {
        client_height = 1;
    }
    if (std::abs(dpi_scale - dpi_scale_) > 0.0001f) {
        cancel_all_animations(true);
    }
    client_width_ = client_width;
    client_height_ = client_height;
    dpi_scale_ = dpi_scale;

    if (!ready()) {
        return false;
    }

    for (int i = 0; i < flux::ui::k_render_layer_count; ++i) {
        surface& layer = surfaces_[i];
        if (!layer.created || !layer.swap_chain) {
            continue;
        }
        if (layer.render_context) {
            layer.render_context->SetTarget(nullptr);
        }
        layer.target_bitmap.Reset();
        if (d3d_context_) {
            d3d_context_->Flush();
        }
        HRESULT hr = layer.swap_chain->ResizeBuffers(
            2,
            static_cast<UINT>(client_width),
            static_cast<UINT>(client_height),
            DXGI_FORMAT_B8G8R8A8_UNORM,
            0);
        if (FAILED(hr) || !create_render_target(layer, last_error_)) {
            const flux::ui::render_layer id = static_cast<flux::ui::render_layer>(i);
            destroy_layer(id);
            if (!create_layer(id, last_error_)) {
                return false;
            }
        }
    }

    update_visual_sizes();
    for (int i = 0; i < flux::ui::k_render_layer_count; ++i) {
        if (!surfaces_[i].created) {
            continue;
        }
        const flux::ui::render_layer id = static_cast<flux::ui::render_layer>(i);
        apply_layer_transform(id);
        if (surfaces_[i].has_shadow) {
            std::string shadow_error;
            if (!apply_layer_shadow(id, shadow_error)) {
                last_error_ = shadow_error;
            }
        }
        if (surfaces_[i].has_clip) {
            std::string clip_error;
            if (!apply_layer_clip(id, clip_error)) {
                last_error_ = clip_error;
            }
        }
    }
    return true;
}
void composition_renderer::shutdown() {
    animator_.shutdown();
    pending_animations_.clear();
    completed_animations_.clear();
    destroy_layers();
    backdrop_visual_.Reset();
    backdrop_sprite_.Reset();
    root_visual_.Reset();
    root_container_.Reset();

    target_.Reset();
    desktop_interop_.Reset();
    compositor_interop_.Reset();
    compositor_.Reset();
    dispatcher_controller_.Reset();

    d2d_device_.Reset();
    d2d_factory_.Reset();
    dxgi_device_.Reset();
    d3d_context_.Reset();
    d3d_device_.Reset();

    if (ole_initialized_) {
        OleUninitialize();
        ole_initialized_ = false;
    }

    hwnd_ = nullptr;
    client_width_ = 0;
    client_height_ = 0;
    dpi_scale_ = 1.0f;
    backdrop_ = backdrop_mode::none;
    backdrop_native_ = false;
    for (int i = 0; i < flux::ui::k_render_layer_count; ++i) {
        base_transforms_[i] = flux::gfx::layer_transform{};
    }
}
}
