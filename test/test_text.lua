local flux = require "flux"
local helpers = require "test.helpers"

local text = "这是一段用于测试自动换行和省略号的中文长文本，mixed with English words and numbers 1234567890。"

test("text.layout", function()
    helpers.begin {
        width = 520,
        height = 260,
        dpi = 1.0,
    }
    helpers.frame(function()
        flux.text(20, 20, text, 0xFFF2F2F2, 14, { width = 220, ellipsis = true })
        flux.text(20, 70, text, 0xFFF2F2F2, 14, { width = 220, wrap = true })
        flux.text(20, 150, text, 0xFFF2F2F2, 14,
            { width = 220, wrap = true, max_lines = 2, ellipsis = true })
        flux.text(280, 20, "右对齐", 0xFFF2F2F2, 14, { width = 220, align = "right" })
        flux.text(280, 60, "居中", 0xFFF2F2F2, 18, { width = 220, align = "center" })
        flux.text(280, 100, "Bold 粗体", 0xFFF2F2F2, 16, { weight = "bold" })
    end)
    helpers.check("text_layout")
    helpers.finish()
end)

test("text.line_metrics", function()
    local lines = "a\nb\nc\nd\ne"
    local _, _, count, overflow = flux.measure_text(lines, 14, {
        width = 200,
        wrap = true,
    })
    assert(count == 5, "expected 5 total lines, got " .. tostring(count))
    assert(overflow == 0, "unlimited text should not overflow, got " .. tostring(overflow))

    local _, _, unlimited, clipped = flux.measure_text(lines, 14, {
        width = 200,
        wrap = true,
        max_lines = 2,
    })
    assert(unlimited == 5, "total line count should survive max_lines, got " .. tostring(unlimited))
    assert(clipped == 0, "max_lines without ellipsis does not trim, got " .. tostring(clipped))

    local _, height, limited, overflow = flux.measure_text(lines, 14, {
        width = 200,
        wrap = true,
        max_lines = 2,
        ellipsis = true,
    })
    assert(limited == 5, "total line count should survive max_lines, got " .. tostring(limited))
    assert(overflow == 3, "expected 3 clipped lines, got " .. tostring(overflow))
    assert(height > 0, "visible height should be positive")
end)

test("text.font_family", function()
    local default = flux.font_family()
    assert(default ~= "", "system UI font should resolve")

    assert(not flux.set_font_family("__flux_missing_font__"),
        "unknown font should be rejected")
    assert(flux.font_family() == default,
        "unknown font should fall back to the system UI font")

    assert(flux.set_font_family(default), "system font should resolve")
    assert(flux.font_family() == default, "font family should stay stable")
end)
