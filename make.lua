local lm = require "luamake"

local flux_links = {
    "d3d11",
    "dxgi",
    "d2d1",
    "dwrite",
    "dwmapi",
    "gdi32",
    "user32",
    "imm32",
    "ole32",
    "shcore",
    "runtimeobject",
    "WindowsApp",
    "CoreMessaging",
    "winmm",
    "dcomp",
}

lm:conf "flux_base" {
    cxx = "c++20",
    rtti = "off",
    includes = "src",
    defines = {
        "_UNICODE",
        "UNICODE",
        "WIN32_LEAN_AND_MEAN",
        "NOMINMAX",
        "_WIN32_WINNT=0x0A00",
        "WINVER=0x0A00",
    },
    windows = {
        links = flux_links,
    },
    msvc = {
        flags = "/utf-8",
    },
}

lm:lua_src "flux_common" {
    confs = { "flux_base" },
    sources = {
        "src/*.cpp",
        "src/ui/*.cpp",
        "src/text/*.cpp",
        "src/platform/*.cpp",
        "src/gfx/*.cpp",
        "src/runtime/*.cpp",
        "src/app/*.cpp",
        "src/lua/*.cpp",
        "src/widgets/*.cpp",
        "!src/gfx/offscreen_renderer.cpp",
        "!src/lua/lua_offscreen.cpp",
        "!src/lua/lua_testing.cpp",
    },
}

lm:lua_src "flux_release_api" {
    confs = { "flux_base" },
    sources = {
        "src/lua/lua_offscreen.cpp",
        "src/lua/lua_testing.cpp",
    },
}

lm:lua_src "flux_test_api" {
    confs = { "flux_base" },
    defines = { "FLUX_ENABLE_TESTING" },
    sources = {
        "src/gfx/offscreen_renderer.cpp",
        "src/lua/lua_offscreen.cpp",
        "src/lua/lua_testing.cpp",
    },
}

lm:lua_dll "flux" {
    confs = { "flux_base" },
    deps = { "flux_common", "flux_release_api" },
}

lm:lua_dll "flux_testing" {
    confs = { "flux_base" },
    basename = "flux",
    bindir = "build/test/bin",
    export_luaopen = "off",
    ldflags = { "/EXPORT:luaopen_flux" },
    deps = { "flux_common", "flux_test_api" },
    windows = {
        links = { "windowscodecs" },
    },
}

lm:default { "flux" }
