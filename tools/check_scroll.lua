local flux = require "flux"

assert(flux.create {
    title = "flux-scroll",
    width = 600,
    height = 500,
    backdrop = "none",
})
flux.show()

local rows = 1000
local frames = 300
local selected = 1
local elapsed = 0.0

for frame = 1, frames do
    if not flux.pump() then
        break
    end
    if not flux.begin_frame(true) then
        break
    end
    flux.clear(0xFF1E1E1E)
    flux.set_child_scroll("list", (frame - 1) * 37 % 30000)
    flux.set_cursor_pos(20, 20)
    flux.begin_child("bench", 560, 440, { id = "list" })

    local start = os.clock()
    for index = 1, rows do
        if flux.selectable("row " .. index, index == selected, { id = "row" .. index, w = 540, h = 28 }) then
            selected = index
        end
    end
    elapsed = elapsed + (os.clock() - start)

    flux.end_child()
    if not flux.end_frame() then
        break
    end
end

print(("scroll: %.3f ms/frame (%d rows, %d frames)"):format(
    elapsed / frames * 1000, rows, frames))
flux.shutdown()
