# 架构与实现

## 分层

依赖方向：

```text
ui / text  <-  platform  <-  gfx  <-  runtime  <-  widgets  <-  app  <-  lua
```

| 目录 | 命名空间 | 职责 |
| --- | --- | --- |
| `src/ui` | `flux::ui` | 输入、布局、样式、draw list、render layer、焦点、DPI 与动效工具 |
| `src/text` | `flux::text` | 文本选项、UTF-8、数值与字符转换 |
| `src/platform` | `flux::platform` | Win32 / UTF-16 等叶子工具 |
| `src/gfx` | `flux::gfx` | Direct2D、DirectWrite、WUC、离屏渲染与 draw replay |
| `src/runtime` | `flux::runtime` | `core_services`：UI、draw、文本度量、剪贴板和 IME 服务门面 |
| `src/widgets` | `flux::widgets` | 内置控件与跨帧控件状态 |
| `src/app` | `flux::app` | `app_state`、Win32 窗口生命周期/消息循环、输入事件和帧调度 |
| `src/lua` | `flux::lua` | Lua C API 绑定 |

控件层只依赖 `ui/*`、`text/*`、`runtime/*` 和控件自己的头，不直接包含 Windows、IMM、gfx 或应用单例。`gfx`、`platform`、`app` 和 `lua` 是外层；只有这些层可以同时看到所有模块。

所有库自有 C++ 标识符使用 snake_case；完整限定名以 `flux::` 开头。Win32/WinRT/D2D/DWrite 符号和 COM override 方法名保持原样。

## 运行时状态

跨帧控件状态集中在 `flux::widgets::widget_runtime_state`，通过 `flux::widgets::widget_runtime()` 访问：

- per-`ui_id` 状态放在 `widget_runtime().states`（`widget_state_store`）。新增控件只需定义状态结构和 `kind`，不需要再加 per-type map。
- 动画通道 `anims`、popup/table/list 作用域和 tooltip 状态单独保留。
- 控件在首次访问时按帧 prune；`widget_runtime_reset()` 清理全部状态。

布局和绘制函数保持无 retained 控件对象。单窗口上下文由 `flux::app::current()` 承载；多窗口支持尚未实现。

## 绘制与合成

窗口只有一个 WUC `SpriteVisual`。内容通过 DXGI flip-model swap chain 提交：

1. Direct2D 绘制到后缓冲。
2. `draw_list` 的 `content` / `overlay` / `popup` / `tooltip` 四个逻辑层按顺序重放到同一个后缓冲。
3. 只执行一次 `Clear`，然后 `Present` 整帧翻转。

flip 队列保证 DWM 不会采样到只含部分内容的半帧。

`draw_cmd` 是 56 字节的 trivially copyable POD，按 kind 使用 union；文本放在 `draw_list` 的 NUL 结尾 arena 中，命令只保存 offset/length。详见[布局决策](decisions/draw-command-layout.md)。

早期方案使用 `CompositionDrawingSurface` 原地绘制，DWM 可能在两次绘制之间看到半帧，因此改为 swap chain。

`content` 图层可以应用 WUC 变换、blur、阴影和几何裁剪；其他三个图层只是绘制缓冲，不是独立 surface。不要为每个控件创建 retained visual；局部动画应画在 content 缓冲中。

## 帧调度

- `flux.request_frame()` 请求下一帧。
- `flux.begin_frame(force_draw=false)` 根据输入、UI 动画、控件动画和 WUC 动画决定是否开帧；传 `true` 强制开帧。
- `flux.sleep(ms)` 在空闲时让出 CPU。
- `flux.set_vsync(false)` 关闭垂直同步，帧率完全由宿主决定。

框架内部不主动限帧或 `Sleep`。动画活动跟踪覆盖控件补间、popup/tooltip、滚动、caret 和 WUC 动画；只要动画未结束，`begin_frame()` 会继续返回 true。

## 文本

文本层使用 DirectWrite `IDWriteTextLayout`：

- `text` 与 `measure_text` 共享布局缓存；省略 `size` 时使用当前样式字号。
- 字体族默认取系统 UI 消息字体（`lfMessageFont`，回退 `DEFAULT_GUI_FONT`），可通过 `flux.set_font_family` 覆盖。
- 字体变化会清空格式、行高和布局缓存。
- 输入框的 IME 组字窗口复用同一字体和 DPI。

## 自绘控件与 ID

自绘控件通常使用 `invisible_button` 分离鼠标交互与键盘焦点：

```lua
local hovered, active, clicked, double_clicked, focused =
    flux.invisible_button(label, w, h, { focusable = false, interactive = true })
```

- `focusable=false` 不进入 Tab 顺序；
- `interactive=true` 仍接收鼠标事件；
- 需要可聚焦但不占 Tab stop 时传 `tab_stop=false`。

控件的稳定 ID 由 label 生成；需要与显示文本不同的 ID 时使用 `{ id = "..." }`。`flux.make_id(label)` 与控件使用同一套 ID 规则，可用于把自绘控件与 `control_anim`、焦点和测试定位对齐。ID 受 `push_id` 作用域影响；跨作用域引用应保存第一次生成的 `ui_id`。

自绘控件可复用控件动画：

```lua
local hover, press, focus, value =
    flux.control_anim(id, { hovered = hovered, active = active, focused = focused, selected = selected })
local color = flux.mix_color(base, highlight, hover)
```

动画只影响绘制，不改变布局、焦点或点击区域。

## 样式

`style_sheet` 包含颜色槽和尺寸变量。`set_style_color` / `set_style_var` 修改 base style，跨帧持续；`push_style_*` 复制当前 style，并在 `pop_style_*` 后恢复。host 主题通常只需在窗口创建后应用一次；窗口重建或 `reset_ui` 后需要重新应用。

## Lua 绑定

`src/lua` 的注册函数按目标表拆分，一个函数只注册一个表或一组根函数：

- 根表函数：`push_draw_funcs`、`push_ui_funcs`、`push_widget_funcs`、`push_window_funcs`
- 子表：`push_input_table`、`push_layer_table`、`push_anim_table`、`push_offscreen_table`、`push_testing_table`

`registration.h` 的 `register_table` 统一创建子表。`lua_context.h` 提供共享的 `ui()` / `input()` 访问器和鼠标/按键解析；`lua_input.cpp`、`lua_layer.cpp`、`lua_anim.cpp` 各自注册一个子表。

## 测试构建

测试代码由 `FLUX_ENABLE_TESTING` 在编译期开启：

- 发布构建不定义该宏，`lua_offscreen.cpp` 只提供空的 `push_offscreen_funcs`，`offscreen_renderer.cpp` 不参与链接。
- 测试构建定义该宏，输出 `build/test/bin/flux.dll`，包含 `flux.offscreen` 和 `flux.testing`。
- 两个构建共用 `flux_common` 对象；只有测试相关文件需要分别编译。

详见[测试文档](testing.md)。
