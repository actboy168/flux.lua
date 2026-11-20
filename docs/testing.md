# 测试与基准

## 测试构建

测试代码不在默认构建中。构建测试模块：

```powershell
luamake flux_testing
```

产物为 `build/test/bin/flux.dll`，与发布模块同名但定义 `FLUX_ENABLE_TESTING`，额外包含：

- `flux.offscreen`：离屏渲染、截图、像素读取、golden 比较；
- `flux.testing`：确定性输入注入、动画状态查询、draw list 导出和调试钩子。

发布 `build/bin/flux.dll` 不包含这些 API。

## 离屏渲染

无窗口渲染使用 WIC bitmap + Direct2D WIC render target，和窗口路径共用同一套 draw list replay：

```lua
package.cpath = "build/test/bin/?.dll;" .. package.cpath
local flux = require "flux"

assert(flux.offscreen.create {
    width = 400,
    height = 300,
    dpi = 1.0,
    background = 0xFF1E1E1E,
})

flux.testing.reset()
assert(flux.offscreen.begin_frame())
flux.clear(0xFF1E1E1E)
flux.set_cursor_pos(20, 20)
flux.button("按钮", 120, 36, { id = "button" })
assert(flux.offscreen.end_frame())

assert(flux.offscreen.save("build/out.png"))
local differing, max_diff, total = flux.offscreen.compare("test/golden/widgets_basic.png")
assert(flux.offscreen.hash())
local color = flux.offscreen.pixel(30, 30)
flux.offscreen.destroy()
```

`flux.offscreen.begin_frame(delta)` 可以指定固定时间步长；测试 helpers 默认使用 `1/60s`。`compare` 返回差异像素数、最大通道差和总像素数；allowance 由测试用例决定。

## flux.testing

输入注入：

- `mouse_move`、`mouse_down` / `mouse_up`、`click`、`double_click`
- `wheel`（单位是滚轮格）
- `key_down` / `key_up` / `key_press`
- `text`、`ime` / `ime_commit`
- `reset`：清空控件状态、动画和输入

状态查询：

- `anim(label)` -> `hover, press, focus, value`
- `anim_reset()`
- `combo_reveal(label)` -> 0..1
- `focus_visible()` -> boolean
- `id_depth()` -> number
- `id_imbalance()` -> boolean
- `assert_id_balanced()`：ID 栈不平衡时抛错
- `style_imbalance()` -> boolean
- `debug_set_reduced_motion(enabled)`：强制开关动画
- `draw_commands(layer?)`：只读导出当前帧已有的 draw list，字段包括 `kind/x/y/w/h/color/text/...`；内部命令为 56 字节 POD，文本由 arena 解析，导出的 21 个字段和默认值保持不变

调试钩子：

- `debug_set_dpi_scale(scale)`
- `debug_lose_device()` -> ok [, err]

## 运行测试

```powershell
luamake test
$env:FLUX_UPDATE_GOLDEN='1'; luamake test   # 重新生成 golden
```

`luamake test` 会先构建 `flux_testing`，再从 `build/test/bin` 加载测试模块。测试位于 `test/`，golden 位于 `test/golden/`。

当前覆盖 85 个离屏用例，包括：

- 基础控件、文本、点击、焦点和 Tab 顺序；
- 文本行 metrics、字体族、DPI；
- 输入、IME、撤销/重做、密码；
- 动画、reduced motion、`control_anim`、`mix_color`；
- 滚轮链式消费、平滑滚动、固定内容高度 child；
- combo、popup、tooltip、表格、页签；
- 虚拟化列表、选择模型、绘制层；
- draw list 原始命令导出和帧重置。

## 检查工具

```powershell
luamake lua tools\check_module.lua        # 发布模块 API 冒烟
luamake lua tools\check_text.lua
luamake lua tools\check_scroll.lua
luamake lua tools\check_virtual_list.lua
luamake lua tools\check_dpi.lua
luamake lua tools\check_device_loss.lua
luamake lua tools\check_pacing.lua
luamake lua tools\check_animation.lua
luamake lua tools\check_effects.lua
luamake lua tools\bench_invisible.lua
powershell -ExecutionPolicy Bypass -File tools\check_stability.ps1
```

需要 `flux.testing` 的检查从 `build/test/bin` 加载模块；`check_module.lua` 针对发布模块，确认测试 API 不存在。

## 手工验收

以下项目依赖真实窗口和输入法，无法用离屏测试覆盖：

- 微软拼音、搜狗等输入法的候选窗、组字窗口、跨屏和 100%/150% DPI 定位。
- 窗口失活/恢复后的 caret、选区和 composition 状态。
- 长文本、选区和剪贴板的真实输入法回归。

验收标准：composition 内联显示，候选窗跟随光标，输入法激活时 Ctrl 快捷键不误触。

## 参考基准

本机参考值，不是硬性指标：

- 1000 个 invisible button：约 0.2 ms/帧。
- 1000 行滚动列表：约 0.2 ms/帧（不含绘制和合成）。
- 1000 行虚拟化列表：约 0.06 ms/帧，每帧 realize 约 19 行。

帧节奏由 `Present` 和 vsync 决定；关闭 vsync 后由宿主自行控制。
