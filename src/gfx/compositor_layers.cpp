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

bool composition_renderer::ensure_layer(flux::ui::render_layer layer, std::string& error) {
    const int index = static_cast<int>(layer);
    if (index < 0 || index >= flux::ui::k_render_layer_count) {
        error = "invalid render layer";
        return false;
    }
    if (surfaces_[static_cast<int>(flux::ui::render_layer::content)].created) {
        return true;
    }
    return create_layer(flux::ui::render_layer::content, error);
}
bool composition_renderer::layer_exists(flux::ui::render_layer layer) const {
    const int index = static_cast<int>(layer);
    return index >= 0 && index < flux::ui::k_render_layer_count
        && surfaces_[static_cast<int>(flux::ui::render_layer::content)].created;
}
bool composition_renderer::set_layer_transform(flux::ui::render_layer layer, const flux::gfx::layer_transform& transform,
                                              std::string& error) {
    const int index = static_cast<int>(layer);
    if (index < 0 || index >= flux::ui::k_render_layer_count) {
        error = "invalid render layer";
        return false;
    }
    if (layer != flux::ui::render_layer::content) {
        error = "only the content layer supports transforms";
        return false;
    }
    cancel_layer_animations(layer, false);
    base_transforms_[index] = transform;
    apply_layer_transform(layer);
    return true;
}
const flux::gfx::layer_transform& composition_renderer::layer_transform(flux::ui::render_layer layer) const {
    static const flux::gfx::layer_transform empty;
    const int index = static_cast<int>(layer);
    if (index < 0 || index >= flux::ui::k_render_layer_count) {
        return empty;
    }
    return base_transforms_[index];
}
bool composition_renderer::create_swap_chain(surface& layer, std::string& error) {
    Microsoft::WRL::ComPtr<IDXGIAdapter> adapter;
    HRESULT hr = dxgi_device_->GetAdapter(adapter.GetAddressOf());
    if (FAILED(hr)) {
        return detail::composition_fail(error, "IDXGIDevice::GetAdapter", hr);
    }
    Microsoft::WRL::ComPtr<IDXGIFactory2> factory;
    hr = adapter->GetParent(IID_PPV_ARGS(factory.GetAddressOf()));
    if (FAILED(hr)) {
        return detail::composition_fail(error, "IDXGIAdapter::GetParent(IDXGIFactory2)", hr);
    }

    DXGI_SWAP_CHAIN_DESC1 desc = {};
    desc.Width = static_cast<UINT>(client_width_);
    desc.Height = static_cast<UINT>(client_height_);
    desc.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
    desc.Stereo = FALSE;
    desc.SampleDesc.Count = 1;
    desc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    desc.BufferCount = 2;
    desc.Scaling = DXGI_SCALING_STRETCH;
    desc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_SEQUENTIAL;
    desc.AlphaMode = DXGI_ALPHA_MODE_PREMULTIPLIED;
    desc.Flags = 0;

    hr = factory->CreateSwapChainForComposition(
        d3d_device_.Get(), &desc, nullptr, layer.swap_chain.GetAddressOf());
    if (FAILED(hr)) {
        return detail::composition_fail(error, "IDXGIFactory2::CreateSwapChainForComposition", hr);
    }

    hr = compositor_interop_->CreateCompositionSurfaceForSwapChain(
        layer.swap_chain.Get(), layer.composition_surface.GetAddressOf());
    if (FAILED(hr)) {
        return detail::composition_fail(error, "ICompositorInterop::CreateCompositionSurfaceForSwapChain", hr);
    }
    return true;
}
bool composition_renderer::create_render_target(surface& layer, std::string& error) {
    layer.target_bitmap.Reset();
    layer.render_context.Reset();

    HRESULT hr = d2d_device_->CreateDeviceContext(
        D2D1_DEVICE_CONTEXT_OPTIONS_NONE, layer.render_context.GetAddressOf());
    if (FAILED(hr)) {
        return detail::composition_fail(error, "ID2D1Device::CreateDeviceContext", hr);
    }

    Microsoft::WRL::ComPtr<IDXGISurface> back_buffer;
    hr = layer.swap_chain->GetBuffer(0, IID_PPV_ARGS(back_buffer.GetAddressOf()));
    if (FAILED(hr)) {
        return detail::composition_fail(error, "IDXGISwapChain1::GetBuffer", hr);
    }

    const D2D1_BITMAP_PROPERTIES1 properties = D2D1::BitmapProperties1(
        D2D1_BITMAP_OPTIONS_TARGET | D2D1_BITMAP_OPTIONS_CANNOT_DRAW,
        D2D1::PixelFormat(DXGI_FORMAT_B8G8R8A8_UNORM, D2D1_ALPHA_MODE_PREMULTIPLIED),
        96.0f,
        96.0f);
    hr = layer.render_context->CreateBitmapFromDxgiSurface(
        back_buffer.Get(), &properties, layer.target_bitmap.GetAddressOf());
    if (FAILED(hr)) {
        return detail::composition_fail(error, "ID2D1DeviceContext::CreateBitmapFromDxgiSurface", hr);
    }
    layer.render_context->SetTarget(layer.target_bitmap.Get());
    layer.render_context->SetDpi(96.0f, 96.0f);
    return true;
}
bool composition_renderer::create_layer(flux::ui::render_layer layer, std::string& error) {
    const int index = static_cast<int>(layer);
    if (!d3d_device_ || !compositor_ || !compositor_interop_ || !root_container_) {
        error = "renderer is not initialized";
        return false;
    }
    surface& state = surfaces_[index];
    if (state.created) {
        return true;
    }

    if (!create_swap_chain(state, error)) {
        destroy_layer(layer);
        return false;
    }

    HRESULT hr = compositor_->CreateSurfaceBrushWithSurface(
        state.composition_surface.Get(), state.brush.GetAddressOf());
    if (FAILED(hr)) {
        destroy_layer(layer);
        return detail::composition_fail(error, "ICompositor::CreateSurfaceBrushWithSurface", hr);
    }
    state.brush->put_Stretch(ABI::Windows::UI::Composition::CompositionStretch_None);
    state.brush->put_HorizontalAlignmentRatio(0.0f);
    state.brush->put_VerticalAlignmentRatio(0.0f);

    hr = compositor_->CreateSpriteVisual(state.sprite.GetAddressOf());
    if (FAILED(hr)) {
        destroy_layer(layer);
        return detail::composition_fail(error, "ICompositor::CreateSpriteVisual", hr);
    }
    hr = state.sprite.As(&state.visual);
    if (FAILED(hr)) {
        destroy_layer(layer);
        return detail::composition_fail(error, "QueryInterface(IVisual)", hr);
    }
    Microsoft::WRL::ComPtr<ABI::Windows::UI::Composition::ICompositionBrush> brush_base;
    hr = state.brush.As(&brush_base);
    if (FAILED(hr)) {
        destroy_layer(layer);
        return detail::composition_fail(error, "QueryInterface(ICompositionBrush)", hr);
    }
    hr = state.sprite->put_Brush(brush_base.Get());
    if (FAILED(hr)) {
        destroy_layer(layer);
        return detail::composition_fail(error, "ISpriteVisual::put_Brush", hr);
    }

    if (!create_render_target(state, error)) {
        destroy_layer(layer);
        return false;
    }

    state.created = true;
    if (!insert_layer_visual(layer)) {
        destroy_layer(layer);
        return false;
    }

    update_visual_sizes();
    apply_layer_transform(layer);
    if (state.blur > 0.0f) {
        std::string effect_error;
        if (!apply_layer_effect(layer, effect_error)) {
            last_error_ = effect_error;
        }
    }
    if (state.has_shadow) {
        std::string shadow_error;
        if (!apply_layer_shadow(layer, shadow_error)) {
            last_error_ = shadow_error;
        }
    }
    if (state.has_clip) {
        std::string clip_error;
        if (!apply_layer_clip(layer, clip_error)) {
            last_error_ = clip_error;
        }
    }
    return true;
}
bool composition_renderer::insert_layer_visual(flux::ui::render_layer layer) {
    const int index = static_cast<int>(layer);
    surface& state = surfaces_[index];
    Microsoft::WRL::ComPtr<ABI::Windows::UI::Composition::IVisualCollection> children;
    HRESULT hr = root_container_->get_Children(children.GetAddressOf());
    if (FAILED(hr)) {
        return detail::composition_fail(last_error_, "IContainerVisual::get_Children", hr);
    }

    hr = children->InsertAtTop(state.visual.Get());
    if (FAILED(hr)) {
        return detail::composition_fail(last_error_, "IVisualCollection::InsertAtTop", hr);
    }
    for (int i = index + 1; i < flux::ui::k_render_layer_count; ++i) {
        if (surfaces_[i].created) {
            hr = children->InsertAtTop(surfaces_[i].visual.Get());
            if (FAILED(hr)) {
                return detail::composition_fail(last_error_, "IVisualCollection::InsertAtTop", hr);
            }
        }
    }
    return true;
}
bool composition_renderer::remove_layer_visual(flux::ui::render_layer layer) {
    const int index = static_cast<int>(layer);
    surface& state = surfaces_[index];
    if (!state.visual || !root_container_) {
        return true;
    }
    Microsoft::WRL::ComPtr<ABI::Windows::UI::Composition::IVisualCollection> children;
    HRESULT hr = root_container_->get_Children(children.GetAddressOf());
    if (FAILED(hr)) {
        return detail::composition_fail(last_error_, "IContainerVisual::get_Children", hr);
    }
    hr = children->Remove(state.visual.Get());
    if (FAILED(hr) && hr != E_INVALIDARG) {
        return detail::composition_fail(last_error_, "IVisualCollection::Remove", hr);
    }
    return true;
}
void composition_renderer::destroy_layer(flux::ui::render_layer layer) {
    const int index = static_cast<int>(layer);
    surface& state = surfaces_[index];
    remove_layer_visual(layer);
    state.brush.Reset();
    state.effect_brush.Reset();
    state.drop_shadow.Reset();
    state.geometric_clip.Reset();
    state.clip_geometry.Reset();
    if (state.render_context) {
        state.render_context->SetTarget(nullptr);
    }
    state.render_context.Reset();
    state.target_bitmap.Reset();
    state.composition_surface.Reset();
    state.swap_chain.Reset();
    state.visual.Reset();
    state.sprite.Reset();
    state.created = false;
}
void composition_renderer::destroy_layers() {
    for (int i = 0; i < flux::ui::k_render_layer_count; ++i) {
        destroy_layer(static_cast<flux::ui::render_layer>(i));
    }
}
void composition_renderer::apply_layer_transform(flux::ui::render_layer layer) {
    apply_layer_property(layer, flux::gfx::layer_property::opacity);
    apply_layer_property(layer, flux::gfx::layer_property::offset);
    apply_layer_property(layer, flux::gfx::layer_property::scale);
    apply_layer_property(layer, flux::gfx::layer_property::rotation);
}
void composition_renderer::apply_layer_property(flux::ui::render_layer layer, flux::gfx::layer_property property) {
    const int index = static_cast<int>(layer);
    surface& state = surfaces_[index];
    if (!state.visual) {
        return;
    }
    const flux::gfx::layer_transform& transform = base_transforms_[index];
    switch (property) {
    case flux::gfx::layer_property::opacity:
        state.visual->put_Opacity(transform.opacity);
        break;
    case flux::gfx::layer_property::offset: {
        ABI::Windows::Foundation::Numerics::Vector3 offset = {};
        offset.X = transform.offset_x * dpi_scale_;
        offset.Y = transform.offset_y * dpi_scale_;
        offset.Z = 0.0f;
        state.visual->put_Offset(offset);
        break;
    }
    case flux::gfx::layer_property::scale: {
        ABI::Windows::Foundation::Numerics::Vector3 scale = {};
        scale.X = transform.scale_x;
        scale.Y = transform.scale_y;
        scale.Z = 1.0f;
        state.visual->put_Scale(scale);
        break;
    }
    case flux::gfx::layer_property::rotation:
        state.visual->put_RotationAngleInDegrees(transform.rotation);
        break;
    }
}
bool composition_renderer::apply_layer_effect(flux::ui::render_layer layer, std::string& error) {
    const int index = static_cast<int>(layer);
    surface& state = surfaces_[index];
    if (!state.sprite || !state.brush) {
        error = "layer has no visual";
        return false;
    }

    Microsoft::WRL::ComPtr<ABI::Windows::UI::Composition::ICompositionBrush> surface_brush;
    HRESULT hr = state.brush.As(&surface_brush);
    if (FAILED(hr)) {
        return detail::composition_fail(error, "QueryInterface(ICompositionBrush)", hr);
    }

    if (state.blur <= 0.0f) {
        state.effect_brush.Reset();
        hr = state.sprite->put_Brush(surface_brush.Get());
        if (FAILED(hr)) {
            return detail::composition_fail(error, "ISpriteVisual::put_Brush", hr);
        }
        return true;
    }

    state.effect_brush.Reset();
    if (!create_blur_brush(compositor_.Get(), surface_brush.Get(), state.blur,
                           state.effect_brush, error)) {
        state.sprite->put_Brush(surface_brush.Get());
        return false;
    }
    hr = state.sprite->put_Brush(state.effect_brush.Get());
    if (FAILED(hr)) {
        state.effect_brush.Reset();
        state.sprite->put_Brush(surface_brush.Get());
        return detail::composition_fail(error, "ISpriteVisual::put_Brush(effect)", hr);
    }
    return true;
}
bool composition_renderer::set_layer_blur(flux::ui::render_layer layer, float blur, std::string& error) {
    const int index = static_cast<int>(layer);
    if (index < 0 || index >= flux::ui::k_render_layer_count) {
        error = "invalid render layer";
        return false;
    }
    if (layer != flux::ui::render_layer::content) {
        error = "blur is only supported on the content layer";
        return false;
    }
    if (blur < 0.0f) {
        error = "blur must be >= 0";
        return false;
    }
    if (!ensure_layer(layer, error)) {
        return false;
    }
    const float previous = surfaces_[index].blur;
    surfaces_[index].blur = blur;
    if (!apply_layer_effect(layer, error)) {
        surfaces_[index].blur = previous;
        apply_layer_effect(layer, error);
        return false;
    }
    return true;
}
float composition_renderer::layer_blur(flux::ui::render_layer layer) const {
    const int index = static_cast<int>(layer);
    if (index < 0 || index >= flux::ui::k_render_layer_count) {
        return 0.0f;
    }
    return surfaces_[index].blur;
}
bool composition_renderer::apply_layer_shadow(flux::ui::render_layer layer, std::string& error) {
    const int index = static_cast<int>(layer);
    surface& state = surfaces_[index];
    if (!state.sprite) {
        if (!state.has_shadow) {
            return true;
        }
        error = "layer has no visual";
        return false;
    }

    Microsoft::WRL::ComPtr<ABI::Windows::UI::Composition::ISpriteVisual2> sprite2;
    HRESULT hr = state.sprite.As(&sprite2);
    if (FAILED(hr)) {
        if (!state.has_shadow) {
            return true;
        }
        return detail::composition_fail(error, "QueryInterface(ISpriteVisual2)", hr);
    }

    if (!state.has_shadow) {
        state.drop_shadow.Reset();
        hr = sprite2->put_Shadow(nullptr);
        if (FAILED(hr)) {
            return detail::composition_fail(error, "ISpriteVisual2::put_Shadow", hr);
        }
        return true;
    }

    if (compositor2_ == nullptr) {
        error = "ICompositor2 is not available";
        return false;
    }
    if (!state.drop_shadow) {
        hr = compositor2_->CreateDropShadow(state.drop_shadow.GetAddressOf());
        if (FAILED(hr)) {
            return detail::composition_fail(error, "CreateDropShadow", hr);
        }
        Microsoft::WRL::ComPtr<ABI::Windows::UI::Composition::IDropShadow2> shadow2;
        if (SUCCEEDED(state.drop_shadow.As(&shadow2))) {
            shadow2->put_SourcePolicy(
                ABI::Windows::UI::Composition::CompositionDropShadowSourcePolicy_InheritFromVisualContent);
        }
    }

    const flux::gfx::layer_shadow& shadow = state.shadow;
    state.drop_shadow->put_BlurRadius(shadow.blur * dpi_scale_);
    ABI::Windows::Foundation::Numerics::Vector3 offset = {};
    offset.X = shadow.offset_x * dpi_scale_;
    offset.Y = shadow.offset_y * dpi_scale_;
    state.drop_shadow->put_Offset(offset);
    state.drop_shadow->put_Opacity(shadow.opacity);
    ABI::Windows::UI::Color color = {};
    color.A = static_cast<BYTE>((shadow.color >> 24) & 0xFFu);
    color.R = static_cast<BYTE>(shadow.color & 0xFFu);
    color.G = static_cast<BYTE>((shadow.color >> 8) & 0xFFu);
    color.B = static_cast<BYTE>((shadow.color >> 16) & 0xFFu);
    state.drop_shadow->put_Color(color);

    Microsoft::WRL::ComPtr<ABI::Windows::UI::Composition::ICompositionShadow> base_shadow;
    hr = state.drop_shadow.As(&base_shadow);
    if (FAILED(hr)) {
        return detail::composition_fail(error, "QueryInterface(ICompositionShadow)", hr);
    }
    hr = sprite2->put_Shadow(base_shadow.Get());
    if (FAILED(hr)) {
        return detail::composition_fail(error, "ISpriteVisual2::put_Shadow", hr);
    }
    return true;
}
bool composition_renderer::apply_layer_clip(flux::ui::render_layer layer, std::string& error) {
    const int index = static_cast<int>(layer);
    surface& state = surfaces_[index];
    if (!state.visual) {
        if (!state.has_clip) {
            return true;
        }
        error = "layer has no visual";
        return false;
    }
    if (!state.has_clip) {
        state.geometric_clip.Reset();
        state.clip_geometry.Reset();
        HRESULT hr = state.visual->put_Clip(nullptr);
        if (FAILED(hr)) {
            return detail::composition_fail(error, "IVisual::put_Clip", hr);
        }
        return true;
    }
    if (compositor5_ == nullptr || compositor6_ == nullptr) {
        error = "geometric clips are not available on this system";
        return false;
    }

    if (!state.clip_geometry) {
        HRESULT hr = compositor5_->CreateRoundedRectangleGeometry(
            state.clip_geometry.GetAddressOf());
        if (FAILED(hr)) {
            return detail::composition_fail(error, "CreateRoundedRectangleGeometry", hr);
        }
    }
    if (!state.geometric_clip) {
        Microsoft::WRL::ComPtr<ABI::Windows::UI::Composition::ICompositionGeometry> geometry;
        HRESULT hr = state.clip_geometry.As(&geometry);
        if (FAILED(hr)) {
            return detail::composition_fail(error, "QueryInterface(ICompositionGeometry)", hr);
        }
        hr = compositor6_->CreateGeometricClipWithGeometry(
            geometry.Get(), state.geometric_clip.GetAddressOf());
        if (FAILED(hr)) {
            return detail::composition_fail(error, "CreateGeometricClipWithGeometry", hr);
        }
    }

    const flux::gfx::layer_clip& clip = state.clip;
    ABI::Windows::Foundation::Numerics::Vector2 offset = {};
    offset.X = clip.x * dpi_scale_;
    offset.Y = clip.y * dpi_scale_;
    ABI::Windows::Foundation::Numerics::Vector2 size = {};
    size.X = clip.w * dpi_scale_;
    size.Y = clip.h * dpi_scale_;
    ABI::Windows::Foundation::Numerics::Vector2 radius = {};
    radius.X = clip.radius * dpi_scale_;
    radius.Y = clip.radius * dpi_scale_;
    state.clip_geometry->put_Offset(offset);
    state.clip_geometry->put_Size(size);
    state.clip_geometry->put_CornerRadius(radius);

    Microsoft::WRL::ComPtr<ABI::Windows::UI::Composition::ICompositionClip> base_clip;
    HRESULT hr = state.geometric_clip.As(&base_clip);
    if (FAILED(hr)) {
        return detail::composition_fail(error, "QueryInterface(ICompositionClip)", hr);
    }
    hr = state.visual->put_Clip(base_clip.Get());
    if (FAILED(hr)) {
        return detail::composition_fail(error, "IVisual::put_Clip", hr);
    }
    return true;
}
bool composition_renderer::set_layer_clip(flux::ui::render_layer layer, const flux::gfx::layer_clip& clip,
                                         std::string& error) {
    const int index = static_cast<int>(layer);
    if (index < 0 || index >= flux::ui::k_render_layer_count) {
        error = "invalid render layer";
        return false;
    }
    if (layer != flux::ui::render_layer::content) {
        error = "clips are only supported on the content layer";
        return false;
    }
    if (clip.w < 0.0f || clip.h < 0.0f) {
        error = "clip size must be >= 0";
        return false;
    }
    if (!ensure_layer(layer, error)) {
        return false;
    }
    const bool previous_has = surfaces_[index].has_clip;
    const flux::gfx::layer_clip previous = surfaces_[index].clip;
    surfaces_[index].clip = clip;
    surfaces_[index].has_clip = true;
    if (!apply_layer_clip(layer, error)) {
        surfaces_[index].clip = previous;
        surfaces_[index].has_clip = previous_has;
        std::string ignored;
        apply_layer_clip(layer, ignored);
        return false;
    }
    return true;
}
bool composition_renderer::clear_layer_clip(flux::ui::render_layer layer) {
    const int index = static_cast<int>(layer);
    if (index < 0 || index >= flux::ui::k_render_layer_count) {
        return false;
    }
    surfaces_[index].has_clip = false;
    std::string error;
    return apply_layer_clip(layer, error);
}
bool composition_renderer::layer_has_clip(flux::ui::render_layer layer) const {
    const int index = static_cast<int>(layer);
    if (index < 0 || index >= flux::ui::k_render_layer_count) {
        return false;
    }
    return surfaces_[index].has_clip;
}
bool composition_renderer::set_layer_shadow(flux::ui::render_layer layer, const flux::gfx::layer_shadow& shadow,
                                           std::string& error) {
    const int index = static_cast<int>(layer);
    if (index < 0 || index >= flux::ui::k_render_layer_count) {
        error = "invalid render layer";
        return false;
    }
    if (layer != flux::ui::render_layer::content) {
        error = "shadows are only supported on the content layer";
        return false;
    }
    if (!ensure_layer(layer, error)) {
        return false;
    }
    const bool previous_has = surfaces_[index].has_shadow;
    const flux::gfx::layer_shadow previous = surfaces_[index].shadow;
    surfaces_[index].shadow = shadow;
    surfaces_[index].has_shadow = true;
    if (!apply_layer_shadow(layer, error)) {
        surfaces_[index].shadow = previous;
        surfaces_[index].has_shadow = previous_has;
        std::string ignored;
        apply_layer_shadow(layer, ignored);
        return false;
    }
    return true;
}
bool composition_renderer::clear_layer_shadow(flux::ui::render_layer layer) {
    const int index = static_cast<int>(layer);
    if (index < 0 || index >= flux::ui::k_render_layer_count) {
        return false;
    }
    surfaces_[index].has_shadow = false;
    std::string error;
    return apply_layer_shadow(layer, error);
}
bool composition_renderer::layer_has_shadow(flux::ui::render_layer layer) const {
    const int index = static_cast<int>(layer);
    if (index < 0 || index >= flux::ui::k_render_layer_count) {
        return false;
    }
    return surfaces_[index].has_shadow;
}
bool composition_renderer::create_backdrop(std::string& error) {
    Microsoft::WRL::ComPtr<ABI::Windows::UI::Composition::ICompositor3> compositor3;
    HRESULT hr = compositor_.As(&compositor3);
    if (FAILED(hr)) {
        return detail::composition_fail(error, "QueryInterface(ICompositor3)", hr);
    }

    Microsoft::WRL::ComPtr<ABI::Windows::UI::Composition::ICompositionBackdropBrush> brush;
    hr = compositor3->CreateHostBackdropBrush(brush.GetAddressOf());
    if (FAILED(hr)) {
        return detail::composition_fail(error, "ICompositor3::CreateHostBackdropBrush", hr);
    }

    hr = compositor_->CreateSpriteVisual(backdrop_sprite_.GetAddressOf());
    if (FAILED(hr)) {
        return detail::composition_fail(error, "ICompositor::CreateSpriteVisual", hr);
    }
    hr = backdrop_sprite_.As(&backdrop_visual_);
    if (FAILED(hr)) {
        return detail::composition_fail(error, "QueryInterface(IVisual)", hr);
    }
    Microsoft::WRL::ComPtr<ABI::Windows::UI::Composition::ICompositionBrush> brush_base;
    hr = brush.As(&brush_base);
    if (FAILED(hr)) {
        return detail::composition_fail(error, "QueryInterface(ICompositionBrush)", hr);
    }
    hr = backdrop_sprite_->put_Brush(brush_base.Get());
    if (FAILED(hr)) {
        return detail::composition_fail(error, "ISpriteVisual::put_Brush", hr);
    }

    Microsoft::WRL::ComPtr<ABI::Windows::UI::Composition::IVisualCollection> children;
    hr = root_container_->get_Children(children.GetAddressOf());
    if (FAILED(hr)) {
        return detail::composition_fail(error, "IContainerVisual::get_Children", hr);
    }
    hr = children->InsertAtBottom(backdrop_visual_.Get());
    if (FAILED(hr)) {
        return detail::composition_fail(error, "IVisualCollection::InsertAtBottom", hr);
    }

    return true;
}
ABI::Windows::Foundation::Numerics::Vector2 composition_renderer::visual_size() const {
    ABI::Windows::Foundation::Numerics::Vector2 size = {};
    size.X = static_cast<float>(client_width_);
    size.Y = static_cast<float>(client_height_);
    return size;
}
void composition_renderer::update_visual_sizes() {
    const auto size = visual_size();
    if (root_visual_) {
        root_visual_->put_Size(size);
    }
    for (surface& layer : surfaces_) {
        if (layer.visual) {
            layer.visual->put_Size(size);
        }
    }
    if (backdrop_visual_) {
        backdrop_visual_->put_Size(size);
    }
}
}
