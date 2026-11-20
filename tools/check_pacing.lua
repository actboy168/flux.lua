local flux = require "flux"

assert(flux.create {
    title = "flux-pacing",
    width = 400,
    height = 300,
    backdrop = "none",
})
flux.show()

local count = 300
local min_ms = 1e9
local max_ms = 0.0
local total_ms = 0.0
local samples = 0
local over_20 = 0
local render_total = 0.0
local wait_total = 0.0

for index = 1, count do
    if not flux.pump() then
        break
    end
    if not flux.begin_frame(true) then
        break
    end
    flux.clear(0xFF1E1E1E)
    flux.rect_filled(20, 20, 100, 40, 0xFF3C8AE0, 8)
    flux.text(30, 30, "pacing", 0xFFFFFFFF, 14)
    flux.end_frame()

    local ms = flux.input.delta_time() * 1000.0
    local render_ms, wait_ms = flux.frame_timings()
    if index > 5 then
        min_ms = math.min(min_ms, ms)
        max_ms = math.max(max_ms, ms)
        total_ms = total_ms + ms
        render_total = render_total + render_ms
        wait_total = wait_total + wait_ms
        samples = samples + 1
        if ms > 20.0 then
            over_20 = over_20 + 1
        end
    end
end

print(("pacing: min=%.2f max=%.2f avg=%.2f over20=%d/%d render=%.2f wait=%.2f"):format(
    min_ms, max_ms, total_ms / math.max(1, samples), over_20, samples,
    render_total / math.max(1, samples), wait_total / math.max(1, samples)))
flux.shutdown()
