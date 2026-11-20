local flux = require "flux"
local helpers = require "test.helpers"

local items = { "Alpha", "Beta", { label = "Gamma", disabled = true }, "Delta" }

test("combo.keyboard", function()
    helpers.begin {
        width = 400,
        height = 300,
        dpi = 1.0,
    }
    flux.testing.debug_set_reduced_motion(false)

    local selected = 1
    local open = false
    local changed = false
    local function frame()
        helpers.frame(function()
            flux.set_cursor_pos(20, 20)
            changed, selected, open = flux.combo("选择", selected, items, { id = "combo", w = 180, h = 30 })
        end)
    end

    frame()
    assert(selected == 1 and not open)

    flux.testing.click(40, 35)
    frame()
    assert(open, "click should open the combo")
    local reveal = flux.testing.combo_reveal("combo")
    assert(reveal > 0 and reveal < 1, "popup should fade in, got " .. tostring(reveal))
    for _ = 1, 60 do
        frame()
    end
    reveal = flux.testing.combo_reveal("combo")
    assert(reveal == 1, "popup reveal should settle at 1, got " .. tostring(reveal))
    for _ = 1, 12 do
        frame()
    end
    assert(flux.testing.combo_reveal("combo") == 1,
        "popup fade must settle without moving item geometry")

    flux.testing.key_press("down")
    frame()
    flux.testing.key_press("enter")
    frame()
    assert(changed and selected == 2 and not open,
        "Down+Enter should select Beta and close, got " .. tostring(selected))
    assert(flux.testing.combo_reveal("combo") == 0,
        "closed popup reveal should be 0")

    flux.testing.click(40, 35)
    frame()
    flux.testing.key_press("down")
    frame()
    flux.testing.key_press("enter")
    frame()
    assert(selected == 4, "keyboard should skip disabled Gamma, got " .. tostring(selected))

    flux.testing.click(40, 35)
    frame()
    flux.testing.key_press("escape")
    frame()
    assert(not open and selected == 4, "Escape should close without changing selection")

    helpers.finish()
end)

test("combo.reveal_reduced", function()
    helpers.begin {
        width = 400,
        height = 300,
        dpi = 1.0,
    }
    flux.testing.debug_set_reduced_motion(true)

    local selected = 1
    local open = false
    local function frame()
        helpers.frame(function()
            flux.set_cursor_pos(20, 20)
            _, selected, open = flux.combo("动效", selected, items, { id = "combo_motion", w = 180, h = 30 })
        end)
    end

    frame()
    flux.testing.click(40, 35)
    frame()
    assert(open, "combo should open")
    assert(flux.testing.combo_reveal("combo_motion") == 1,
        "reduced motion should reveal the popup immediately")

    flux.testing.key_press("escape")
    frame()
    assert(not open)
    assert(flux.testing.combo_reveal("combo_motion") == 0)
    flux.testing.debug_set_reduced_motion(false)
    helpers.finish()
end)

test("combo.wheel", function()
    helpers.begin {
        width = 400,
        height = 360,
        dpi = 1.0,
    }

    local many = {}
    for index = 1, 30 do
        many[index] = string.format("项目%02d", index)
    end

    local selected = 1
    local function draw()
        flux.set_cursor_pos(20, 20)
        flux.begin_child("外层", 340, 300, { id = "combo_outer" })
        flux.set_cursor_pos(20, 20)
        _, selected = flux.combo("选择", selected, many, { id = "combo_wheel", w = 180, h = 30 })
        flux.dummy(10, 600)
        flux.end_child()
    end
    local function frame()
        helpers.frame(draw)
    end

    frame()
    flux.testing.click(40, 35)
    frame()

    local before = flux.child_scroll("combo_list")

    flux.testing.mouse_move(60, 120)
    flux.testing.wheel(-1)
    helpers.settle(draw)

    local after = flux.child_scroll("combo_list")
    assert(math.abs((after - before) - 48) < 0.01,
        ("popup wheel should scroll by 48, got %.2f"):format(after - before))
    assert(flux.child_scroll("combo_outer") == 0,
        "modal popup wheel must not scroll the background child")

    helpers.finish()
end)

test("combo.typeahead", function()
    helpers.begin {
        width = 400,
        height = 300,
        dpi = 1.0,
    }

    local selected = 1
    local open = false
    local function frame(delta)
        helpers.frame(function()
            flux.set_cursor_pos(20, 20)
            _, selected, open = flux.combo("选择", selected, items, { id = "combo_typeahead", w = 180, h = 30 })
        end, delta)
    end

    flux.set_keyboard_focus("combo_typeahead")
    frame()
    flux.testing.text("d")
    frame()
    assert(selected == 4, "typeahead D should select Delta, got " .. tostring(selected))

    frame(1.1)
    flux.testing.text("a")
    frame()
    assert(selected == 1, "typeahead should wrap to Alpha, got " .. tostring(selected))

    frame(1.1)
    flux.testing.text("b")
    frame()
    assert(selected == 2, "typeahead B should select Beta, got " .. tostring(selected))

    frame(1.1)
    flux.testing.text("g")
    frame()
    assert(selected == 2, "typeahead should skip disabled Gamma")

    flux.testing.click(40, 35)
    frame()
    assert(open, "combo should open for Ctrl+Enter")
    flux.testing.key_down("ctrl")
    flux.testing.key_press("enter")
    flux.testing.key_up("ctrl")
    frame()
    assert(selected == 0 and not open, "Ctrl+Enter should clear selection")

    helpers.finish()
end)

test("combo.mouse", function()
    helpers.begin {
        width = 400,
        height = 300,
        dpi = 1.0,
    }

    local selected = 1
    local open = false
    local function frame()
        helpers.frame(function()
            flux.set_cursor_pos(20, 20)
            _, selected, open = flux.combo("选择", selected, items, { id = "combo", w = 180, h = 30 })
        end)
    end

    frame()
    flux.testing.click(40, 35)
    frame()
    assert(open, "clicking the combo should open it")

    -- Click the second popup row.
    flux.testing.click(60, 105)
    frame()
    assert(selected == 2 and not open, "mouse click should select Beta and close")

    helpers.finish()
end)

test("combo.ownership", function()
    helpers.begin {
        width = 400,
        height = 300,
        dpi = 1.0,
    }

    local selected = 1
    local open = false
    local function frame()
        helpers.frame(function()
            flux.set_cursor_pos(20, 20)
            _, selected, open = flux.combo("选择", selected, items,
                { id = "combo_ctl", w = 180, h = 30 })
        end)
    end

    frame()
    assert(not flux.combo_is_open("选择", { id = "combo_ctl" }))
    flux.combo_set_open("选择", true, { id = "combo_ctl" })
    frame()
    assert(open and flux.combo_is_open("选择", { id = "combo_ctl" }),
        "combo_set_open should drive the internal open state")
    flux.combo_set_open("选择", false, { id = "combo_ctl" })
    frame()
    assert(not open and not flux.combo_is_open("选择", { id = "combo_ctl" }))

    helpers.finish()
end)
