local flux = require "flux"
local helpers = require "test.helpers"

local function draw_list()
    flux.set_cursor_pos(20, 20)
    flux.begin_child("list", 320, 220, { id = "scroll" })
    for index = 1, 50 do
        flux.selectable("行 " .. index, index == 1, { id = "row" .. index, w = 300, h = 26 })
    end
    flux.end_child()
end

test("scroll.wheel", function()
    helpers.begin {
        width = 360,
        height = 300,
        dpi = 1.0,
    }
    helpers.frame(draw_list)
    assert(flux.child_scroll("scroll") == 0, "child should start at the top")
    helpers.check("scroll_top")

    flux.testing.mouse_move(100, 100)
    flux.testing.wheel(-1)
    helpers.frame(draw_list)
    assert(flux.child_scroll("scroll") > 0, "wheel should scroll the child")
    assert(flux.child_scroll("scroll") < 200, "one wheel notch should scroll a small amount")
    helpers.settle(draw_list)
    assert(math.abs(flux.child_scroll("scroll") - 48) < 0.01,
        "smooth scroll should settle on the target")
    helpers.check("scroll_moved")
    helpers.finish()
end)

test("scroll.reduced_motion", function()
    helpers.begin {
        width = 360,
        height = 300,
        dpi = 1.0,
    }
    helpers.frame(draw_list)
    flux.testing.debug_set_reduced_motion(true)
    flux.testing.mouse_move(100, 100)
    flux.testing.wheel(-1)
    helpers.frame(draw_list)
    assert(flux.child_scroll("scroll") == 48,
        "reduced motion should snap to the wheel target")
    flux.testing.debug_set_reduced_motion(false)
    helpers.finish()
end)

test("scroll.fixed_content", function()
    helpers.begin {
        width = 220,
        height = 180,
        dpi = 1.0,
    }

    flux.set_style_var("scrollbar_idle_alpha", 1)

    helpers.frame(function()
        flux.set_cursor_pos(20, 20)
        assert(flux.begin_child_fixed("固定", 100, 100, 1000, { id = "fixed_child" }))
        flux.dummy(0, 10)
        flux.end_child()
    end)
    assert(flux.child_content_height("fixed_child") == 1000,
        "fixed child content height should not depend on submitted rows")
    assert(flux.child_scroll("fixed_child") == 0)

    flux.set_child_scroll("fixed_child", 500)
    helpers.frame(function()
        flux.set_cursor_pos(20, 20)
        assert(flux.begin_child_fixed("固定", 100, 100, 1000, { id = "fixed_child" }))
        flux.dummy(0, 10)
        flux.end_child()
    end)
    assert(flux.child_scroll("fixed_child") == 500,
        "fixed child should keep the programmatic scroll")

    assert(flux.child_content_height({ id = "fixed_child" }) == 1000)
    flux.set_child_scroll({ id = "fixed_child" }, 250)
    local child_key = flux.make_id("fixed_child")
    assert(flux.child_scroll(child_key) == 250,
        "child accessors should accept a stable integer ID")
    flux.set_child_scroll(child_key, 500)
    assert(flux.child_scroll({ id = "fixed_child" }) == 500,
        "child accessors should accept the same custom ID table")

    -- content 1000, view 100, scroll 500 => thumb y ~= 62, height 24.
    local thumb = flux.offscreen.pixel(115, 70)
    assert(thumb == 0xFFB4B4B4, ("scrollbar thumb color mismatch: %08X"):format(thumb))

    helpers.finish()
end)

local function draw_nested()
    flux.set_cursor_pos(20, 20)
    flux.begin_child("outer", 300, 200, { id = "nested_outer" })
    flux.dummy(10, 10)
    flux.begin_child("inner", 200, 100, { id = "nested_inner" })
    for index = 1, 20 do
        flux.selectable("内层 " .. index, false, { id = "inner" .. index, w = 180, h = 26 })
    end
    flux.end_child()
    flux.dummy(10, 300)
    flux.end_child()
end

test("scroll.chain_to_parent", function()
    helpers.begin {
        width = 360,
        height = 300,
        dpi = 1.0,
    }

    helpers.frame(draw_nested)
    flux.set_child_scroll("nested_inner", 10000)
    helpers.frame(draw_nested)
    local inner_max = flux.child_scroll("nested_inner")
    assert(inner_max > 0, "inner child should clamp to its maximum scroll")

    flux.testing.mouse_move(80, 80)
    flux.testing.wheel(-1)
    helpers.frame(draw_nested)
    assert(flux.child_scroll("nested_inner") == inner_max,
        "inner child should not consume the wheel at its maximum")
    assert(flux.child_scroll("nested_outer") > 0,
        "remaining wheel delta should chain to the parent child")

    helpers.finish()
end)

test("scroll.nested_wheel", function()
    helpers.begin {
        width = 360,
        height = 300,
        dpi = 1.0,
    }

    helpers.frame(draw_nested)
    flux.testing.mouse_move(80, 80)
    flux.testing.wheel(-1)
    helpers.frame(draw_nested)

    assert(flux.child_scroll("nested_inner") > 0,
        "the innermost hovered child should consume the wheel")
    assert(flux.child_scroll("nested_outer") == 0,
        "the outer child should not scroll when the pointer is over the inner child")

    helpers.finish()
end)
