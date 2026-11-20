local flux = require "flux"
local helpers = require "test.helpers"

local LABEL = "项目"

local function draw_list(count, mode, options)
    if type(mode) == "table" then
        options = mode
        mode = options.mode
    end
    options = options or {}
    mode = mode or "single"
    local row_height = options.row_height or 24
    local height = options.height or 200
    local width = options.width or 300
    local _, first, last = flux.begin_list(LABEL, count, row_height, width, height, { mode = mode })
    for index = first, last do
        flux.list_item(index)
    end
    flux.end_list()
    return first, last
end

test("virtual_list.window", function()
    helpers.begin { width = 500, height = 400, dpi = 1.0 }

    local first, last
    helpers.frame(function()
        flux.set_cursor_pos(20, 20)
        first, last = draw_list(1000, { mode = "single" })
    end)

    assert(first == 1, "first visible row should be 1, got " .. tostring(first))
    assert(last >= 8 and last <= 10, "last visible row out of range: " .. tostring(last))
    assert(flux.child_content_height(LABEL) == 24000,
        "fixed content height should be count * row_height")
    helpers.finish()
end)

test("virtual_list.wheel", function()
    helpers.begin { width = 500, height = 400, dpi = 1.0 }

    local first_before
    helpers.frame(function()
        flux.set_cursor_pos(20, 20)
        first_before = select(1, draw_list(1000, { mode = "single" }))
    end)

    flux.testing.mouse_move(100, 100)
    flux.testing.wheel(-1)

    local first_after
    local function draw_track()
        flux.set_cursor_pos(20, 20)
        first_after = select(1, draw_list(1000, { mode = "single" }))
    end
    helpers.settle(draw_track)

    assert(flux.child_scroll(LABEL) > 0, "wheel should scroll the list")
    assert(first_after > first_before, "visible window should shift after scrolling")

    helpers.finish()
end)

test("virtual_list.select_single", function()
    helpers.begin { width = 500, height = 400, dpi = 1.0 }

    local function frame()
        helpers.frame(function()
            flux.set_cursor_pos(20, 20)
            draw_list(1000, { mode = "single" })
        end)
    end

    frame()
    flux.testing.click(50, 20 + 24 + 12)
    frame()
    assert(flux.list_selected(LABEL, 2), "click should select row 2")
    assert(flux.list_selected_count(LABEL) == 1)

    flux.testing.click(50, 20 + 3 * 24 + 12)
    frame()
    assert(not flux.list_selected(LABEL, 2), "single selection should clear old row")
    assert(flux.list_selected(LABEL, 4), "click should select row 4")
    assert(flux.list_selected_count(LABEL) == 1)

    helpers.finish()
end)

test("virtual_list.set_selected", function()
    helpers.begin { width = 500, height = 400, dpi = 1.0 }

    local function frame()
        helpers.frame(function()
            flux.set_cursor_pos(20, 20)
            draw_list(1000, { mode = "single" })
        end)
    end

    frame()
    assert(not flux.list_selected(LABEL, 5), "list should start unselected")
    local ok, changed = flux.list_set_selected(LABEL, 5)
    assert(ok and changed, "setting a new selection should report a change")
    frame()
    assert(flux.list_selected(LABEL, 5), "external selection should be visible")
    assert(flux.list_selected_count(LABEL) == 1)
    assert(flux.list_focused(LABEL) == 5, "external selection should move the focused row")

    ok, changed = flux.list_set_selected(LABEL, 5)
    assert(ok and not changed, "setting the same selection should not report a change")
    ok, changed = flux.list_set_selected(LABEL, 9)
    assert(ok and changed)
    frame()
    assert(not flux.list_selected(LABEL, 5), "single selection should clear the old row")
    assert(flux.list_selected(LABEL, 9))

    ok, changed = flux.list_set_selected(LABEL, 0)
    assert(ok and changed, "index 0 should clear the selection")
    frame()
    assert(flux.list_selected_count(LABEL) == 0, "external clear should remove selection")

    helpers.finish()
end)

test("virtual_list.item_ex", function()
    helpers.begin { width = 500, height = 400, dpi = 1.0 }

    local function frame()
        helpers.frame(function()
            flux.set_cursor_pos(20, 20)
            local _, first, last = flux.begin_list(LABEL, 10, 24, 300, 200, { mode = "single" })
            for index = first, last do
                flux.list_item_ex(index, {
                    focusable = false,
                    chrome = false,
                })
            end
            flux.end_list()
        end)
    end

    frame()
    flux.testing.click(50, 20 + 24 + 12)
    frame()
    assert(flux.list_selected(LABEL, 2), "custom item should still drive list selection")

    helpers.finish()
end)

test("virtual_list.select_extended", function()
    helpers.begin { width = 500, height = 400, dpi = 1.0 }

    local function frame()
        helpers.frame(function()
            flux.set_cursor_pos(20, 20)
            draw_list(1000, { mode = "extended" })
        end)
    end

    frame()
    flux.testing.click(50, 20 + 24 + 12)
    frame()

    flux.testing.key_down("shift")
    flux.testing.click(50, 20 + 4 * 24 + 12)
    frame()
    flux.testing.key_up("shift")
    for index = 2, 5 do
        assert(flux.list_selected(LABEL, index), "shift range should include " .. index)
    end
    assert(flux.list_selected_count(LABEL) == 4)

    flux.testing.key_down("ctrl")
    flux.testing.click(50, 20 + 2 * 24 + 12)
    frame()
    flux.testing.key_up("ctrl")
    assert(not flux.list_selected(LABEL, 3), "ctrl click should toggle row 3 off")
    assert(flux.list_selected_count(LABEL) == 3)

    flux.testing.click(50, 20 + 6 * 24 + 12, "right")
    frame()
    assert(flux.list_selected(LABEL, 7), "right click should select an unselected row")
    assert(flux.list_selected_count(LABEL) == 1)

    flux.testing.key_down("ctrl")
    flux.testing.key_press("a")
    flux.testing.key_up("ctrl")
    frame()
    assert(flux.list_selected_count(LABEL) == 1000, "ctrl+a should select all")
    assert(flux.list_selected(LABEL, 1) and flux.list_selected(LABEL, 1000))

    helpers.finish()
end)

test("virtual_list.select_multiple", function()
    helpers.begin { width = 500, height = 400, dpi = 1.0 }

    local function frame()
        helpers.frame(function()
            flux.set_cursor_pos(20, 20)
            draw_list(1000, "multiple")
        end)
    end

    frame()
    flux.testing.click(50, 20 + 12)
    frame()
    flux.testing.click(50, 20 + 2 * 24 + 12)
    frame()
    assert(flux.list_selected(LABEL, 1) and flux.list_selected(LABEL, 3),
        "multiple mode should keep both plain-clicked rows")
    assert(flux.list_selected_count(LABEL) == 2)

    flux.testing.click(50, 20 + 12)
    frame()
    assert(not flux.list_selected(LABEL, 1), "plain click should toggle off row 1")
    assert(flux.list_selected_count(LABEL) == 1)

    helpers.finish()
end)

test("virtual_list.keyboard", function()
    helpers.begin { width = 500, height = 400, dpi = 1.0 }

    local function frame()
        helpers.frame(function()
            flux.set_cursor_pos(20, 20)
            draw_list(1000, { mode = "single" })
        end)
    end

    frame()
    flux.testing.click(50, 20 + 12)
    frame()
    assert(flux.list_focused(LABEL) == 1)

    flux.testing.key_press("down")
    frame()
    assert(flux.list_focused(LABEL) == 2)
    assert(flux.list_selected(LABEL, 2))

    flux.testing.key_press("end")
    frame()
    assert(flux.list_focused(LABEL) == 1000)
    assert(flux.list_selected(LABEL, 1000))

    helpers.finish()
end)


test("virtual_list.partial_clip", function()
    helpers.begin { width = 500, height = 400, dpi = 1.0 }

    local function frame()
        helpers.frame(function()
            flux.set_cursor_pos(20, 20)
            draw_list(1000, "single", { height = 50, row_height = 24 })
        end)
    end

    flux.set_child_scroll(LABEL, 12)
    frame()

    flux.testing.click(50, 10)
    frame()
    assert(not flux.list_selected(LABEL, 1),
        "click outside the child viewport must not select a partially clipped row")

    flux.testing.click(50, 25)
    frame()
    assert(flux.list_selected(LABEL, 1),
        "click inside the child viewport should select the row")

    helpers.finish()
end)

test("virtual_list.count_change", function()
    helpers.begin { width = 500, height = 400, dpi = 1.0 }

    local function frame(count)
        helpers.frame(function()
            flux.set_cursor_pos(20, 20)
            draw_list(count, { mode = "extended" })
        end)
    end

    frame(1000)
    flux.list_select_all(LABEL)
    frame(1000)
    assert(flux.list_selected_count(LABEL) == 1000)

    frame(10)
    assert(flux.list_selected_count(LABEL) <= 10,
        "shrinking the list should truncate selection")
    assert(not flux.list_selected(LABEL, 11))

    frame(0)
    assert(flux.child_content_height(LABEL) == 0)

    helpers.finish()
end)

test("virtual_list.basic_golden", function()
    helpers.begin { width = 400, height = 300, dpi = 1.0 }
    flux.testing.debug_set_reduced_motion(true)

    local function frame()
        helpers.frame(function()
            flux.set_cursor_pos(20, 20)
            if flux.begin_list(LABEL, 5, 28, 240, 180, { mode = "single" }) then
                local first, last = 1, 5
                for index = first, last do
                    flux.list_item(index)
                    local x, y, w, h = flux.item_rect()
                    flux.text(x + 8, y + 6, ("项目 %d"):format(index),
                        flux.style_color("text"), 14)
                end
                flux.end_list()
            end
        end)
    end

    frame()
    helpers.check("virtual_list_basic")

    helpers.finish()
end)


test("virtual_list.stable_id", function()
    helpers.begin {
        width = 400,
        height = 300,
        dpi = 1.0,
    }

    local function frame()
        helpers.frame(function()
            flux.set_cursor_pos(20, 20)
            local ok, first, last = flux.begin_list(
                "显示名", 100, 24, 300, 200, { id = "vl_stable", mode = "single" })
            assert(ok)
            for index = first, last do
                flux.list_item(index)
            end
            flux.end_list()
        end)
    end

    frame()
    assert(flux.list_set_selected({ id = "vl_stable" }, 3))
    local key = flux.make_id("vl_stable")
    assert(flux.list_selected(key, 3), "list queries should accept a stable integer ID")
    assert(not flux.list_selected("显示名", 3),
        "label lookup must not alias a list created with a custom id")

    frame()
    assert(flux.list_focused({ id = "vl_stable" }) == 3)
    assert(flux.list_scroll_to({ id = "vl_stable" }, 50, "center"))
    frame()
    assert(flux.child_scroll({ id = "vl_stable" }) > 0,
        "list child should use the same custom id")

    helpers.finish()
end)
