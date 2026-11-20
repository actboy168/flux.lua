#include "gfx/draw_replay.h"

#include <d2d1helper.h>
#include <wrl/client.h>

#include <algorithm>

#include "gfx/color.h"
#include "gfx/text_engine.h"

namespace flux::gfx {

void replay_commands(ID2D1RenderTarget* target, ID2D1SolidColorBrush* brush,
                     const std::vector<flux::ui::draw_cmd>& commands,
                     std::string_view text_arena, float scale) {
    if (target == nullptr || brush == nullptr) {
        return;
    }
    target->SetAntialiasMode(D2D1_ANTIALIAS_MODE_PER_PRIMITIVE);
    target->SetTextAntialiasMode(D2D1_TEXT_ANTIALIAS_MODE_GRAYSCALE);
    target->SetTransform(D2D1::Matrix3x2F::Identity());

    int clip_depth = 0;
    for (const flux::ui::draw_cmd& cmd : commands) {
        const float x = cmd.x * scale;
        const float y = cmd.y * scale;
        const float w = cmd.width() * scale;
        const float h = cmd.height() * scale;
        const float radius = cmd.corner_radius() * scale;
        const float thickness = cmd.stroke_thickness() * scale;
        switch (cmd.kind) {
        case flux::ui::draw_kind::rect_filled: {
            brush->SetColor(color_from_u32(cmd.color));
            D2D1_RECT_F rect = D2D1::RectF(x, y, x + w, y + h);
            if (radius > 0.0f) {
                target->FillRoundedRectangle(D2D1::RoundedRect(rect, radius, radius), brush);
            } else {
                target->FillRectangle(rect, brush);
            }
            break;
        }
        case flux::ui::draw_kind::rect: {
            brush->SetColor(color_from_u32(cmd.color));
            D2D1_RECT_F rect = D2D1::RectF(x, y, x + w, y + h);
            if (radius > 0.0f) {
                target->DrawRoundedRectangle(D2D1::RoundedRect(rect, radius, radius), brush, thickness);
            } else {
                target->DrawRectangle(rect, brush, thickness);
            }
            break;
        }
        case flux::ui::draw_kind::rect_gradient: {
            D2D1_GRADIENT_STOP stops[2] = {
                {0.0f, color_from_u32(cmd.color)},
                {1.0f, color_from_u32(cmd.color2())},
            };
            Microsoft::WRL::ComPtr<ID2D1GradientStopCollection> collection;
            HRESULT hr = target->CreateGradientStopCollection(
                stops, 2, D2D1_GAMMA_2_2, D2D1_EXTEND_MODE_CLAMP,
                collection.GetAddressOf());
            if (FAILED(hr)) {
                break;
            }
            D2D1_RECT_F rect = D2D1::RectF(x, y, x + w, y + h);
            if (cmd.gradient_kind() == 3) {
                Microsoft::WRL::ComPtr<ID2D1RadialGradientBrush> gradient;
                const D2D1_RADIAL_GRADIENT_BRUSH_PROPERTIES properties =
                    D2D1::RadialGradientBrushProperties(
                        D2D1::Point2F(x + w * 0.5f, y + h * 0.5f),
                        D2D1::Point2F(0.0f, 0.0f),
                        std::max(0.0f, w * 0.5f),
                        std::max(0.0f, h * 0.5f));
                hr = target->CreateRadialGradientBrush(
                    properties, collection.Get(), gradient.GetAddressOf());
                if (SUCCEEDED(hr)) {
                    if (radius > 0.0f) {
                        target->FillRoundedRectangle(
                            D2D1::RoundedRect(rect, radius, radius), gradient.Get());
                    } else {
                        target->FillRectangle(rect, gradient.Get());
                    }
                }
            } else {
                const bool horizontal = cmd.gradient_kind() == 2;
                Microsoft::WRL::ComPtr<ID2D1LinearGradientBrush> gradient;
                const D2D1_LINEAR_GRADIENT_BRUSH_PROPERTIES properties =
                    D2D1::LinearGradientBrushProperties(
                        D2D1::Point2F(x, y),
                        horizontal ? D2D1::Point2F(x + w, y) : D2D1::Point2F(x, y + h));
                hr = target->CreateLinearGradientBrush(
                    properties, collection.Get(), gradient.GetAddressOf());
                if (SUCCEEDED(hr)) {
                    if (radius > 0.0f) {
                        target->FillRoundedRectangle(
                            D2D1::RoundedRect(rect, radius, radius), gradient.Get());
                    } else {
                        target->FillRectangle(rect, gradient.Get());
                    }
                }
            }
            break;
        }
        case flux::ui::draw_kind::text: {
            flux::text::text_options options;
            options.width = cmd.text_width() * scale;
            options.height = cmd.text_height() * scale;
            options.line_height = cmd.line_height() * scale;
            options.max_lines = cmd.max_lines();
            options.align = cmd.text_align();
            options.valign = cmd.text_valign();
            options.weight = cmd.font_weight();
            options.wrap = cmd.wrap();
            options.ellipsis = cmd.ellipsis();
            text_engine_instance().draw(
                target,
                brush,
                cmd.text_cstr_of(text_arena),
                x,
                y,
                cmd.font_size() * scale,
                cmd.color,
                options);
            break;
        }
        case flux::ui::draw_kind::push_clip:
            target->PushAxisAlignedClip(
                D2D1::RectF(x, y, x + w, y + h),
                D2D1_ANTIALIAS_MODE_ALIASED);
            clip_depth += 1;
            break;
        case flux::ui::draw_kind::pop_clip:
            if (clip_depth > 0) {
                target->PopAxisAlignedClip();
                clip_depth -= 1;
            }
            break;
        }
    }
    while (clip_depth > 0) {
        target->PopAxisAlignedClip();
        clip_depth -= 1;
    }
}

void replay_draw_list(ID2D1RenderTarget* target, ID2D1SolidColorBrush* brush,
                      const flux::ui::draw_list& draw_list, uint32_t background, float scale) {
    if (target == nullptr || brush == nullptr) {
        return;
    }
    target->SetAntialiasMode(D2D1_ANTIALIAS_MODE_PER_PRIMITIVE);
    target->SetTextAntialiasMode(D2D1_TEXT_ANTIALIAS_MODE_GRAYSCALE);
    target->SetTransform(D2D1::Matrix3x2F::Identity());
    target->Clear(color_from_u32(background));

    for (int i = 0; i < flux::ui::k_render_layer_count; ++i) {
        replay_commands(target, brush, draw_list.commands(static_cast<flux::ui::render_layer>(i)),
                        draw_list.text_arena(), scale);
    }
}

}
