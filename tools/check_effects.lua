local flux = require "flux"

local ok, err = flux.create {
    title = "flux.effects",
    width = 400,
    height = 300,
    titlebar_height = 30,
    backdrop = "none",
}
assert(ok, err)
flux.show()
assert(flux.layer.exists("content"), "content layer should exist")
assert(flux.layer.exists("overlay"), "overlay layer should exist")
assert(flux.layer.exists("popup"), "popup layer should exist")
assert(flux.layer.exists("tooltip"), "tooltip layer should exist")

local frame = 0
while true do
    if not flux.pump() then
        break
    end
    if not flux.begin_frame(true) then
        break
    end
    frame = frame + 1
    if frame == 20 then
        assert(flux.layer.ensure("content"))
        local shadow_ok, shadow_err = flux.layer.shadow("content", {
            blur = 18,
            offset = { 0, 6 },
            opacity = 0.6,
            color = 0xFF000000,
        })
        assert(shadow_ok, shadow_err)
        assert(flux.layer.shadow("content"), "content should have a shadow")
        assert(flux.layer.shadow("content", nil), "shadow should clear")
        assert(not flux.layer.shadow("content"), "content shadow should be off")

        local clip_ok, clip_err = flux.layer.clip("content", {
            x = 80,
            y = 150,
            w = 240,
            h = 100,
            radius = 12,
        })
        assert(clip_ok, clip_err)
        assert(flux.layer.clip("content"), "content should have a clip")
        assert(flux.layer.clip("content", nil), "clip should clear")
        assert(not flux.layer.clip("content"), "content clip should be off")

        local blur_ok, blur_err = flux.layer.blur("content", 8)
        assert(blur_ok, blur_err)
        assert(math.abs(flux.layer.blur("content") - 8) < 0.001)
        assert(flux.layer.blur("content", 0))
        assert(math.abs(flux.layer.blur("content")) < 0.001)
    end

    flux.clear(0xFF202020)
    flux.rect_filled(0, 0, 400, 300, 0xFF202020)
    flux.rect_filled(30, 30, 340, 80, 0xFF3070C0, 10)
    flux.text(50, 55, "effects check", 0xFFFFFFFF, 18)
    flux.layer.use("content")
    flux.rect_filled(80, 150, 240, 100, 0xFFE8E8E8, 12)
    flux.text(100, 180, "content", 0xFF202020, 18)
    flux.layer.reset()

    if not flux.end_frame() then
        break
    end
    if frame >= 120 then
        break
    end
end

flux.shutdown()
print("effects ok")
