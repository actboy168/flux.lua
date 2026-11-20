local flux = require "flux"

local ok, err = flux.create {
    title = "flux-bench",
    width = 560,
    height = 540,
    backdrop = "none",
}
assert(ok, err)


local count = 1000
local frames = 300
local elapsed = 0.0

for _ = 1, frames do
    if not flux.pump() then
        break
    end
    if not flux.begin_frame(true) then
        break
    end
    local start = os.clock()
    for index = 1, count do
        flux.invisible_button("bench", 8, 8, { id = "bench" .. index })
    end
    elapsed = elapsed + (os.clock() - start)
    if not flux.end_frame() then
        break
    end
end

print(("invisible_button: %.3f ms/frame (%d buttons, %d frames)"):format(
    elapsed / frames * 1000, count, frames))
flux.shutdown()
