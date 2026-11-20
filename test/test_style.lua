local flux = require "flux"
local helpers = require "test.helpers"

test("style.persistent_override", function()
    helpers.begin {
        width = 320,
        height = 220,
        dpi = 1.0,
    }

    flux.set_style_color("popup_bg", 0xFF112233)
    flux.set_style_var("menu_item_height", 33)
    assert(flux.style_color("popup_bg") == 0xFF112233,
        "set_style_color should change the base style")
    assert(flux.style_var("menu_item_height") == 33,
        "set_style_var should change the base style")

    helpers.frame(function()
        flux.set_cursor_pos(20, 20)
        flux.button("按钮", 100, 30, { id = "style_button" })
    end)

    assert(flux.style_color("popup_bg") == 0xFF112233,
        "base style overrides should survive a frame")
    assert(flux.style_var("menu_item_height") == 33)

    flux.push_style_color("popup_bg", 0xFF445566)
    flux.push_style_var("menu_item_height", 21)
    assert(flux.style_color("popup_bg") == 0xFF445566)
    assert(flux.style_var("menu_item_height") == 21)
    flux.pop_style_var()
    flux.pop_style_color()
    assert(flux.style_color("popup_bg") == 0xFF112233,
        "popping an override should restore the base style")
    assert(flux.style_var("menu_item_height") == 33)

    flux.push_style_color("popup_bg", 0xFF778899)
    flux.push_style_var("menu_item_height", 19)
    assert(flux.pop_style_color(), "color and var stacks should pop independently")
    assert(flux.style_color("popup_bg") == 0xFF112233)
    assert(flux.style_var("menu_item_height") == 19,
        "popping a color must leave the var override in place")
    assert(flux.pop_style_var())
    assert(flux.style_var("menu_item_height") == 33)

    helpers.finish()
end)

test("style.scoped_spacing", function()
    helpers.begin {
        width = 320,
        height = 220,
        dpi = 1.0,
    }

    local y1, y2, y3
    helpers.frame(function()
        flux.set_item_spacing(10, 5)
        flux.set_cursor_pos(0, 0)
        flux.dummy(10, 10)
        local _, y = flux.cursor_pos()
        y1 = y

        flux.push_style_var("item_spacing_y", 2)
        flux.set_cursor_pos(0, 0)
        flux.dummy(10, 10)
        _, y = flux.cursor_pos()
        y2 = y
        flux.pop_style_var()

        flux.set_cursor_pos(0, 0)
        flux.dummy(10, 10)
        _, y = flux.cursor_pos()
        y3 = y
    end)

    assert(y1 == 15, "set_item_spacing should drive layout spacing")
    assert(y2 == 12, "push_style_var should scope item spacing")
    assert(y3 == 15, "pop_style_var should restore item spacing")
    helpers.finish()
end)

test("style.menu_measure", function()
    helpers.begin {
        width = 360,
        height = 260,
        dpi = 1.0,
    }

    flux.set_style_var("popup_padding_y", 7)
    flux.set_style_var("menu_item_height", 20)
    flux.set_style_var("menu_separator_gap", 3)
    flux.set_style_var("menu_separator_height", 5)
    flux.set_style_var("item_spacing_y", 4)

    local measured
    local last_bottom
    helpers.frame(function()
        measured = flux.menu_measure {
            items = 3,
            separators = 1,
            text_rows = 1,
            text_row_height = 18,
            extra_height = 2,
        }
        -- padding 7*2 + text 18 + items 60 + separator 5
        -- + four gaps (item spacing 4) 16 + extra 2
        assert(measured == 115, "unexpected menu height " .. tostring(measured))

        local menu_height = flux.menu_measure { items = 3, separators = 1 }
        assert(menu_height == 91, "unexpected plain menu height " .. tostring(menu_height))
        flux.popup_open("菜单", { id = "measure_popup" })
        assert(flux.begin_popup("菜单", 20, 20, 180, menu_height, { id = "measure_popup" }))
        flux.menu_item("一", { id = "measure_1", shortcut = "", checked = false })
        flux.menu_item("二", { id = "measure_2", shortcut = "", checked = false })
        flux.menu_separator()
        flux.menu_item("三", { id = "measure_3", shortcut = "", checked = false })
        local _, y, _, h = flux.item_rect()
        last_bottom = y + h
        flux.end_popup()
    end)

    local menu_height = 91
    local bottom = 20 + menu_height - 7
    assert(math.abs(last_bottom - bottom) < 0.01,
        ("last menu item should end at the popup padding, got %.2f expected %.2f")
            :format(last_bottom, bottom))

    helpers.finish()
end)
