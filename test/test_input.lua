local flux = require "flux"
local helpers = require "test.helpers"

test("input.text_edit", function()
    helpers.begin {
        width = 400,
        height = 260,
        dpi = 1.0,
    }

    local value = "abc"
    local changed = false
    local submitted = false
    local focused = false
    local function frame()
        helpers.frame(function()
            flux.set_cursor_pos(20, 20)
            changed, value, submitted, _, focused =
                flux.input_text("输入", value, { id = "text", w = 220, h = 32 })
        end)
    end

    frame()
    assert(value == "abc")
    assert(not changed)

    flux.testing.click(80, 36)
    frame()
    assert(focused, "input should receive focus on click")

    flux.testing.key_press("end")
    frame()
    flux.testing.text("def")
    frame()
    assert(changed and value == "abcdef", "typing should insert at the caret")

    flux.testing.key_press("home")
    frame()
    flux.testing.key_down("shift")
    flux.testing.key_press("right")
    frame()
    flux.testing.key_up("shift")
    flux.testing.text("Y")
    frame()
    assert(value == "Ybcdef", "shift selection should be replaced, got " .. value)

    flux.testing.key_down("ctrl")
    flux.testing.key_press("a")
    frame()
    flux.testing.key_up("ctrl")
    flux.testing.text("Z")
    frame()
    assert(value == "Z", "Ctrl+A should select all, got " .. value)

    flux.testing.key_press("enter")
    frame()
    assert(submitted, "Enter should submit a single-line input")

    helpers.finish()
end)

test("input.mouse_selection", function()
    helpers.begin {
        width = 400,
        height = 260,
        dpi = 1.0,
    }

    local value = "hello world"
    local function frame()
        helpers.frame(function()
            flux.set_cursor_pos(20, 20)
            local changed, next_value = flux.input_text("鼠标", value, { id = "mouse", w = 220, h = 32 })
            if changed then
                value = next_value
            end
        end)
    end

    frame()
    flux.testing.click(45, 36)
    frame()
    flux.testing.text("X")
    frame()
    assert(value == "heXllo world", "click should place the caret, got " .. value)

    value = "hello world"
    flux.testing.reset()
    flux.testing.double_click(80, 36)
    frame()
    flux.testing.text("Y")
    frame()
    assert(value == "hello Y", "double click should select a word, got " .. value)

    helpers.finish()
end)

test("input.ime_composition", function()
    helpers.begin {
        width = 400,
        height = 260,
        dpi = 1.0,
    }

    local value = ""
    local function frame()
        helpers.frame(function()
            flux.set_cursor_pos(20, 20)
            local changed, next_value = flux.input_text("输入法", value, { id = "ime", w = 220, h = 32 })
            if changed then
                value = next_value
            end
        end)
    end

    frame()
    flux.testing.click(40, 36)
    frame()
    flux.testing.ime("ni", 2)
    frame()
    assert(value == "", "composition must not commit into the value")
    flux.testing.ime_commit("你")
    frame()
    assert(value == "你", "IME result should be inserted, got " .. value)

    helpers.finish()
end)

test("input.undo_redo", function()
    helpers.begin {
        width = 400,
        height = 260,
        dpi = 1.0,
    }

    local value = ""
    local function frame()
        helpers.frame(function()
            flux.set_cursor_pos(20, 20)
            local changed, next_value = flux.input_text("撤销", value, { id = "undo", w = 220, h = 32 })
            if changed then
                value = next_value
            end
        end)
    end

    frame()
    flux.testing.click(40, 36)
    frame()
    flux.testing.text("abc")
    frame()
    assert(value == "abc")

    flux.testing.key_down("ctrl")
    flux.testing.key_press("z")
    frame()
    flux.testing.key_up("ctrl")
    assert(value == "", "undo should restore the text before the typing run, got " .. value)

    flux.testing.key_down("ctrl")
    flux.testing.key_press("y")
    frame()
    flux.testing.key_up("ctrl")
    assert(value == "abc", "redo should restore the typing run, got " .. value)

    flux.testing.key_press("backspace")
    frame()
    assert(value == "ab", "backspace should delete the previous grapheme, got " .. value)

    flux.testing.key_press("home")
    frame()
    flux.testing.key_press("delete")
    frame()
    assert(value == "b", "delete should remove the grapheme at the caret, got " .. value)

    helpers.finish()
end)

test("input.tab_focus", function()
    helpers.begin {
        width = 400,
        height = 260,
        dpi = 1.0,
    }

    local first = ""
    local second = ""
    local first_focused = false
    local second_focused = false
    local function frame()
        helpers.frame(function()
            flux.set_cursor_pos(20, 20)
            first_focused = select(5, flux.input_text("一", first, { id = "first", w = 160, h = 32 }))
            flux.set_cursor_pos(20, 70)
            second_focused = select(5, flux.input_text("二", second, { id = "second", w = 160, h = 32 }))
        end)
    end

    flux.testing.key_press("tab")
    frame()
    frame()
    assert(first_focused, "Tab should focus the first input")

    flux.testing.key_press("tab")
    frame()
    frame()
    assert(second_focused, "Tab should move focus to the second input")

    helpers.finish()
end)

test("input.tab_skips_mouse_only", function()
    helpers.begin {
        width = 400,
        height = 260,
        dpi = 1.0,
    }

    local function frame()
        helpers.frame(function()
            flux.set_cursor_pos(20, 20)
            flux.invisible_button("一", 120, 30, { id = "mouse_tab", focusable = false, interactive = true })
            flux.set_cursor_pos(20, 70)
            flux.invisible_button("二", 120, 30, { id = "focus_tab", focusable = true, interactive = true })
        end)
    end

    frame()
    flux.testing.click(60, 35)
    frame()
    assert(flux.keyboard_focus_id() == 0,
        "clicking a mouse-only item must not move keyboard focus")

    flux.testing.key_press("tab")
    frame()
    frame()
    assert(flux.keyboard_focus_id() == flux.make_id("focus_tab"),
        "Tab should skip the mouse-only item")

    helpers.finish()
end)

test("input.tab_skips_no_tab_stop", function()
    helpers.begin {
        width = 400,
        height = 260,
        dpi = 1.0,
    }

    local function frame()
        helpers.frame(function()
            flux.set_cursor_pos(20, 20)
            flux.invisible_button("一", 120, 30, { id = "skip_stop", focusable = true, interactive = true, tab_stop = false })
            flux.set_cursor_pos(20, 70)
            flux.invisible_button("二", 120, 30, { id = "keep_stop", focusable = true, interactive = true })
        end)
    end

    frame()
    flux.testing.key_press("tab")
    frame()
    frame()
    assert(flux.keyboard_focus_id() == flux.make_id("keep_stop"),
        "Tab should skip focusable widgets with tab_stop=false")

    helpers.finish()
end)

test("input.password_and_number", function()
    helpers.begin {
        width = 400,
        height = 260,
        dpi = 1.0,
    }

    local password = "secret"
    local number = 0
    local submitted = false
    local function frame()
        helpers.frame(function()
            flux.set_cursor_pos(20, 20)
            _, password = flux.input_text("密码", password, { id = "password", w = 220, h = 32, password = true })
            flux.set_cursor_pos(20, 70)
            local changed, value, did_submit =
                flux.input_int("数量", number, { id = "number", w = 120, h = 32, step = 1 })
            if changed then
                number = value
            end
            if did_submit then
                submitted = true
            end
        end)
    end

    frame()
    assert(password == "secret")

    flux.testing.click(60, 86)
    frame()
    flux.testing.key_down("ctrl")
    flux.testing.key_press("a")
    frame()
    flux.testing.key_up("ctrl")
    flux.testing.text("42")
    frame()
    flux.testing.key_press("enter")
    frame()
    assert(number == 42, "number input should parse 42, got " .. tostring(number))
    assert(submitted, "Enter should submit the number input")

    helpers.finish()
end)
