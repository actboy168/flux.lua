local flux = require "flux"
local helpers = require "test.helpers"

test("tabs.basic", function()
    helpers.begin {
        width = 400,
        height = 200,
        dpi = 1.0,
    }

    local selected = 1
    local clicked = 0
    local changed = false
    local focused = 0
    local function draw()
        flux.set_cursor_pos(20, 20)
        selected, clicked, changed, focused = flux.tab_bar("主导航", { id = "tabs", "One", "Two", "Three" }, {
            selected = selected,
            width = 330,
            height = 30,
            equal_width = true,
        })
    end

    helpers.frame(draw)
    assert(selected == 1 and clicked == 0)

    flux.testing.click(20 + 107.333 + 4 + 50, 35)
    helpers.frame(draw)
    assert(selected == 2, "clicking the second tab should select it")
    assert(clicked == 2, "clicked index should be 1-based")
    assert(changed, "selection change should be reported")
    assert(focused == 2, "focused index should be 1-based")

    helpers.finish()
end)

test("tabs.golden", function()
    helpers.begin {
        width = 400,
        height = 160,
        dpi = 1.0,
    }

    flux.set_style_color("tab_bg", 0xFFDADADA)
    flux.set_style_color("tab_bg_hover", 0xFFE4E4E4)
    flux.set_style_color("tab_bg_selected", 0xFF333333)
    flux.set_style_color("tab_text", 0xFF333333)
    flux.set_style_color("tab_text_selected", 0xFFDADADA)
    flux.set_style_color("tab_baseline", 0xFF8C8C8C)
    flux.set_style_color("tab_focus", 0xFF7AACC9)

    helpers.frame(function()
        flux.set_cursor_pos(20, 20)
        flux.tab_bar("页签", { id = "golden_tabs", "概览", "详情", "设置" }, {
            selected = 2,
            width = 300,
            height = 28,
        })
    end)
    helpers.check("tabs_folder")

    helpers.finish()
end)

test("tabs.keyboard", function()
    helpers.begin {
        width = 440,
        height = 200,
        dpi = 1.0,
    }

    local items = {
        "A",
        "B",
        { label = "C", disabled = true },
        "D",
    }
    local selected = 1
    local function draw()
        flux.set_cursor_pos(20, 20)
        selected = flux.tab_bar("键", items, { id = "tabs_keys",
            selected = selected,
            width = 360,
            height = 30,
        })
    end

    helpers.frame(draw)
    flux.testing.key_press("tab")
    helpers.frame(draw)
    helpers.frame(draw)

    flux.testing.key_press("right")
    helpers.frame(draw)
    assert(selected == 2, "Right should move to B")

    flux.testing.key_press("right")
    helpers.frame(draw)
    assert(selected == 4, "Right should skip the disabled C")

    flux.testing.key_press("home")
    helpers.frame(draw)
    assert(selected == 1, "Home should select the first tab")

    flux.testing.key_press("end")
    helpers.frame(draw)
    assert(selected == 4, "End should select the last tab")

    flux.testing.key_press("left")
    helpers.frame(draw)
    assert(selected == 2, "Left should move back to B")

    helpers.finish()
end)

test("tabs.roving_tab", function()
    helpers.begin {
        width = 440,
        height = 220,
        dpi = 1.0,
    }

    local selected = 2
    local function draw()
        flux.set_cursor_pos(20, 20)
        flux.button("前", 80, 28, { id = "before_tabs" })
        flux.set_cursor_pos(20, 70)
        selected = flux.tab_bar("漫游", { id = "tabs_roving", "A", "B", "C" }, {
            selected = selected,
            width = 300,
            height = 28,
        })
        flux.set_cursor_pos(20, 130)
        flux.button("后", 80, 28, { id = "after_tabs" })
    end

    helpers.frame(draw)

    flux.testing.key_press("tab")
    helpers.frame(draw)
    helpers.frame(draw)
    assert(flux.make_id("before_tabs") == flux.keyboard_focus_id(),
        "first Tab should focus the leading button")

    flux.testing.key_press("tab")
    helpers.frame(draw)
    helpers.frame(draw)
    assert(flux.keyboard_focus_id() ~= 0
        and flux.keyboard_focus_id() ~= flux.make_id("before_tabs")
        and flux.keyboard_focus_id() ~= flux.make_id("after_tabs"),
        "second Tab should enter the selected tab")

    flux.testing.key_press("tab")
    helpers.frame(draw)
    helpers.frame(draw)
    assert(flux.make_id("after_tabs") == flux.keyboard_focus_id(),
        "third Tab should leave the tab strip")

    helpers.finish()
end)

test("tabs.non_roving", function()
    helpers.begin {
        width = 440,
        height = 220,
        dpi = 1.0,
    }

    local function draw()
        flux.set_cursor_pos(20, 20)
        flux.button("前", 80, 28, { id = "before_nonroving" })
        flux.set_cursor_pos(20, 70)
        flux.tab_bar("非漫游", { id = "tabs_nonroving", "A", "B", "C" }, {
            selected = 1,
            width = 300,
            height = 28,
            roving = false,
        })
        flux.set_cursor_pos(20, 130)
        flux.button("后", 80, 28, { id = "after_nonroving" })
    end

    helpers.frame(draw)
    flux.testing.key_press("tab")
    helpers.frame(draw)
    helpers.frame(draw)
    flux.testing.key_press("tab")
    helpers.frame(draw)
    helpers.frame(draw)
    flux.testing.key_press("tab")
    helpers.frame(draw)
    helpers.frame(draw)

    assert(flux.keyboard_focus_id() ~= 0
        and flux.keyboard_focus_id() ~= flux.make_id("before_nonroving")
        and flux.keyboard_focus_id() ~= flux.make_id("after_nonroving"),
        "roving=false should keep every tab in the Tab order")

    helpers.finish()
end)

test("tabs.items_table", function()
    helpers.begin {
        width = 440,
        height = 200,
        dpi = 1.0,
    }

    local items = {
        { label = "甲", id = "alpha", width = 80 },
        { label = "乙", id = "beta", width = 80, disabled = true },
        { label = "丙", id = "gamma", width = 80 },
    }
    local selected = 0
    local clicked = 0
    local function draw()
        flux.set_cursor_pos(20, 20)
        selected, clicked = flux.tab_bar("表", items, { id = "tabs_items",
            selected = selected,
            height = 28,
        })
    end

    helpers.frame(draw)
    assert(selected == 0, "selected=0 should leave the bar unselected")
    flux.testing.click(20 + 80 + 4 + 40, 35)
    helpers.frame(draw)
    assert(selected == 0, "clicking a disabled tab must not select it")

    flux.testing.click(20 + 40, 35)
    helpers.frame(draw)
    assert(selected == 1 and clicked == 1, "clicking an enabled tab should select it")

    helpers.finish()
end)

test("tabs.clip", function()
    helpers.begin {
        width = 220,
        height = 160,
        dpi = 1.0,
    }

    local selected = 1
    helpers.frame(function()
        flux.set_cursor_pos(20, 20)
        selected = flux.tab_bar("裁剪", { id = "tabs_clip", "甲", "乙", "丙" }, {
            selected = selected,
            width = 80,
            height = 28,
            min_width = 36,
        })
    end)

    helpers.finish()
end)

test("tabs.style", function()
    helpers.begin {
        width = 300,
        height = 140,
        dpi = 1.0,
    }

    flux.set_style_color("tab_bg", 0xFF203040)
    flux.set_style_color("tab_bg_selected", 0xFF102030)
    flux.set_style_color("tab_text", 0xFFFFFFFF)
    flux.set_style_color("tab_text_selected", 0xFFFFFFFF)
    flux.set_style_color("tab_baseline", 0xFFFF00FF)

    helpers.frame(function()
        flux.set_cursor_pos(20, 20)
        flux.tab_bar("样式", { id = "tabs_style", "A", "B" }, {
            selected = 1,
            width = 200,
            height = 28,
            equal_width = true,
        })
    end)

    local first = flux.offscreen.pixel(24, 30)
    local second = flux.offscreen.pixel(20 + 100 + 6 + 4, 30)
    assert(first == 0xFF102030, ("selected tab color mismatch: %08X"):format(first))
    assert(second == 0xFF203040, ("normal tab color mismatch: %08X"):format(second))
    local baseline = flux.offscreen.pixel(150, 47)
    assert(baseline == 0xFFFF00FF, ("baseline color mismatch: %08X"):format(baseline))
    local merged = flux.offscreen.pixel(50, 47)
    assert(merged == 0xFF102030,
        ("selected tab should cover the baseline, got %08X"):format(merged))

    helpers.finish()
end)

test("tabs.anim", function()
    helpers.begin {
        width = 300,
        height = 140,
        dpi = 1.0,
    }

    flux.set_style_color("tab_bg", 0xFF202020)
    flux.set_style_color("tab_bg_hover", 0xFF202020)
    flux.set_style_color("tab_bg_selected", 0xFFE0E0E0)
    flux.set_style_color("tab_text", 0xFF202020)
    flux.set_style_color("tab_text_selected", 0xFFE0E0E0)
    flux.set_style_color("tab_baseline", 0x00000000)

    local selected = 1
    local function draw()
        flux.set_cursor_pos(20, 20)
        selected = flux.tab_bar("动效", { id = "tabs_anim", "A", "B" }, {
            selected = selected,
            width = 200,
            height = 28,
            equal_width = true,
        })
    end

    helpers.frame(draw)
    selected = 2
    helpers.frame(draw)

    local mid = flux.offscreen.pixel(126, 25)
    assert(mid ~= 0xFFE0E0E0 and mid ~= 0xFF202020,
        ("tab selection should animate, got %08X"):format(mid))

    for _ = 1, 80 do
        helpers.frame(draw)
    end
    local settled = flux.offscreen.pixel(126, 25)
    assert(settled == 0xFFE0E0E0,
        ("selected tab should settle on its color, got %08X"):format(settled))

    helpers.finish()
end)

