local flux = require "flux"

local M = {}

local UPDATE = os.getenv("FLUX_UPDATE_GOLDEN") == "1"

function M.reset_style()
    flux.set_style_color("text", 0xFFF2F2F2)
    flux.set_style_color("text_dim", 0xFFB4B4B4)
    flux.set_style_color("panel", 0xA62A2A2A)
    flux.set_style_color("panel_hover", 0xC2333333)
    flux.set_style_color("panel_active", 0xD23B3B3B)
    flux.set_style_color("border", 0x26FFFFFF)
    flux.set_style_color("accent", 0xFF3C8AE0)
    flux.set_style_color("accent_hover", 0xFF5A9BEA)
    flux.set_style_color("accent_active", 0xFF2F74C0)
    flux.set_style_color("white", 0xFFFFFFFF)
    flux.set_style_color("disabled", 0xFF6E6E6E)
    flux.set_style_color("track", 0x33000000)
    flux.set_style_color("popup_bg", 0xA62A2A2A)
    flux.set_style_color("popup_border", 0x26FFFFFF)
    flux.set_style_color("popup_shadow", 0x40000000)
    flux.set_style_color("menu_text", 0xFFF2F2F2)
    flux.set_style_color("menu_shortcut", 0xFFB4B4B4)
    flux.set_style_color("menu_item_hover", 0xC2333333)
    flux.set_style_color("menu_separator", 0x26FFFFFF)
    flux.set_style_color("tooltip_bg", 0xF0202020)
    flux.set_style_color("tooltip_border", 0x26FFFFFF)
    flux.set_style_color("tooltip_shadow", 0x66000000)
    flux.set_style_color("tab_bar_bg", 0x00000000)
    flux.set_style_color("tab_bar_border", 0x00000000)
    flux.set_style_color("tab_bg", 0xA62A2A2A)
    flux.set_style_color("tab_bg_hover", 0xC2333333)
    flux.set_style_color("tab_bg_selected", 0xD23B3B3B)
    flux.set_style_color("tab_text", 0xFFB4B4B4)
    flux.set_style_color("tab_text_selected", 0xFFF2F2F2)
    flux.set_style_color("tab_baseline", 0x26FFFFFF)
    flux.set_style_color("tab_focus", 0xFF3C8AE0)
    flux.set_style_var("item_spacing_x", 8)
    flux.set_style_var("item_spacing_y", 6)
    flux.set_style_var("frame_padding_x", 10)
    flux.set_style_var("frame_padding_y", 4)
    flux.set_style_var("rounding", 6)
    flux.set_style_var("border_size", 1)
    flux.set_style_var("scrollbar_size", 10)
    flux.set_style_var("font_size", 14)
    flux.set_style_var("line_height", 0)
    flux.set_style_var("popup_padding_x", 10)
    flux.set_style_var("popup_padding_y", 4)
    flux.set_style_var("popup_shadow_spread", 4)
    flux.set_style_var("popup_shadow_offset_y", 2)
    flux.set_style_var("menu_item_height", 28)
    flux.set_style_var("menu_item_padding_x", 10)
    flux.set_style_var("menu_separator_gap", 4)
    flux.set_style_var("menu_separator_height", 9)
    flux.set_style_var("menu_header_line_height", 18)
    flux.set_style_var("combo_item_height", 24)
    flux.set_style_var("combo_popup_padding", 6)
    flux.set_style_var("combo_popup_max_height", 260)
    flux.set_style_var("combo_popup_gap", 4)
    flux.set_style_var("combo_popup_min_width", 72)
    flux.set_style_var("tooltip_padding_x", 8)
    flux.set_style_var("tooltip_padding_y", 5)
    flux.set_style_var("tooltip_shadow_spread", 7)
    flux.set_style_var("tooltip_shadow_offset_y", 3)
    flux.set_style_var("tab_height", 28)
    flux.set_style_var("tab_min_width", 36)
    flux.set_style_var("tab_gap", 4)
    flux.set_style_var("tab_padding_x", 6)
    flux.set_style_var("tab_rounding", 4)
    flux.set_style_var("tab_baseline_size", 1)
    flux.set_style_var("tab_font_size", 0)
    flux.set_style_var("scrollbar_idle_alpha", 0)
end

function M.begin(options)
    options = options or {}
    options.background = options.background or 0xFF1E1E1E
    local ok, err = flux.offscreen.create(options)
    assert(ok, err)
    M.reset_style()
    flux.testing.reset()
    flux.testing.debug_set_reduced_motion(false)
end

function M.frame(draw, delta)
    assert(flux.offscreen.begin_frame(delta or (1.0 / 60.0)))
    flux.clear(0xFF1E1E1E)
    draw()
    flux.assert_style_balanced()
    assert(flux.offscreen.end_frame())
end

function M.settle(draw, seconds)
    -- draw must only issue draw commands; do not pass an M.frame wrapper.
    seconds = seconds or 0.5
    local step = 1.0 / 60.0
    for _ = 1, math.max(1, math.ceil(seconds / step)) do
        M.frame(draw, step)
    end
end

function M.check(name)
    local path = "test/golden/" .. name .. ".png"
    if UPDATE then
        local ok, err = flux.offscreen.save(path)
        assert(ok, err)
        return
    end
    local differing, second, total = flux.offscreen.compare(path)
    if differing == nil then
        error("golden compare failed for " .. path .. ": " .. tostring(second))
    end
    local ratio = differing / math.max(1, total)
    if ratio > 0.001 or second > 8 then
        error(("golden mismatch %s: %d/%d pixels, max channel diff %d"):format(
            name, differing, total, second))
    end
end

function M.finish()
    flux.offscreen.destroy()
end

return M
