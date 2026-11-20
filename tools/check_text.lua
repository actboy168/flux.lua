local flux = require "flux"

assert(flux.create {
    title = "flux-text-check",
    width = 600,
    height = 420,
    backdrop = "none",
})
flux.show()

local text = "这是一段用于测试自动换行和省略号的中文长文本，mixed with English words and numbers 1234567890。"

local single_width, single_height = flux.measure_text(text, 14)
local wrap_width, wrap_height = flux.measure_text(text, 14, { width = 260, wrap = true })
local ellipsis_width, ellipsis_height = flux.measure_text(text, 14, { width = 260, ellipsis = true })
local limited_width, limited_height, limited_lines, limited_overflow = flux.measure_text(text, 14,
    { width = 260, wrap = true, max_lines = 2, ellipsis = true })

print(("single=%.1fx%.1f wrap=%.1fx%.1f ellipsis=%.1fx%.1f max2=%.1fx%.1f lines=%d overflow=%d"):format(
    single_width, single_height, wrap_width, wrap_height,
    ellipsis_width, ellipsis_height, limited_width, limited_height,
    limited_lines, limited_overflow))

assert(single_width > 260.0, "single-line width should exceed the wrap column")
assert(wrap_height > single_height * 2.0, "wrapped text should use multiple lines")
assert(ellipsis_height < wrap_height, "ellipsis text should stay on fewer lines")
assert(limited_height <= single_height * 2.5, "max_lines should limit height")
assert(limited_lines > 2 and limited_overflow > 0, "max_lines should report overflow")

for _ = 1, 10 do
    flux.pump()
    flux.begin_frame(true)
    flux.clear(0xFF1E1E1E)
    flux.text(20, 20, text, 0xFFF2F2F2, 14, { width = 260, ellipsis = true })
    flux.text(20, 60, text, 0xFFF2F2F2, 14, { width = 260, wrap = true })
    flux.text(20, 180, text, 0xFFF2F2F2, 14,
        { width = 260, wrap = true, max_lines = 2, ellipsis = true })
    flux.end_frame()
end

print("text layout ok")
flux.shutdown()
