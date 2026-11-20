local flux = require "flux"
local helpers = require "test.helpers"

test("widgets.golden", function()
    helpers.begin {
        width = 400,
        height = 260,
        dpi = 1.0,
    }
    helpers.frame(function()
        flux.set_cursor_pos(20, 20)
        flux.button("标准按钮", 120, 36, { id = "button" })
        flux.same_line()
        flux.button("主按钮", 120, 36, { id = "primary", primary = true })
        flux.set_cursor_pos(20, 70)
        flux.checkbox("启用功能", true, { id = "check" })
        flux.set_cursor_pos(20, 115)
        flux.selectable("列表项", true, { id = "row1", w = 240, h = 30 })
        flux.set_cursor_pos(20, 155)
        flux.progress_bar("progress", 0.6, 240, { h = 8, id = 123 })
    end)
    helpers.check("widgets_basic")
    helpers.finish()
end)

test("widgets.click", function()
    helpers.begin {
        width = 400,
        height = 260,
        dpi = 1.0,
    }
    local clicked = false
    helpers.frame(function()
        flux.set_cursor_pos(20, 20)
        clicked = flux.button("按钮", 120, 36, { id = "button" })
    end)
    assert(not clicked, "button should not be clicked without input")

    flux.testing.click(80, 38)
    helpers.frame(function()
        flux.set_cursor_pos(20, 20)
        clicked = flux.button("按钮", 120, 36, { id = "button" })
    end)
    assert(clicked, "button should be clicked after input injection")
    helpers.finish()
end)

test("widgets.mouse_only", function()
    helpers.begin {
        width = 400,
        height = 260,
        dpi = 1.0,
    }

    local hovered = false
    local clicked = false
    local focused = false
    local function frame()
        helpers.frame(function()
            flux.set_cursor_pos(20, 20)
            hovered, _, clicked, _, focused =
                flux.invisible_button("仅鼠标", 120, 30, { id = "mouse_only", focusable = false, interactive = true })
        end)
    end

    frame()
    assert(not hovered, "item should not hover without a pointer")
    flux.testing.mouse_move(60, 35)
    frame()
    assert(hovered, "mouse-only item should hover")

    flux.testing.click(60, 35)
    frame()
    assert(clicked, "mouse-only item should accept clicks")
    assert(not focused, "mouse-only item must not take keyboard focus")
    assert(flux.keyboard_focus_id() == 0, "mouse-only click must not change keyboard focus")

    helpers.finish()
end)

test("widgets.form_row", function()
    helpers.begin {
        width = 400,
        height = 260,
        dpi = 1.0,
    }

    local control_rect
    local cursor
    helpers.frame(function()
        flux.set_cursor_pos(40, 30)
        flux.begin_form_row(100, 10, 28)
        flux.form_label("标签")
        flux.form_align()
        flux.button("控件", 80, 24, { id = "form_control" })
        control_rect = { flux.item_rect() }
        flux.end_form_row()
        cursor = { flux.cursor_pos() }
    end)

    assert(control_rect[1] == 40 + 100 + 10,
        "form_align should place the control after the label column")
    assert(control_rect[2] == 30, "control should start at the row top")
    assert(cursor[2] == 30 + 28 + 6, "form row should advance the cursor")

    helpers.finish()
end)

test("widgets.control_anim", function()
    helpers.begin {
        width = 200,
        height = 120,
        dpi = 1.0,
    }

    local value = 0
    local function draw(selected)
        return function()
            local _, _, _, animated = flux.control_anim("anim_probe", {
                selected = selected,
            })
            value = animated
        end
    end

    helpers.frame(draw(true))
    assert(value == 1, "first appearance should snap to the selected state")
    helpers.frame(draw(false))
    assert(value > 0 and value < 1, "deselection should animate")
    helpers.settle(draw(false), 1.2)
    assert(value == 0, "deselection should settle at 0")

    local no_snap = 0
    local function draw_no_snap()
        no_snap = select(4, flux.control_anim("no_snap_probe", {
            selected = true,
            snap = false,
        }))
    end
    helpers.frame(draw_no_snap)
    assert(no_snap == 0, "snap=false should start from the unselected value")
    helpers.settle(draw_no_snap, 1.2)
    assert(no_snap == 1, "snap=false should animate to the selected value")

    local mixed = flux.mix_color(0xFF000000, 0xFFFFFFFF, 0.5)
    assert((mixed & 0xFF) > 0 and (mixed & 0xFF) < 255, "mix_color should blend channels")
    mixed = flux.mix_color(0x00000000, 0xFFFFFFFF, 0.5)
    assert(mixed == 0x80FFFFFF, ("fading from transparent should not darken, got %08X"):format(mixed))

    helpers.finish()
end)

test("widgets.focus", function()
    helpers.begin {
        width = 400,
        height = 260,
        dpi = 1.0,
    }
    flux.testing.key_press("tab")
    helpers.frame(function()
        flux.set_cursor_pos(20, 20)
        flux.button("A", 100, 30, { id = "a" })
        flux.button("B", 100, 30, { id = "b" })
    end)

    local focused = false
    helpers.frame(function()
        flux.set_cursor_pos(20, 20)
        flux.button("A", 100, 30, { id = "a" })
        focused = flux.is_item_focused()
    end)
    assert(focused, "first button should receive focus after Tab")
    helpers.finish()
end)

test("widgets.focus_visible", function()
    helpers.begin {
        width = 400,
        height = 260,
        dpi = 1.0,
    }

    local function frame()
        helpers.frame(function()
            flux.set_cursor_pos(20, 20)
            flux.button("A", 100, 30, { id = "fv_a" })
            flux.set_cursor_pos(20, 60)
            flux.button("B", 100, 30, { id = "fv_b" })
        end)
    end

    flux.testing.key_press("tab")
    frame()
    assert(flux.testing.focus_visible(), "Tab focus should be visible")

    flux.testing.click(40, 75)
    frame()
    assert(not flux.testing.focus_visible(), "pointer focus should not draw a focus ring")

    flux.set_keyboard_focus("fv_a")
    frame()
    assert(flux.testing.focus_visible(), "programmatic focus should be visible")

    helpers.finish()
end)

test("widgets.checkbox_keys", function()
    helpers.begin {
        width = 400,
        height = 260,
        dpi = 1.0,
    }

    local checked = false
    local function frame()
        helpers.frame(function()
            flux.set_cursor_pos(20, 20)
            local changed
            changed, checked = flux.checkbox("选项", checked, { id = "ck_keys", w = 160, h = 28 })
        end)
    end

    flux.set_keyboard_focus("ck_keys")
    frame()
    flux.testing.key_press("enter")
    frame()
    assert(not checked, "Enter should not toggle a checkbox")

    flux.testing.key_press("space")
    frame()
    assert(checked, "Space should toggle a checkbox")

    helpers.finish()
end)

test("widgets.boolean_args", function()
    assert(not pcall(flux.checkbox, "选项", 1),
        "checkbox should reject non-boolean checked values")
    assert(not pcall(flux.selectable, "选项", nil),
        "selectable should reject non-boolean selected values")
end)

test("widgets.checkbox", function()
    helpers.begin {
        width = 400,
        height = 260,
        dpi = 1.0,
    }
    local checked = true
    local changed = false
    helpers.frame(function()
        flux.set_cursor_pos(20, 20)
        changed, checked = flux.checkbox("选项", checked, { id = "check" })
    end)
    assert(not changed and checked)

    flux.testing.click(30, 34)
    helpers.frame(function()
        flux.set_cursor_pos(20, 20)
        changed, checked = flux.checkbox("选项", checked, { id = "check" })
    end)
    assert(changed and not checked, "checkbox should toggle on click")
    helpers.finish()
end)
