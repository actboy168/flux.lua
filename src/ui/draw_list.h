#pragma once

#include <algorithm>
#include <array>
#include <cassert>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <string>
#include <string_view>
#include <type_traits>
#include <utility>
#include <vector>

#include "ui/render_layer.h"

namespace flux::ui {

enum class draw_kind : uint8_t {
    rect_filled,
    rect,
    rect_gradient,
    text,
    push_clip,
    pop_clip,
};

struct draw_cmd {
    draw_kind kind = draw_kind::rect_filled;
    uint8_t gradient_type = 0;
    uint8_t flags = 0;
    uint8_t alpha_byte = 255;
    float x = 0.0f;
    float y = 0.0f;
    uint32_t color = 0xFFFFFFFFu;

    union payload_t {
        struct {
            float w;
            float h;
            float radius;
            float thickness;
        } rect;
        struct {
            float w;
            float h;
            float radius;
            uint32_t color2;
        } gradient;
        struct {
            float w;
            float h;
        } clip;
        struct {
            uint32_t offset;
            uint32_t length;
            float size;
            float text_width;
            float text_height;
            float line_height;
            int32_t max_lines;
            int32_t align;
            int32_t valign;
            int32_t weight;
        } text;
    } payload{};

    bool wrap() const { return (flags & 1u) != 0; }
    bool ellipsis() const { return (flags & 2u) != 0; }

    float width() const {
        switch (kind) {
        case draw_kind::rect_filled:
        case draw_kind::rect:
            return payload.rect.w;
        case draw_kind::rect_gradient:
            return payload.gradient.w;
        case draw_kind::push_clip:
            return payload.clip.w;
        case draw_kind::text:
        case draw_kind::pop_clip:
            return 0.0f;
        }
        return 0.0f;
    }

    float height() const {
        switch (kind) {
        case draw_kind::rect_filled:
        case draw_kind::rect:
            return payload.rect.h;
        case draw_kind::rect_gradient:
            return payload.gradient.h;
        case draw_kind::push_clip:
            return payload.clip.h;
        case draw_kind::text:
        case draw_kind::pop_clip:
            return 0.0f;
        }
        return 0.0f;
    }

    float corner_radius() const {
        switch (kind) {
        case draw_kind::rect_filled:
        case draw_kind::rect:
            return payload.rect.radius;
        case draw_kind::rect_gradient:
            return payload.gradient.radius;
        default:
            return 0.0f;
        }
    }

    float stroke_thickness() const {
        return kind == draw_kind::rect ? payload.rect.thickness : 1.0f;
    }

    float font_size() const {
        return kind == draw_kind::text ? payload.text.size : 14.0f;
    }

    uint32_t color2() const {
        if (kind == draw_kind::rect_gradient) {
            return payload.gradient.color2;
        }
        if (kind == draw_kind::push_clip || kind == draw_kind::pop_clip) {
            return 0xFFFFFFFFu;
        }
        return (static_cast<uint32_t>(alpha_byte) << 24) | 0x00FFFFFFu;
    }

    int gradient_kind() const {
        return kind == draw_kind::rect_gradient ? static_cast<int>(gradient_type) : 0;
    }

    float text_width() const {
        return kind == draw_kind::text ? payload.text.text_width : 0.0f;
    }

    float text_height() const {
        return kind == draw_kind::text ? payload.text.text_height : 0.0f;
    }

    float line_height() const {
        return kind == draw_kind::text ? payload.text.line_height : 0.0f;
    }

    int max_lines() const {
        return kind == draw_kind::text ? payload.text.max_lines : 0;
    }

    int text_align() const {
        return kind == draw_kind::text ? payload.text.align : 0;
    }

    int text_valign() const {
        return kind == draw_kind::text ? payload.text.valign : 0;
    }

    int font_weight() const {
        return kind == draw_kind::text ? payload.text.weight : 400;
    }

    std::string_view text_of(std::string_view arena) const {
        if (kind != draw_kind::text) {
            return {};
        }
        const size_t offset = payload.text.offset;
        const size_t length = payload.text.length;
        if (offset > arena.size() || length > arena.size() - offset) {
            return {};
        }
        return arena.substr(offset, length);
    }

    const char* text_cstr_of(std::string_view arena) const {
        if (kind != draw_kind::text) {
            return "";
        }
        const size_t offset = payload.text.offset;
        const size_t length = payload.text.length;
        if (offset + length >= arena.size()) {
            return "";
        }
        return arena.data() + offset;
    }
};

static_assert(sizeof(draw_cmd) == 56, "draw_cmd should stay compact");
static_assert(offsetof(draw_cmd, payload) == 16, "unexpected draw_cmd layout");
static_assert(std::is_trivially_copyable_v<draw_cmd>);
static_assert(std::is_standard_layout_v<draw_cmd>);

struct draw_list {
    std::array<std::vector<draw_cmd>, k_render_layer_count> layers;
    int active_layer_ = 0;
    std::vector<int> layer_stack;
    std::array<int, k_render_layer_count> clip_depths = {};
    float alpha = 1.0f;
    float offset_x = 0.0f;
    float offset_y = 0.0f;
    std::vector<float> alpha_stack;
    std::vector<std::pair<float, float>> offset_stack;
    std::string text_arena_;

    void clear() {
        for (auto& layer : layers) {
            layer.clear();
        }
        text_arena_.clear();
        active_layer_ = 0;
        layer_stack.clear();
        clip_depths.fill(0);
        alpha = 1.0f;
        offset_x = 0.0f;
        offset_y = 0.0f;
        alpha_stack.clear();
        offset_stack.clear();
    }

    void push_alpha(float value) {
        alpha_stack.push_back(alpha);
        alpha *= std::max(0.0f, value);
    }

    void pop_alpha() {
        if (!alpha_stack.empty()) {
            alpha = alpha_stack.back();
            alpha_stack.pop_back();
        }
    }

    void push_offset(float x, float y) {
        offset_stack.emplace_back(offset_x, offset_y);
        offset_x += x;
        offset_y += y;
    }

    void pop_offset() {
        if (!offset_stack.empty()) {
            offset_x = offset_stack.back().first;
            offset_y = offset_stack.back().second;
            offset_stack.pop_back();
        }
    }

    bool use_layer(render_layer layer) {
        const int index = static_cast<int>(layer);
        if (index < 0 || index >= k_render_layer_count) {
            return false;
        }
        active_layer_ = index;
        return true;
    }

    render_layer active_layer() const {
        return static_cast<render_layer>(active_layer_);
    }

    bool push_layer(render_layer layer) {
        const int index = static_cast<int>(layer);
        if (index < 0 || index >= k_render_layer_count) {
            return false;
        }
        layer_stack.push_back(active_layer_);
        active_layer_ = index;
        return true;
    }

    bool pop_layer() {
        if (layer_stack.empty()) {
            return false;
        }
        active_layer_ = layer_stack.back();
        layer_stack.pop_back();
        return true;
    }

    bool has_commands(render_layer layer) const {
        const int index = static_cast<int>(layer);
        return index >= 0 && index < k_render_layer_count && !layers[index].empty();
    }

    const std::vector<draw_cmd>& commands(render_layer layer) const {
        return layers[static_cast<int>(layer)];
    }

    std::vector<draw_cmd>& commands(render_layer layer) {
        return layers[static_cast<int>(layer)];
    }

    std::string_view text_arena() const {
        return text_arena_;
    }

    std::string_view text_of(const draw_cmd& cmd) const {
        return cmd.text_of(text_arena_);
    }

    const char* text_cstr_of(const draw_cmd& cmd) const {
        return cmd.text_cstr_of(text_arena_);
    }

    void rect_filled(float x, float y, float w, float h, uint32_t color, float radius) {
        draw_cmd cmd = make_cmd(draw_kind::rect_filled);
        cmd.x = x;
        cmd.y = y;
        cmd.color = color;
        cmd.payload.rect = { w, h, radius, 0.0f };
        push(std::move(cmd));
    }

    void rect(float x, float y, float w, float h, uint32_t color, float radius, float thickness) {
        draw_cmd cmd = make_cmd(draw_kind::rect);
        cmd.x = x;
        cmd.y = y;
        cmd.color = color;
        cmd.payload.rect = { w, h, radius, thickness };
        push(std::move(cmd));
    }

    void rect_gradient(float x, float y, float w, float h, uint32_t from, uint32_t to,
                       float radius, bool horizontal) {
        draw_cmd cmd = make_cmd(draw_kind::rect_gradient);
        cmd.gradient_type = horizontal ? 2 : 1;
        cmd.x = x;
        cmd.y = y;
        cmd.color = from;
        cmd.payload.gradient = { w, h, radius, to };
        push(std::move(cmd));
    }

    void rect_radial(float x, float y, float w, float h, uint32_t inner, uint32_t outer,
                     float radius) {
        draw_cmd cmd = make_cmd(draw_kind::rect_gradient);
        cmd.gradient_type = 3;
        cmd.x = x;
        cmd.y = y;
        cmd.color = inner;
        cmd.payload.gradient = { w, h, radius, outer };
        push(std::move(cmd));
    }

    void shadow_rect(float x, float y, float w, float h, float radius, uint32_t color,
                     float spread, float offset_y) {
        // Four scaled rounded rects approximate a soft shadow. More layers add
        // overdraw without a visible gain at typical popup/card sizes.
        constexpr int k_layers = 4;
        for (int layer = k_layers - 1; layer >= 0; --layer) {
            const float t = static_cast<float>(layer + 1) / static_cast<float>(k_layers);
            const float grow = spread * t;
            const float strength = 0.25f + (1.0f - t) * 0.85f;
            const uint32_t alpha_value = static_cast<uint32_t>(
                std::lround(((color >> 24) & 0xFFu) * strength));
            const uint32_t shadow_color = (color & 0x00FFFFFFu)
                | (std::min<uint32_t>(alpha_value, 255u) << 24);
            rect_filled(x - grow, y + offset_y - grow * 0.25f,
                        w + grow * 2.0f, h + grow * 2.0f,
                        shadow_color, radius + grow);
        }
    }

    void text(float x, float y, std::string_view value, uint32_t color, float size) {
        text_options(x, y, value, color, size, 0.0f, 0.0f, 0.0f, 0, 0, 0, 400,
                     false, false);
    }

    void text_options(float x, float y, std::string_view value, uint32_t color, float size,
                      float width, float height, float line_height, int max_lines,
                      int align, int valign, int weight, bool wrap, bool ellipsis) {
        draw_cmd cmd = make_cmd(draw_kind::text);
        cmd.x = x;
        cmd.y = y;
        cmd.color = color;
        cmd.flags = static_cast<uint8_t>((wrap ? 1u : 0u) | (ellipsis ? 2u : 0u));
        const uint32_t offset = append_text(value);
        cmd.payload.text = {
            offset,
            static_cast<uint32_t>(value.size()),
            size,
            width,
            height,
            line_height,
            max_lines,
            align,
            valign,
            weight,
        };
        push(std::move(cmd));
    }

    void push_clip(float x, float y, float w, float h) {
        draw_cmd cmd = make_cmd(draw_kind::push_clip);
        cmd.x = x;
        cmd.y = y;
        cmd.payload.clip = { w, h };
        push(std::move(cmd));
        clip_depths[active_layer_] += 1;
    }

    void pop_clip() {
        draw_cmd cmd = make_cmd(draw_kind::pop_clip);
        push(std::move(cmd));
        if (clip_depths[active_layer_] > 0) {
            clip_depths[active_layer_] -= 1;
        }
    }

private:
    static draw_cmd make_cmd(draw_kind kind) {
        draw_cmd cmd;
        cmd.kind = kind;
        return cmd;
    }

    uint32_t append_text(std::string_view value) {
        assert(value.size() < static_cast<size_t>(std::numeric_limits<uint32_t>::max()));
        assert(text_arena_.size()
               <= static_cast<size_t>(std::numeric_limits<uint32_t>::max()) - value.size() - 1);
        const uint32_t offset = static_cast<uint32_t>(text_arena_.size());
        if (!value.empty()) {
            text_arena_.append(value.data(), value.size());
        }
        text_arena_.push_back('\0');
        return offset;
    }

    void push(draw_cmd&& cmd) {
        if (offset_x != 0.0f || offset_y != 0.0f) {
            cmd.x += offset_x;
            cmd.y += offset_y;
        }
        if (alpha < 1.0f) {
            const float factor = std::max(0.0f, alpha);
            auto scale = [factor](uint32_t color) {
                const uint32_t value = static_cast<uint32_t>(
                    std::lround(((color >> 24) & 0xFFu) * factor));
                return (color & 0x00FFFFFFu)
                    | (std::min<uint32_t>(value, 255u) << 24);
            };
            if (cmd.kind != draw_kind::push_clip && cmd.kind != draw_kind::pop_clip) {
                cmd.color = scale(cmd.color);
                if (cmd.kind == draw_kind::rect_gradient) {
                    cmd.payload.gradient.color2 = scale(cmd.payload.gradient.color2);
                }
                const long alpha_value = std::lround(255.0f * factor);
                cmd.alpha_byte = static_cast<uint8_t>(std::min<long>(alpha_value, 255));
            }
        }
        layers[active_layer_].push_back(std::move(cmd));
    }
};

}
