local flux = require "flux"
local helpers = require "test.helpers"

test("table.basic", function()
    helpers.begin {
        width = 500,
        height = 300,
        dpi = 1.0,
    }

    local selected = 1
    local clicked_row = 0
    helpers.frame(function()
        flux.set_cursor_pos(20, 20)
        assert(flux.begin_table("项目表", { "#", "名称", "大小" }, { 50, 180, 80 }, 320, 200, { id = "table" }))
        for index = 1, 5 do
            local clicked = flux.table_next_row(index, selected == index)
            if clicked then
                clicked_row = index
                selected = index
            end
            flux.table_cell_text(tostring(index))
            flux.table_cell_text("项目 " .. index)
            flux.table_cell_text("12 MB")
        end
        flux.end_table()
    end)
    assert(clicked_row == 0)

    flux.testing.click(30, 100)
    helpers.frame(function()
        flux.set_cursor_pos(20, 20)
        assert(flux.begin_table("项目表", { "#", "名称", "大小" }, { 50, 180, 80 }, 320, 200, { id = "table" }))
        for index = 1, 5 do
            local clicked = flux.table_next_row(index, selected == index)
            if clicked then
                clicked_row = index
                selected = index
            end
            flux.table_cell_text(tostring(index))
            flux.table_cell_text("项目 " .. index)
            flux.table_cell_text("12 MB")
        end
        flux.end_table()
    end)
    assert(clicked_row == 2, "clicking the second row should select it")
    assert(selected == 2)

    helpers.frame(function()
        flux.set_cursor_pos(20, 20)
        assert(flux.begin_table("项目表", { "#", "名称", "大小" }, { 50, 180, 80 }, 320, 200, { id = "table" }))
        for index = 1, 5 do
            flux.table_next_row(index, selected == index)
            flux.table_cell_text(tostring(index))
            flux.table_cell_text("项目 " .. index)
            flux.table_cell_text("12 MB")
        end
        flux.end_table()
    end)

    helpers.finish()
end)
