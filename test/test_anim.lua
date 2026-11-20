local flux = require "flux"
local helpers = require "test.helpers"

test("anim.hover_and_value", function()
    helpers.begin {
        width = 400,
        height = 300,
        dpi = 1.0,
    }
    flux.testing.debug_set_reduced_motion(false)

    local checked = false
    local fraction = 0.2
    local function draw()
        flux.set_cursor_pos(20, 20)
        flux.button("按钮", 120, 36, { id = "anim_button" })
        flux.set_cursor_pos(20, 80)
        flux.checkbox("选项", checked, { id = "anim_check", w = 160, h = 28 })
        flux.set_cursor_pos(20, 150)
        flux.progress_bar("progress", fraction, 100, { h = 8 })
    end
    local function frame(delta)
        helpers.frame(draw, delta or (1.0 / 60.0))
    end

    frame()
    local hover = select(1, flux.testing.anim("anim_button"))
    local value = select(4, flux.testing.anim("anim_check"))
    assert(hover == 0, "idle button should have no hover animation")
    assert(value == 0, "unchecked box should start at value 0")

    flux.testing.mouse_move(70, 38)
    frame()
    hover = select(1, flux.testing.anim("anim_button"))
    assert(hover == 1, "first hover should snap to 1, got " .. tostring(hover))

    flux.testing.mouse_move(320, 260)
    frame()
    hover = select(1, flux.testing.anim("anim_button"))
    assert(hover > 0 and hover < 1, "hover-out should interpolate, got " .. tostring(hover))
    for _ = 1, 60 do
        frame()
    end
    hover = select(1, flux.testing.anim("anim_button"))
    assert(hover == 0, "hover should settle at 0, got " .. tostring(hover))

    flux.testing.mouse_move(70, 38)
    frame()
    hover = select(1, flux.testing.anim("anim_button"))
    assert(hover > 0 and hover < 1, "armed hover-in should interpolate, got " .. tostring(hover))

    checked = true
    frame()
    value = select(4, flux.testing.anim("anim_check"))
    assert(value > 0 and value < 1, "checkbox toggle should interpolate, got " .. tostring(value))
    for _ = 1, 60 do
        frame()
    end
    value = select(4, flux.testing.anim("anim_check"))
    assert(value == 1, "checkbox should settle checked, got " .. tostring(value))

    local progress_before = flux.offscreen.pixel(70, 154)
    fraction = 0.8
    for _ = 1, 20 do
        frame()
    end
    local progress_after = flux.offscreen.pixel(70, 154)
    assert(progress_before ~= progress_after,
        "progress bar should animate toward the new fraction")

    flux.testing.debug_set_reduced_motion(true)
    flux.testing.mouse_move(320, 260)
    frame()
    hover = select(1, flux.testing.anim("anim_button"))
    assert(hover == 0, "reduced motion should snap hover out")
    flux.testing.debug_set_reduced_motion(false)

    helpers.finish()
end)
