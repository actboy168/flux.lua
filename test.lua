package.path = "?.lua;?/init.lua;" .. package.path

-- Test code is not part of the default build; ensure the testing variant exists.
local build_result = os.execute("luamake flux_testing")
assert(build_result == true or build_result == 0, "failed to build flux_testing")

package.cpath = "build/test/bin/?.dll;" .. package.cpath

local tests = {}

function test(name, fn)
    tests[#tests + 1] = { name = name, fn = fn }
end

local files = {
    "test/test_offscreen.lua",
    "test/test_text.lua",
    "test/test_widgets.lua",
    "test/test_style.lua",
    "test/test_input.lua",
    "test/test_anim.lua",
    "test/test_scroll.lua",
    "test/test_tabs.lua",
    "test/test_combo.lua",
    "test/test_popup.lua",
    "test/test_table.lua",
    "test/test_draw_list.lua",
    "test/test_virtual_list.lua",
    "test/test_layers.lua",
    "test/test_visual.lua",
    "test/test_dpi.lua",
}

for _, file in ipairs(files) do
    dofile(file)
end

local failed = 0
for _, case in ipairs(tests) do
    local ok, err = xpcall(case.fn, debug.traceback)
    if ok then
        print("ok   " .. case.name)
    else
        failed = failed + 1
        print("FAIL " .. case.name .. "\n" .. tostring(err))
    end
end

print(("SUMMARY pass=%d fail=%d"):format(#tests - failed, failed))
os.exit(failed == 0 and 0 or 1, true)
