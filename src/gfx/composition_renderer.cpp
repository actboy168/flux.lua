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
#include <format>
#include <cwchar>

#include "gfx/color.h"
#include "gfx/draw_replay.h"
#include "gfx/text_engine.h"
#include "gfx/effect_brush.h"
#include "platform/os_win32.h"

namespace flux::gfx {

bool composition_renderer::render_content_all(const flux::ui::draw_list& draw_list, uint32_t background,
                                             float scale) {
    surface& layer = surfaces_[static_cast<int>(flux::ui::render_layer::content)];
    if (!layer.render_context || !layer.swap_chain) {
        last_error_ = "content layer has no swap chain";
        return false;
    }

    ID2D1DeviceContext* context = layer.render_context.Get();
    context->BeginDraw();
    context->SetAntialiasMode(D2D1_ANTIALIAS_MODE_PER_PRIMITIVE);
    context->SetTextAntialiasMode(D2D1_TEXT_ANTIALIAS_MODE_GRAYSCALE);
    context->SetTransform(D2D1::Matrix3x2F::Identity());
    context->Clear(color_from_u32(background));

    Microsoft::WRL::ComPtr<ID2D1SolidColorBrush> brush;
    HRESULT hr = context->CreateSolidColorBrush(
        D2D1::ColorF(D2D1::ColorF::White), brush.GetAddressOf());
    if (FAILED(hr)) {
        detail::composition_fail(last_error_, "ID2D1DeviceContext::CreateSolidColorBrush", hr);
        context->EndDraw();
        return false;
    }
    replay_commands(context, brush.Get(), draw_list.commands(flux::ui::render_layer::content),
                    draw_list.text_arena(), scale);
    replay_commands(context, brush.Get(), draw_list.commands(flux::ui::render_layer::overlay),
                    draw_list.text_arena(), scale);
    replay_commands(context, brush.Get(), draw_list.commands(flux::ui::render_layer::popup),
                    draw_list.text_arena(), scale);
    replay_commands(context, brush.Get(), draw_list.commands(flux::ui::render_layer::tooltip),
                    draw_list.text_arena(), scale);

    hr = context->EndDraw();
    if (FAILED(hr)) {
        detail::composition_fail(last_error_, "ID2D1DeviceContext::EndDraw", hr);
        return false;
    }
    hr = layer.swap_chain->Present(present_interval_, 0);
    if (FAILED(hr)) {
        detail::composition_fail(last_error_, "IDXGISwapChain1::Present", hr);
        return false;
    }
    return true;
}

bool composition_renderer::frame(const flux::ui::draw_list& draw_list, uint32_t background, float dpi_scale) {
    if (!ready()) {
        last_error_ = "renderer is not initialized";
        return false;
    }

    last_error_.clear();
    dpi_scale_ = dpi_scale;

    if (d3d_device_) {
        HRESULT reason = d3d_device_->GetDeviceRemovedReason();
        if (FAILED(reason)) {
            detail::composition_fail(last_error_, "D3D11 device removed", reason);
            return false;
        }
    }

    for (int i = 0; i < flux::ui::k_render_layer_count; ++i) {
        const flux::ui::render_layer id = static_cast<flux::ui::render_layer>(i);
        if (draw_list.has_commands(id) && !surfaces_[i].created) {
            if (!ensure_layer(id, last_error_)) {
                return false;
            }
        }
    }

    const float scale = dpi_scale > 0.0f ? dpi_scale : 1.0f;
    return render_content_all(draw_list, background, scale);
}

}
