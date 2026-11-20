local flux = require "flux"
local helpers = require "test.helpers"

test("dpi.offscreen", function()
    helpers.begin {
        width = 300,
        height = 200,
        dpi = 1.5,
    }
    local width, height = flux.offscreen.pixel_size()
    assert(width == 450 and height == 300, "unexpected pixel size")

    helpers.frame(function()
        flux.rect_filled(20, 20, 100, 40, 0xFF3C8AE0, 8)
        flux.text(30, 30, "DPI 1.5", 0xFFFFFFFF, 14)
    end)
    helpers.check("dpi_15")
    helpers.finish()
end)
