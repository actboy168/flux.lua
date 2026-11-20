# 测试代码编译宏

## 决策

- 只保留一个 Lua 模块名 `flux`；测试专用代码通过宏 `FLUX_ENABLE_TESTING` 在测试构建中显式开启，发布构建不编译。
- `src/lua/lua_offscreen.cpp` 的测试代码由 `#ifdef FLUX_ENABLE_TESTING` 保护；未定义宏时只提供空的 `push_offscreen_funcs`。
- 发布构建不定义 `FLUX_ENABLE_TESTING`，输出 `build/bin/flux.dll`，不包含 `flux.offscreen` / `flux.testing`。
- 测试构建定义该宏，额外链接 `offscreen_renderer.cpp`，输出 `build/test/bin/flux.dll`；文件名仍是 `flux.dll`，只是目录不同。默认 `luamake` 只构建发布目标，测试目标通过 `luamake flux_testing` 显式构建。
- `make.lua` 共用 `flux_common` 对象；只有 `lua_offscreen.cpp` 和测试构建的 `offscreen_renderer.cpp` 需要按宏编译不同变体。
- `test.lua` 以及需要测试 API 的工具把 `build/test/bin` 加到 `package.cpath` 前面，再 `require "flux"`。
- 不再有 `flux_test.dll`、`flux_test_entry.cpp`、`lua_offscreen_stub.cpp`。

## Why

- offscreen/WIC/PNG/文件系统和输入注入只服务测试与 golden，不应进入发布模块。
- 单一模块名避免 `package.preload`、`FLUX_MODULE` 和 `luaopen_flux_test` 等额外入口；发布与测试的 Lua 代码完全相同。
- 宏在编译期移除测试绑定，发布 DLL 不新增运行时分支；测试构建则保留完整测试能力。
- 两个构建共用公共对象，只有测试相关文件需要重复编译。

## How to apply

- 新增测试专用 Lua API 时放进 `lua_offscreen.cpp` 的 `testing` 表，并确保未定义 `FLUX_ENABLE_TESTING` 的分支不引用它。
- 新增仅测试使用的 `.cpp` 时加入 `flux_test_api`，并从 `flux_common` 排除；发布目标不要链接。
- 发布使用 `build/bin/flux.dll`；`luamake test` 和测试工具使用 `build/test/bin/flux.dll`。
- 内嵌宿主静态链接 flux 时不定义 `FLUX_ENABLE_TESTING`，并排除 `offscreen_renderer.cpp`。

## 验证

- `luamake` 默认只生成 `build/bin/flux.dll`；`luamake flux_testing` 生成测试变体。
- `luamake test`：自动构建测试变体后运行，85/85，从 `build/test/bin` 加载模块。
- `tools/check_module.lua` 针对发布 DLL，确认 `flux.offscreen` / `flux.testing` 为 `nil`。
- `check_dpi.lua`、`check_animation.lua`、`check_device_loss.lua` 从测试构建加载并验证测试钩子。
