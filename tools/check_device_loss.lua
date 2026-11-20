package.cpath = "build/test/bin/?.dll;" .. package.cpath
local flux = require "flux"

local ok, err = flux.create {
    title = "flux-device-loss",
    width = 480,
    height = 320,
    backdrop = "none",
}
assert(ok, err)
flux.show()

local function frame(index)
    assert(flux.pump(), "pump failed")
    assert(flux.begin_frame(true), "begin_frame failed")
    flux.clear(0xFF202020)
    flux.rect_filled(26, 26, 120, 40, 0xFF3C8AE0, 8)
    flux.text(36, 36, "frame " .. index, 0xFFFFFFFF, 14)
    assert(flux.end_frame(), "end_frame failed: " .. flux.last_error())
end

for index = 1, 10 do
    frame(index)
end

local recovered, recover_error = flux.testing.debug_lose_device()
assert(recovered, recover_error)

for index = 11, 20 do
    frame(index)
end

print("device loss recovery ok")
flux.shutdown()
