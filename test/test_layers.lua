local flux = require "flux"
local helpers = require "test.helpers"

test("layers.offscreen", function()
    helpers.begin {
        width = 100,
        height = 80,
        dpi = 1.0,
    }

    helpers.frame(function()
        flux.clear(0xFF000000)
        flux.rect_filled(10, 10, 20, 20, 0xFF00FF00)
        assert(flux.layer.use("overlay"))
        flux.rect_filled(10, 10, 20, 20, 0xFF0000FF)
        assert(flux.layer.use("content"))
        flux.rect_filled(10, 10, 20, 20, 0xFFFF0000)
    end)

    local color = flux.offscreen.pixel(15, 15)
    assert(color ~= nil, "pixel should be readable")
    assert((color & 0xFFFFFF) == 0x0000FF, "overlay must composite above content")

    assert(flux.layer.active() == "content", "begin_frame should reset the active layer")

    flux.push_clip(0, 0, 10, 10)
    assert(flux.layer.use("overlay"), "layer switch should preserve per-layer clips")
    flux.layer.reset()
    flux.pop_clip()
    assert(flux.layer.use("overlay"), "layer switch should remain valid after clips")
    flux.layer.reset()
    assert(flux.layer.active() == "content")

    helpers.finish()
end)

test("layers.stack", function()
    helpers.begin {
        width = 320,
        height = 200,
        dpi = 1.0,
    }

    helpers.frame(function()
        assert(flux.layer.active() == "content")
        assert(flux.layer.push("overlay"))
        assert(flux.layer.active() == "overlay")
        assert(flux.layer.push("popup"))
        assert(flux.layer.active() == "popup")
        assert(flux.layer.pop())
        assert(flux.layer.active() == "overlay")
        assert(flux.layer.pop())
        assert(flux.layer.active() == "content")
        local ok, err = flux.layer.pop()
        assert(not ok and err:find("empty"),
            "popping an empty layer stack should report an error")
    end)

    helpers.finish()
end)
