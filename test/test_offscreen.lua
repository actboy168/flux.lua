local flux = require "flux"
local helpers = require "test.helpers"

test("offscreen.basic", function()
    helpers.begin {
        width = 320,
        height = 200,
        dpi = 1.0,
    }
    helpers.frame(function()
        flux.rect_filled(20, 20, 140, 48, 0xFF3C8AE0, 8)
        flux.text(32, 32, "Hello flux 中文", 0xFFFFFFFF, 16)
    end)
    helpers.check("offscreen_basic")
    helpers.finish()
end)
