local flux = require "flux"
local helpers = require "test.helpers"

local function red(color)
    return color & 0xFF
end

local function green(color)
    return (color >> 8) & 0xFF
end

test("visual.gradient_shadow_offset", function()
    helpers.begin {
        width = 220,
        height = 130,
        dpi = 1.0,
    }

    helpers.frame(function()
        flux.clear(0xFF202020)
        flux.rect_gradient(10, 10, 80, 40, 0xFF0000FF, 0xFF00FF00, 0, false)
        flux.rect_radial(110, 10, 80, 40, 0xFFFFFFFF, 0xFF000000, 0)
        flux.shadow(10, 70, 80, 30, 6, 0x66000000, 8, 4)
        flux.rect_filled(10, 70, 80, 30, 0xFF333333, 6)
        flux.push_offset(20, 0)
        flux.rect_filled(110, 70, 20, 20, 0xFF0000FF, 0)
        flux.pop_offset()
    end)

    local top = flux.offscreen.pixel(50, 13)
    local bottom = flux.offscreen.pixel(50, 47)
    assert(red(top) > green(top), "gradient top should be red")
    assert(green(bottom) > red(bottom), "gradient bottom should be green")

    local center = flux.offscreen.pixel(150, 30)
    local corner = flux.offscreen.pixel(112, 12)
    assert(red(center) > red(corner), "radial gradient should be brighter at center")

    local shadow = flux.offscreen.pixel(6, 95)
    assert(red(shadow) < 0x20, "shadow should darken the background outside the panel")

    local shifted = flux.offscreen.pixel(135, 80)
    local original = flux.offscreen.pixel(115, 80)
    assert(red(shifted) > 0x80, "push_offset should shift drawing commands")
    assert(red(original) < 0x60, "original position should not be painted after offset")

    helpers.finish()
end)
