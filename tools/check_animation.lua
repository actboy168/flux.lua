package.cpath = "build/test/bin/?.dll;" .. package.cpath
local flux = require "flux"

local ok, err = flux.create {
    title = "flux.anim",
    width = 320,
    height = 200,
    titlebar_height = 26,
    backdrop = "none",
}
assert(ok, err)
flux.show()
flux.testing.debug_set_reduced_motion(false)
print(("animation check: software=%s remote=%s limited=%s"):format(
    tostring(flux.software()), tostring(flux.remote_session()), tostring(flux.gfx_limited())))

local function draw()
    flux.clear(0xFF101010)
    flux.rect_filled(20, 20, 100, 40, 0xFF204060)
    flux.layer.use("content")
    flux.rect_filled(20, 120, 120, 40, 0xFF806020, 8)
    flux.text(30, 132, "content", 0xFFFFFFFF, 12)
    flux.layer.reset()
end

local function step()
    if not flux.pump() then
        return nil, "pump stopped"
    end
    if not flux.begin_frame(true) then
        return nil, "begin_frame failed"
    end
    draw()
    if not flux.end_frame() then
        return nil, "end_frame failed: " .. flux.last_error()
    end
    return flux.anim.poll()
end

local function run_until(id, max_frames)
    for _ = 1, max_frames do
        local completed, reason = step()
        if completed == nil then
            return false, reason
        end
        for _, value in ipairs(completed) do
            if value == id then
                return true
            end
        end
    end
    return false, ("animation did not complete (software=%s remote=%s limited=%s)")
        :format(tostring(flux.software()), tostring(flux.remote_session()), tostring(flux.gfx_limited()))
end

local function contains(list, id)
    for _, value in ipairs(list) do
        if value == id then
            return true
        end
    end
    return false
end

local function reset_overlay()
    flux.layer.set("content", { opacity = 0, offset = { 0, 0 }, scale = { 1, 1 }, rotation = 0 })
end

assert(flux.layer.ensure("content"))
reset_overlay()

local show_id = assert(flux.layer.animate("content", {
    opacity = { from = 0, to = 1, duration = 0.12, easing = "out-cubic" },
    offset = { from = { 0, 12 }, to = { 0, 0 }, duration = 0.12, easing = "out-cubic" },
}))
assert(show_id > 0)
assert(flux.anim.running(show_id), "animation should be running after start")

local completed, reason = run_until(show_id, 180)
assert(completed, reason)
local info = flux.layer.get("content")
assert(math.abs(info.opacity - 1) < 0.001, "opacity should finish at 1")
assert(math.abs(info.offset.x) < 0.001 and math.abs(info.offset.y) < 0.001,
    "offset should finish at 0")

flux.layer.set("content", { offset = { 0, 12 } })
local hide_id = assert(flux.layer.animate("content", {
    opacity = { from = 1, to = 0, duration = 0.1, easing = "in-cubic" },
    offset = { from = { 0, 0 }, to = { 0, 12 }, duration = 0.1, easing = "in-cubic" },
}))
completed, reason = run_until(hide_id, 180)
assert(completed, reason)
info = flux.layer.get("content")
assert(math.abs(info.opacity) < 0.001, "opacity should finish at 0")
assert(math.abs(info.offset.y - 12) < 0.001, "offset should finish at 12")

-- Multi-keyframe animation.
reset_overlay()
local key_id = assert(flux.layer.animate("content", {
    opacity = {
        keyframes = {
            { t = 0, value = 0 },
            { t = 0.4, value = 0.8 },
            { t = 1, value = 0.2 },
        },
        duration = 0.12,
    },
}))
completed, reason = run_until(key_id, 180)
assert(completed, reason)
assert(math.abs(flux.layer.get("content").opacity - 0.2) < 0.001)

-- Spring animation.
reset_overlay()
flux.layer.set("content", { offset = { 0, 20 } })
local spring_id = assert(flux.layer.animate("content", {
    offset = {
        from = { 0, 20 },
        to = { 0, 0 },
        spring = { damping = 0.9, period = 0.08 },
    },
}))
assert(flux.anim.running(spring_id))
completed, reason = run_until(spring_id, 240)
assert(completed, reason)
assert(math.abs(flux.layer.get("content").offset.y) < 0.001)

-- Cancellation.
reset_overlay()
local long_id = assert(flux.layer.animate("content", {
    opacity = { from = 0, to = 1, duration = 2.0 },
}))
assert(flux.anim.running(long_id))
assert(flux.anim.stop(long_id, true))
assert(not flux.anim.running(long_id))
assert(math.abs(flux.layer.get("content").opacity - 1) < 0.001)

-- Replacement: the first animation is cancelled and its id is queued.
reset_overlay()
local first_id = assert(flux.layer.animate("content", {
    opacity = { from = 0, to = 1, duration = 2.0 },
}))
local second_id = assert(flux.layer.animate("content", {
    opacity = { from = 0.5, to = 0.7, duration = 0.12 },
}))
local saw_first = false
local saw_second = false
for _ = 1, 120 do
    local ids, step_reason = step()
    assert(ids ~= nil, step_reason)
    if contains(ids, first_id) then
        saw_first = true
    end
    if contains(ids, second_id) then
        saw_second = true
    end
    if saw_second then
        break
    end
end
assert(saw_first, "replaced animation should be reported cancelled")
assert(saw_second, "replacement animation should complete")
assert(math.abs(flux.layer.get("content").opacity - 0.7) < 0.001)

-- Infinite animation stays running until stopped.
local infinite_id = assert(flux.layer.animate("content", {
    opacity = { from = 0, to = 0.4, duration = 0.1, iterations = "infinite" },
}))
step()
assert(flux.anim.running(infinite_id))
assert(flux.anim.stop(infinite_id, true))
assert(math.abs(flux.layer.get("content").opacity - 0.4) < 0.001)

-- Reduced motion applies the final value immediately and queues the id.
flux.testing.debug_set_reduced_motion(true)
reset_overlay()
local reduced_id = assert(flux.layer.animate("content", {
    opacity = { from = 0, to = 0.6, duration = 0.5 },
}))
local immediate = flux.anim.poll()
assert(contains(immediate, reduced_id), "reduced-motion animation should complete immediately")
assert(math.abs(flux.layer.get("content").opacity - 0.6) < 0.001)
assert(not flux.anim.running(reduced_id))
flux.testing.debug_set_reduced_motion(false)

-- DPI converts logical offsets to physical pixels.
flux.testing.debug_set_dpi_scale(1.25)
flux.layer.set("content", { offset = { 10, 20 } })
info = flux.layer.get("content")
assert(math.abs(info.pixel_offset.x - 12.5) < 0.01, "pixel offset x should scale with DPI")
assert(math.abs(info.pixel_offset.y - 25.0) < 0.01, "pixel offset y should scale with DPI")

-- Device loss cancels in-flight animations and reports their ids.
reset_overlay()
local recovery_id = assert(flux.layer.animate("content", {
    opacity = { from = 0, to = 1, duration = 2.0 },
}))
assert(flux.testing.debug_lose_device())
local saw_recovery = false
for _ = 1, 60 do
    local ids, step_reason = step()
    assert(ids ~= nil, step_reason)
    if contains(ids, recovery_id) then
        saw_recovery = true
        break
    end
end
assert(saw_recovery, "recovery should cancel the in-flight animation")
assert(not flux.anim.running(recovery_id))

-- Scale and rotation use the vector/scalar property paths.
reset_overlay()
local transform_id = assert(flux.layer.animate("content", {
    scale = { from = 1, to = 0.9, duration = 0.08 },
    rotation = { from = 0, to = 5, duration = 0.08 },
}))
completed, reason = run_until(transform_id, 180)
assert(completed, reason)
info = flux.layer.get("content")
assert(math.abs(info.scale.x - 0.9) < 0.001, "scale should finish at 0.9")
assert(math.abs(info.rotation - 5) < 0.001, "rotation should finish at 5")

-- Shutdown with an active animation must not crash.
flux.layer.animate("content", {
    opacity = { from = 0, to = 1, duration = 2.0 },
})
flux.shutdown()
print("animation ok dpi=" .. tostring(flux.dpi_scale()))
