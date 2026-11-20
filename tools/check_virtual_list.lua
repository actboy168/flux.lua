local flux = require "flux"

assert(flux.create {
    title = "flux-vlist",
    width = 600,
    height = 500,
    backdrop = "none",
})
flux.show()

local rows = 1000
local frames = 300
local elapsed = 0.0
local realized = 0
local label = "bench"

for frame = 1, frames do
    if not flux.pump() then
        break
    end
    if not flux.begin_frame(true) then
        break
    end
    flux.clear(0xFF1E1E1E)
    flux.set_cursor_pos(20, 20)
    flux.list_scroll_to(label, (frame - 1) * 37 % rows, "top")

    local start = os.clock()
    local _, first, last = flux.begin_list(label, rows, 24, 560, 440, { mode = "single" })
    for index = first, last do
        flux.list_item(index)
    end
    flux.end_list()
    elapsed = elapsed + (os.clock() - start)
    realized = realized + math.max(0, last - first + 1)

    if not flux.end_frame() then
        break
    end
end

local average = realized / frames
assert(average > 0 and average <= 30,
    ("virtual list should realize only the visible window, got %.1f rows"):format(average))
print(("virtual list: %.3f ms/frame (%d rows, %d frames, %.1f realized)"):format(
    elapsed / frames * 1000, rows, frames, average))
flux.shutdown()
