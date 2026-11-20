local flux = require "flux"
local helpers = require "test.helpers"

test("popup.menu", function()
    helpers.begin {
        width = 400,
        height = 300,
        dpi = 1.0,
    }

    local item = 0
    helpers.frame(function()
        flux.set_modal(true)
        flux.set_cursor_pos(20, 20)
        local content_clicked = flux.button("内容", 100, 30, { id = "content" })
        assert(not content_clicked, "modal should block content widgets")

        flux.popup_open("菜单", { id = "popup" })
        assert(flux.begin_popup("菜单", 40, 80, 160, 90, { id = "popup" }))
        if flux.menu_item("项目 A", { id = "a", shortcut = "", checked = false }) then
            item = 1
        end
        if flux.menu_item("项目 B", { id = "b", shortcut = "", checked = true }) then
            item = 2
        end
        flux.end_popup()
    end)
    assert(item == 0, "menu should not activate without input")

    flux.testing.click(80, 94)
    helpers.frame(function()
        flux.set_modal(true)
        flux.set_cursor_pos(20, 20)
        flux.button("内容", 100, 30, { id = "content" })
        flux.popup_open("菜单", { id = "popup" })
        assert(flux.begin_popup("菜单", 40, 80, 160, 90, { id = "popup" }))
        if flux.menu_item("项目 A", { id = "a", shortcut = "", checked = false }) then
            item = 1
        end
        if flux.menu_item("项目 B", { id = "b", shortcut = "", checked = true }) then
            item = 2
        end
        flux.end_popup()
    end)
    assert(item == 1, "clicking the first menu item should select it")

    local button_clicked = false
    helpers.frame(function()
        flux.set_modal(false)
        flux.set_cursor_pos(20, 200)
        button_clicked = flux.button("普通", 100, 30, { id = "button" })
    end)
    flux.testing.click(50, 215)
    helpers.frame(function()
        flux.set_modal(false)
        flux.set_cursor_pos(20, 200)
        button_clicked = flux.button("普通", 100, 30, { id = "button" })
    end)
    assert(button_clicked, "content should work again after modal is disabled")

    helpers.frame(function()
        flux.clear(0xFF000000)
        flux.tooltip("提示", 20, 20, 0)
    end)
    local tooltip_color = flux.offscreen.pixel(24, 24)
    assert(tooltip_color ~= nil and (tooltip_color & 0xFFFFFF) ~= 0,
        "tooltip should be drawn on its layer")

    helpers.finish()
end)

test("popup.tooltip_delay", function()
    helpers.begin {
        width = 200,
        height = 150,
        dpi = 1.0,
    }

    local function frame()
        helpers.frame(function()
            flux.clear(0xFF000000)
            flux.tooltip("延迟提示", 20, 20, 0.35)
        end)
    end

    frame()
    local before = flux.offscreen.pixel(24, 24)
    assert((before & 0xFFFFFF) == 0, "tooltip should be hidden during the delay")

    for _ = 1, 30 do
        frame()
    end
    local after = flux.offscreen.pixel(24, 24)
    assert((after & 0xFFFFFF) ~= 0, "tooltip should appear after the delay")

    helpers.finish()
end)

test("popup.rect_tooltip_hover", function()
    helpers.begin {
        width = 200,
        height = 150,
        dpi = 1.0,
    }

    local function frame(options)
        helpers.frame(function()
            flux.clear(0xFF000000)
            flux.rect_tooltip("提示", 40, 40, 30, 30, options)
        end)
    end

    flux.testing.mouse_move(50, 50)
    frame { delay = 0 }
    local inside = flux.offscreen.pixel(70, 64)
    assert((inside & 0xFFFFFF) ~= 0, "hovered rect should draw its tooltip")

    flux.testing.mouse_move(120, 100)
    frame { delay = 0 }
    local outside = flux.offscreen.pixel(70, 64)
    assert((outside & 0xFFFFFF) == 0, "tooltip should not draw outside the rect")

    flux.testing.mouse_move(50, 50)
    frame { delay = 0, follow_mouse = false }
    local anchored = flux.offscreen.pixel(44, 84)
    assert((anchored & 0xFFFFFF) ~= 0,
        "follow_mouse=false should place the tooltip below the rect")

    helpers.finish()
end)

test("popup.rect_tooltip_flip", function()
    helpers.begin {
        width = 200,
        height = 150,
        dpi = 1.0,
    }

    flux.testing.mouse_move(190, 140)
    helpers.frame(function()
        flux.clear(0xFF000000)
        flux.rect_tooltip("提示", 180, 130, 20, 20, { delay = 0 })
    end)
    local flipped = flux.offscreen.pixel(136, 107)
    assert((flipped & 0xFFFFFF) ~= 0,
        "tooltip should flip left and up near the viewport edges")

    helpers.finish()
end)

test("popup.tooltip_modal_and_order", function()
    helpers.begin {
        width = 200,
        height = 150,
        dpi = 1.0,
    }

    flux.testing.mouse_move(50, 50)
    helpers.frame(function()
        flux.clear(0xFF000000)
        flux.set_modal(true)
        flux.rect_tooltip("提示", 40, 40, 30, 30, { delay = 0 })
    end)
    local blocked = flux.offscreen.pixel(70, 64)
    assert((blocked & 0xFFFFFF) == 0, "modal content should suppress tooltips")

    helpers.frame(function()
        flux.clear(0xFF000000)
        flux.rect_tooltip("第一", 40, 40, 30, 30, { delay = 0 })
        flux.rect_tooltip("第二", 40, 40, 30, 30, {
            delay = 0,
            offset_x = 100,
        })
    end)
    local first = flux.offscreen.pixel(54, 64)
    local last = flux.offscreen.pixel(154, 64)
    assert((first & 0xFFFFFF) == 0, "only the last tooltip request should draw")
    assert((last & 0xFFFFFF) ~= 0, "the last tooltip request should draw")

    helpers.finish()
end)

test("popup.place", function()
    local x, y, w, h = flux.popup_place(
        { x = 20, y = 20, w = 40, h = 20 }, 80, 60,
        { viewport_w = 200, viewport_h = 150 })
    assert(x == 20 and y == 40 and w == 80 and h == 60,
        ("below placement wrong: %s,%s %sx%s"):format(x, y, w, h))

    x, y, w, h = flux.popup_place(
        { x = 160, y = 20, w = 30, h = 20 }, 80, 60,
        { viewport_w = 200, viewport_h = 150 })
    assert(x == 110 and y == 40, "right edge should flip to the anchor right")

    x, y, w, h = flux.popup_place(
        { x = 20, y = 110, w = 40, h = 20 }, 80, 60,
        { viewport_w = 200, viewport_h = 150 })
    assert(x == 20 and y == 50, "bottom edge should flip above the anchor")

    x, y, w, h = flux.popup_place(
        { x = 20, y = 20, w = 40, h = 20 }, 300, 300,
        { viewport_w = 200, viewport_h = 150 })
    assert(x == 4 and y == 4 and w == 192 and h == 142,
        ("oversize placement should clamp: %s,%s %sx%s"):format(x, y, w, h))

    x, y = flux.popup_place(
        { x = 20, y = 20, w = 40, h = 20 }, 80, 60,
        { gap = 6, viewport_w = 200, viewport_h = 150 })
    assert(y == 46, "gap should apply below the anchor")
end)


test("popup.modal_scope", function()
    helpers.begin {
        width = 240,
        height = 200,
        dpi = 1.0,
    }

    local background_clicked = false
    local item_clicked = false

    local function draw()
        flux.clear(0xFF000000)
        flux.set_cursor_pos(10, 10)
        if flux.button("背景", 100, 30, { id = "modal_background" }) then
            background_clicked = true
        end
        flux.popup_open("模态", { modal = true })
        if flux.begin_popup("模态", 40, 80, 120, 100, { modal = true }) then
            if flux.menu_item("项目", { id = "modal_item", shortcut = "", checked = false, checkable = false }) then
                item_clicked = true
            end
            flux.end_popup()
        end
    end

    helpers.frame(draw)
    flux.testing.click(30, 20)
    helpers.frame(draw)
    assert(not background_clicked, "modal popup should block background widgets")

    flux.testing.mouse_move(80, 100)
    flux.testing.mouse_down("left")
    helpers.frame(draw)
    flux.testing.mouse_up("left")
    helpers.frame(draw)
    assert(item_clicked, "modal popup item should accept a cross-frame click")

    helpers.finish()
end)

test("popup.open_stack", function()
    helpers.begin {
        width = 240,
        height = 200,
        dpi = 1.0,
    }

    flux.popup_open("menu", { modal = true })
    assert(flux.popup_is_open("menu"), "explicit popup_open should mark the popup open")
    assert(not flux.popup_is_open("other"), "unrelated popup ids should not be open")

    helpers.frame(function()
        if flux.begin_popup("menu", 20, 20, 100, 80) then
            flux.menu_item("项目", { id = "open_item", shortcut = "", checked = false, checkable = false })
            flux.end_popup()
        end
    end)
    assert(flux.popup_is_open("menu"), "submitted popup should stay open")

    flux.popup_close("menu")
    assert(not flux.popup_is_open("menu"), "popup_close should remove the open entry")

    helpers.finish()
end)

test("popup.auto_close", function()
    helpers.begin { width = 400, height = 300, dpi = 1.0 }

    local function draw()
        helpers.frame(function()
            if not flux.popup_is_open("auto") then
                flux.popup_open("auto")
            end
            if flux.begin_popup("auto", 40, 40, 140, 80) then
                flux.menu_item("项目", { id = "auto_item", shortcut = "", checked = false })
                flux.end_popup()
            end
        end)
    end

    draw()
    assert(flux.popup_is_open("auto"))
    flux.testing.click(300, 240)
    helpers.frame(function() end)
    assert(not flux.popup_is_open("auto"), "outside click should close the popup")

    draw()
    assert(flux.popup_is_open("auto"))
    flux.testing.key_press("escape")
    helpers.frame(function() end)
    assert(not flux.popup_is_open("auto"), "Escape should close the popup")

    helpers.finish()
end)

test("popup.menu_cascade", function()
    helpers.begin { width = 500, height = 320, dpi = 1.0 }

    local submenu_open = false
    local function draw()
        helpers.frame(function()
            if not flux.popup_is_open("cascade_root") then
                flux.popup_open("cascade_root")
            end
            if flux.begin_popup("cascade_root", 20, 20, 180, 120) then
                flux.menu_item("普通项", { id = "cascade_plain", shortcut = "", checked = false })
                submenu_open = flux.begin_menu("更多", { w = 140, h = 90 })
                if submenu_open then
                    flux.menu_item("子项", { id = "cascade_sub", shortcut = "", checked = false })
                    flux.end_menu()
                end
                flux.end_popup()
            end
        end)
    end

    draw()
    assert(not submenu_open)

    flux.testing.mouse_move(40, 70)
    draw()
    assert(submenu_open, "hovering a menu row should open its submenu")

    flux.testing.click(320, 260)
    helpers.frame(function() end)
    assert(not flux.popup_is_open("cascade_root"),
        "outside click should close the whole menu cascade")

    helpers.finish()
end)
