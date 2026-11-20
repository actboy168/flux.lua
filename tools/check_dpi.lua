package.cpath = "build/test/bin/?.dll;" .. package.cpath
local flux = require "flux"

local ok, err = flux.create {
    title = "flux-dpi",
    width = 560,
    height = 540,
    backdrop = "none",
}
assert(ok, err)
flux.show()

for _, scale in ipairs({ 1.0, 1.25, 1.5 }) do
    flux.testing.debug_set_dpi_scale(scale)
    for _ = 1, 3 do
        assert(flux.pump(), "pump failed")
        assert(flux.begin_frame(true), "begin_frame failed")
        flux.clear(0xFF202020)
        flux.set_cursor_pos(10, 10)
        flux.invisible_button("probe", 100, 30, { id = "probe" .. scale })
        local x, y, width, height = flux.item_rect()
        flux.rect_filled(x, y, width, height, 0xFF3C8AE0, 4)
        assert(flux.end_frame(), "end_frame failed: " .. flux.last_error())
    end
    local logical_width, logical_height = flux.size()
    local pixel_width, pixel_height = flux.pixel_size()
    print(("scale=%.2f logical=%dx%d pixel=%dx%d"):format(
        scale, logical_width, logical_height, pixel_width, pixel_height))
    assert(math.abs(pixel_width - logical_width * scale) <= 2, "pixel width mismatch")
    assert(math.abs(pixel_height - logical_height * scale) <= 2, "pixel height mismatch")
end

print("dpi scale ok")
flux.shutdown()
