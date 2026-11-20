local flux = require "flux"
local helpers = require "test.helpers"

local function find_text(commands, text)
    for index, command in ipairs(commands) do
        if command.kind == "text" and command.text == text then
            return command, index
        end
    end
end

test("draw_list.button_text", function()
    helpers.begin { width = 300, height = 160, dpi = 1.0 }

    local item
    helpers.frame(function()
        flux.set_cursor_pos(20, 20)
        flux.button("按钮", 120, 32, { id = "draw_button" })
        item = { flux.item_rect() }
    end)

    local commands = flux.testing.draw_commands("content")
    local text = assert(find_text(commands, "按钮"), "button label should be drawn")
    assert(text.x >= item[1] - 0.01 and text.x + text.w <= item[1] + item[3] + 0.01,
        "button text should stay inside the item rect")
    assert(text.y >= item[2] - 0.01 and text.y + text.h <= item[2] + item[4] + 0.01,
        "button text should stay inside the item rect vertically")

    helpers.finish()
end)

test("draw_list.table_clip", function()
    helpers.begin { width = 360, height = 260, dpi = 1.0 }

    helpers.frame(function()
        flux.set_cursor_pos(20, 20)
        assert(flux.begin_table("表格", { "列一", "列二" }, { 80, 80 }, 200, 120))
        for index = 1, 3 do
            flux.table_next_row(index, false)
            flux.table_cell_text("r" .. index .. "a")
            flux.table_cell_text("r" .. index .. "b")
        end
        flux.end_table()
    end)

    local commands = flux.testing.draw_commands("content")
    local depth = 0
    local clip
    local header_index
    local cell
    local cell_depth
    local cell_index
    for index, command in ipairs(commands) do
        if command.kind == "push_clip" then
            depth = depth + 1
            if clip == nil then
                clip = command
            end
        elseif command.kind == "pop_clip" then
            depth = depth - 1
            assert(depth >= 0, "clip stack should not underflow")
        elseif command.kind == "text" then
            if command.text == "列一" then
                header_index = index
            elseif command.text == "r2a" then
                cell = command
                cell_depth = depth
                cell_index = index
            end
        end
    end

    assert(depth == 0, "clip push/pop should be balanced")
    assert(clip ~= nil, "table body should push a clip")
    assert(header_index ~= nil and cell ~= nil, "table header and cell text should be drawn")
    assert(header_index < cell_index, "header should be drawn before cells")
    assert(cell_depth >= 1, "cell text should be inside the table clip")
    assert(cell.x >= clip.x - 0.01 and cell.x + cell.w <= clip.x + clip.w + 0.01,
        "cell should stay inside the table clip horizontally")
    assert(cell.y >= clip.y - 0.01 and cell.y + cell.h <= clip.y + clip.h + 0.01,
        "cell should stay inside the table clip vertically")

    helpers.finish()
end)

test("draw_list.layers", function()
    helpers.begin { width = 320, height = 200, dpi = 1.0 }

    helpers.frame(function()
        flux.popup_open("菜单")
        assert(flux.begin_popup("菜单", 20, 20, 140, 60))
        flux.menu_item("项目", { id = "draw_menu_item", shortcut = "", checked = false })
        flux.end_popup()
    end)

    local popup = flux.testing.draw_commands("popup")
    local content = flux.testing.draw_commands("content")
    assert(find_text(popup, "项目"), "menu text should be drawn in the popup layer")
    assert(not find_text(content, "项目"), "menu text must not leak into the content layer")

    local all = flux.testing.draw_commands()
    assert(type(all.content) == "table", "no-arg export should include content")
    assert(type(all.popup) == "table", "no-arg export should include popup")

    helpers.finish()
end)

test("draw_list.frame_reset", function()
    helpers.begin { width = 200, height = 120, dpi = 1.0 }

    helpers.frame(function()
        flux.text(10, 10, "帧一", 0xFFFFFFFF, 14)
    end)
    assert(find_text(flux.testing.draw_commands("content"), "帧一"),
        "first frame should expose its text command")

    helpers.frame(function() end)
    assert(not find_text(flux.testing.draw_commands("content"), "帧一"),
        "next frame should not expose previous commands")

    helpers.finish()
end)

test("draw_list.focus_and_elevation", function()
    helpers.begin { width = 400, height = 200, dpi = 1.0 }

    local item
    local function draw()
        flux.set_cursor_pos(20, 20)
        flux.button("焦点", 120, 32, { id = "focus_button", primary = true })
        item = { flux.item_rect() }
    end

    helpers.frame(draw)
    flux.testing.key_press("tab")
    helpers.frame(draw)
    helpers.frame(draw)

    local commands = flux.testing.draw_commands("content")
    local rings = 0
    local bands = 0
    for _, command in ipairs(commands) do
        if command.kind == "rect"
            and command.x <= item[1] + 0.01
            and command.y <= item[2] + 0.01
            and command.x + command.w >= item[1] + item[3] - 0.01
            and command.y + command.h >= item[2] + item[4] - 0.01 then
            rings = rings + 1
        end
        if command.kind == "rect_filled"
            and math.abs(command.h - 1.0) < 0.01
            and command.y >= item[2] + item[4] - 3.01
            and command.y <= item[2] + item[4] + 0.01 then
            bands = bands + 1
        end
    end

    assert(rings == 2, "focused button should draw a two-layer focus ring, got " .. rings)
    assert(bands >= 3, "button should draw bottom elevation bands, got " .. bands)

    helpers.finish()
end)

test("draw_list.command_defaults", function()
    helpers.begin { width = 300, height = 200, dpi = 1.0 }

    helpers.frame(function()
        flux.rect_filled(10, 10, 20, 10, 0xFF112233, 3)
        flux.rect(10, 30, 20, 10, 0xFF445566, 2, 2)
        flux.rect_gradient(10, 50, 20, 10, 0xFF010203, 0xFF040506, 1, true)
        flux.text(10, 70, "txt", 0xFFFFFFFF, 16, {
            width = 80,
            height = 20,
            line_height = 18,
            max_lines = 2,
            align = 1,
            valign = 2,
            weight = 600,
            wrap = true,
            ellipsis = true,
        })
        flux.push_clip(0, 0, 100, 100)
        flux.pop_clip()
    end)

    local commands = flux.testing.draw_commands("content")
    assert(#commands == 6, "expected 6 commands, got " .. #commands)
    local filled, outline, gradient, text, clip_push, clip_pop = table.unpack(commands)

    assert(filled.kind == "rect_filled")
    assert(filled.thickness == 1 and filled.size == 14)
    assert(filled.color2 == 0xFFFFFFFF)
    assert(filled.radius == 3 and filled.w == 20 and filled.h == 10)

    assert(outline.kind == "rect")
    assert(outline.thickness == 2 and outline.size == 14)
    assert(outline.color2 == 0xFFFFFFFF)

    assert(gradient.kind == "rect_gradient")
    assert(gradient.gradient_type == 2)
    assert(gradient.color2 == 0xFF040506)

    assert(text.kind == "text" and text.text == "txt")
    assert(text.w == 0 and text.h == 0 and text.radius == 0)
    assert(text.thickness == 1 and text.size == 16)
    assert(text.color2 == 0xFFFFFFFF)
    assert(text.text_width == 80 and text.text_height == 20 and text.line_height == 18)
    assert(text.max_lines == 2 and text.align == 1 and text.valign == 2)
    assert(text.weight == 600 and text.wrap == true and text.ellipsis == true)

    assert(clip_push.kind == "push_clip")
    assert(clip_push.color == 0xFFFFFFFF and clip_push.color2 == 0xFFFFFFFF)
    assert(clip_push.thickness == 1 and clip_push.size == 14)
    assert(clip_push.w == 100 and clip_push.h == 100)

    assert(clip_pop.kind == "pop_clip")
    assert(clip_pop.color == 0xFFFFFFFF and clip_pop.color2 == 0xFFFFFFFF)

    helpers.finish()
end)

test("draw_list.alpha_defaults", function()
    helpers.begin { width = 300, height = 160, dpi = 1.0 }

    helpers.frame(function()
        flux.push_alpha(0.5)
        flux.rect_filled(10, 10, 20, 10, 0xFFFFFFFF, 0)
        flux.push_clip(0, 0, 100, 100)
        flux.pop_clip()
        flux.pop_alpha()
    end)

    local commands = flux.testing.draw_commands("content")
    assert(commands[1].kind == "rect_filled")
    assert(commands[1].color == 0x80FFFFFF, "rect color should be alpha-scaled")
    assert(commands[1].color2 == 0x80FFFFFF, "implicit color2 should share baked alpha")
    assert(commands[2].kind == "push_clip")
    assert(commands[2].color == 0xFFFFFFFF and commands[2].color2 == 0xFFFFFFFF)
    assert(commands[3].kind == "pop_clip")
    assert(commands[3].color == 0xFFFFFFFF and commands[3].color2 == 0xFFFFFFFF)

    helpers.finish()
end)

test("draw_list.text_arena", function()
    helpers.begin { width = 300, height = 200, dpi = 1.0 }

    local long_text = string.rep("长文本-", 20)
    local labels = {}
    for index = 1, 80 do
        labels[index] = "复制文本 " .. index
    end

    helpers.frame(function()
        flux.text(10, 10, "short", 0xFFFFFFFF, 14)
        flux.text(10, 30, long_text, 0xFFFFFFFF, 14)
        flux.text(10, 50, "", 0xFFFFFFFF, 14)
        for index, label in ipairs(labels) do
            flux.text(10, 70 + index * 2, label, 0xFFFFFFFF, 12)
        end
        flux.layer.use("popup")
        flux.text(10, 10, "popup-text", 0xFFFFFFFF, 14)
        flux.layer.reset()
    end)

    local content = flux.testing.draw_commands("content")
    local popup = flux.testing.draw_commands("popup")
    local long_cmd = assert(find_text(content, long_text), "long text should survive arena growth")
    assert(#long_cmd.text == #long_text)
    assert(find_text(content, ""), "empty text should still emit a command")
    assert(find_text(content, "short"), "short text should be preserved")
    assert(find_text(popup, "popup-text"), "popup text should stay in popup layer")
    assert(not find_text(content, "popup-text"), "popup text must not leak into content")

    local matched = 0
    for _, command in ipairs(content) do
        if command.kind == "text" then
            for _, label in ipairs(labels) do
                if command.text == label then
                    matched = matched + 1
                end
            end
        end
    end
    assert(matched == #labels, "duplicate text commands should remain distinct")

    helpers.frame(function()
        flux.text(10, 10, "next-frame", 0xFFFFFFFF, 14)
    end)
    assert(not find_text(flux.testing.draw_commands("content"), long_text),
        "previous frame text should be reset")

    helpers.finish()
end)

test("frame.id_balance", function()
    helpers.begin { width = 320, height = 200, dpi = 1.0 }
    assert(flux.offscreen.begin_frame())
    flux.clear(0xFF1E1E1E)
    flux.push_id("leak")
    local ok, err = flux.offscreen.end_frame()
    assert(not ok, "an unbalanced ID stack should fail the frame")
    assert(err:find("ID stack"), "error should mention the ID stack: " .. tostring(err))
    assert(flux.testing.id_imbalance(), "ID imbalance should be observable")

    assert(flux.offscreen.begin_frame())
    assert(flux.testing.id_depth() == 0 and not flux.testing.id_imbalance(),
        "begin_frame should recover a leaked ID stack")
    assert(flux.offscreen.end_frame())
    helpers.finish()
end)

test("frame.scope_balance", function()
    helpers.begin { width = 320, height = 200, dpi = 1.0 }
    assert(flux.offscreen.begin_frame())
    flux.clear(0xFF1E1E1E)
    flux.popup_open("leak")
    assert(flux.begin_popup("leak", 10, 10, 100, 80))
    local ok, err = flux.offscreen.end_frame()
    assert(not ok, "an unclosed popup scope should fail the frame")
    assert(err:find("popup scope"), "error should mention the popup scope: " .. tostring(err))

    assert(flux.offscreen.begin_frame())
    local ended, end_err = flux.end_popup()
    assert(not ended and end_err:find("end_popup"),
        "end_popup without begin_popup should report an error")
    local frame_ok, frame_err = flux.offscreen.end_frame()
    assert(not frame_ok and frame_err:find("end_popup"))
    helpers.finish()
end)

test("frame.scope_depths", function()
    helpers.begin { width = 320, height = 200, dpi = 1.0 }

    local function leak_frame(leak, needle)
        assert(flux.offscreen.begin_frame())
        flux.clear(0xFF1E1E1E)
        leak()
        local ok, err = flux.offscreen.end_frame()
        assert(not ok, "leaked scope should fail the frame")
        assert(err:find(needle), "error should mention " .. needle .. ": " .. tostring(err))
    end

    leak_frame(function()
        flux.push_clip(10, 10, 50, 50)
    end, "clip")
    leak_frame(function()
        assert(flux.begin_child("leak_child", 100, 100))
    end, "child")
    leak_frame(function()
        flux.begin_row()
    end, "layout")

    helpers.finish()
end)

test("frame.end_errors", function()
    helpers.begin { width = 320, height = 200, dpi = 1.0 }
    assert(flux.offscreen.begin_frame())
    flux.clear(0xFF1E1E1E)

    local ok, err = flux.end_child()
    assert(not ok and err:find("end_child"))
    ok, err = flux.end_row()
    assert(not ok and err:find("end_row"))
    ok, err = flux.end_column()
    assert(not ok and err:find("end_column"))
    ok, err = flux.end_form_row()
    assert(not ok and err:find("end_form_row"))

    local frame_ok, frame_err = flux.offscreen.end_frame()
    assert(not frame_ok and frame_err:find("end_child"))
    helpers.finish()
end)
