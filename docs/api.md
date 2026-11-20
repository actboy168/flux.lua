# API 参考

所有坐标使用逻辑像素，颜色格式为 `0xAABBGGRR`。Lua 索引从 1 开始；返回 `0` 表示“无选择”的控件会单独注明。

## 绘制

```lua
flux.clear(color)
flux.rect_filled(x, y, w, h, color [, radius])
flux.rect(x, y, w, h, color [, radius [, thickness]])
flux.rect_gradient(x, y, w, h, from, to [, radius [, horizontal]])
flux.rect_radial(x, y, w, h, inner, outer [, radius])
flux.shadow(x, y, w, h [, radius [, color [, spread [, offset_y]]]])
flux.push_alpha(alpha) / flux.pop_alpha()
flux.push_offset(x, y) / flux.pop_offset()
flux.push_clip(x, y, w, h) / flux.pop_clip()
flux.text(x, y, text, color [, size [, options]]) -> width, height
flux.measure_text(text [, size [, options]]) -> width, height, lines, overflow
```

`push_alpha` / `push_offset` 是可嵌套的绘制作用域，只影响 draw list，不改变布局、命中矩形或点击区域；适合页面转场、列表项淡入和局部偏移。`shadow` 用分层圆角矩形模拟软阴影；`rect_gradient` / `rect_radial` 使用 Direct2D 渐变 brush。

`text` options：

- `width` / `height`：文本盒尺寸。
- `wrap`：自动换行（默认 `false`）。
- `ellipsis`：超出时显示省略号（默认 `false`）。
- `max_lines`：最大行数。
- `align`：`"left"` / `"center"` / `"right"`。
- `valign`：`"top"` / `"middle"` / `"bottom"`。
- `line_height`、`weight`（`"normal"` / `"medium"` / `"semibold"` / `"bold"`）。

省略 `size` 时使用当前 `style.font_size`。字体族默认取系统 UI 消息字体，取不到时使用 `DEFAULT_GUI_FONT`。宿主可用 `flux.set_font_family(name)` 覆盖、`flux.font_family()` 查询；切换字体会清空文本布局缓存。

`measure_text` 返回 DirectWrite 行 metrics：`lines` 是总行数；`overflow` 是在 `ellipsis=true` 且受 `max_lines` / `height` 限制时被裁掉的行数，没有裁剪时为 `0`。

## 控件与布局

```lua
flux.label(text [, color [, size [, width [, wrap [, max_lines]]]]])
flux.button(label, w, h [, options]) -> clicked
flux.checkbox(label, checked [, options]) -> changed, new_checked
flux.selectable(label, selected [, options]) -> clicked
flux.invisible_button(label, w, h [, options])
    -> hovered, active, clicked, double_clicked, focused
flux.control_anim(label_or_id, { hovered=, active=, focused=, selected= })
    -> hover, press, focus, value
flux.mix_color(from, to, t) -> color

flux.input_text(label, value [, options])
    -> changed, value, submitted, deactivated, focused
flux.input_int(label, value [, options])
    -> changed, value, submitted, deactivated, focused
flux.input_float(label, value [, options])
    -> changed, value, submitted, deactivated, focused

flux.combo(label, selected, items [, options])
    -> changed, selected, open
flux.combo_is_open(label [, options]) -> boolean
flux.combo_set_open(label, open [, options])
flux.tab_bar(label, items [, options]) -> selected, clicked, changed, focused
flux.progress_bar(label, fraction, w [, options])
flux.separator(width)
flux.spacing(height)
```

上述控件的可选参数统一放在最后一个 options 表里，支持 `id`、`enabled` 和控件自己的字段（`w/h`、`primary`、`password`、`step`、`indeterminate`、`focusable`、`interactive`、`tab_stop`、`shortcut`、`checked`、`checkable`、`modal`、`alpha`、`header_height`）。不再接受旧的位置参数形式。`checkbox` 的 `checked` 和 `selectable` 的 `selected` 必须是 boolean，传其它类型会抛出 Lua 参数错误。会失败的命令型 API 返回 `ok [, err]`（`list_set_selected` / `list_clear_selection` / `list_select_all` 为 `ok, changed`）；参数类型或枚举值错误仍直接抛出 Lua error。`begin_*` / `end_*` 必须配对；帧结束时检查 ID、style、clip、child、layout 以及 popup / table / list 作用域，未平衡会让 `flux.end_frame()` 返回 `false, err` 并清空残留作用域。需要与显示文本不同的稳定 ID 时传 `{ id = "..." }` 或 `{ id = 123 }`；不传时以 label 作为 ID。控件自动参与 Tab 焦点顺序。

`progress_bar` 的 `label` 只作为稳定 ID，不绘制文本；需要覆盖时传 `{ id = "..." }` 或 `{ id = 123 }`。

`invisible_button` 把鼠标交互和键盘焦点拆开，通过 options 传 `interactive`、`focusable`、`tab_stop`：`interactive` 控制 hover / 点击，`focusable` 控制 Tab 顺序和 Enter/Space，`tab_stop`（默认等于 `focusable`）控制是否进入焦点顺序。自绘行/卡片可用 `{ focusable=false, interactive=true }` 只接收鼠标事件；需要可聚焦但不占 Tab stop 时传 `{ tab_stop=false }`。按钮接受 Enter/Space；checkbox 只接受 Space。焦点环只在键盘或程序化焦点时绘制，鼠标点击不会画外环；指针释放有 0.05 DIP 容差，拖出后取消点击。

自绘控件可用 `flux.control_anim(label_or_id, targets)` 复用内置控件的动画状态：返回 `hover/press/focus/value` 四个 0..1 包络（`selected` 对应 value），再配合 `flux.mix_color(from, to, t)` 混合颜色。`targets.snap=false` 让首次出现的选中状态从 0 开始过渡；列表行建议用稳定 key 作为 value 动画 ID。

`input_text` 是单行编辑框：支持鼠标定位/拖拽选择、双击选词、grapheme 级左右移动、Ctrl+左右按词移动、Home/End、Ctrl+A/C/V/X、Ctrl+Z/Y、密码掩码和 IME composition 内联显示/候选窗定位；Enter 返回 `submitted=true`。Esc 只回退到聚焦时的文本，不关闭窗口；Tab/Shift+Tab 切换焦点。`input_int` / `input_float` 使用同一套编辑行为，并在提交或失焦时解析/格式化数值，Up/Down 按 `step` 步进。调用方在 `changed` 为真时把返回的 `value` 写回状态。

`combo` 使用 1-based 索引，`0` 表示无选择；`items` 可以是字符串或 `{ label = "...", disabled = true }`。支持鼠标选择、Up/Down/Home/End、Enter/Space 确认、Ctrl+Enter 取消选择、Esc 关闭、滚动和禁用项；聚焦时支持 1000ms 内的前缀 typeahead（从当前选中项之后开始、回绕、跳过禁用项，最多 256 字符）。打开时通过 popup open stack 登记为 modal，阻止其他控件接收输入。

状态所有权：`selected` 由调用方持有并每帧传入；`open` / `highlighted` / `search` 由框架内部持有。需要程序化打开或关闭时用 `combo_set_open`，查询用 `combo_is_open`；`options.id` 同时用于组合框和其 popup 的稳定 ID。

`tab_bar` 是内置 folder 风格页签：`items` 为字符串或 `{ label, id, width, disabled }`，options 支持 `selected/width/height/min_width/gap/padding_x/rounding/equal_width/roving/draw_baseline`；返回 `selected, clicked, changed, focused`（1-based，0 表示无）。状态所有权：`selected` 由调用方控制并写回，focus 和动画由框架内部持有。`width=0` 使用自然宽度；`width>0` 且 `equal_width=false` 时自动 shrink-to-fit，超出后裁剪。`roving=true`（默认）时只有选中 tab 进入 Tab 顺序，Left/Right/Home/End 在可见 enabled tab 间移动焦点并选中，Enter/Space 激活。

### 表格

```lua
flux.begin_table(label, headers, widths, width, height [, options]) -> ok [, err]
flux.table_next_row(index, selected) -> clicked, double_clicked, right_clicked
flux.table_cell_text(text [, align])
flux.end_table() -> ok [, err]
```

表格有固定表头、可滚动 body、行选中/双击/右键事件；`table_next_row` 的 `right_clicked` 可用于打开上下文菜单。行选择完全由调用方持有，表格只返回点击结果，不保存选中行。

### 虚拟化列表

```lua
flux.begin_list(label, count, row_height, width, height [, options]) -> ok, first, last
flux.list_item(index) -> clicked, double_clicked, right_clicked, selected, changed
flux.list_item_ex(index, { focusable=, tab_stop=, chrome= })
    -> clicked, double_clicked, right_clicked, selected, changed, hovered, active, focused
flux.end_list() -> ok [, err]

flux.list_selected(label_or_id, index) -> boolean
flux.list_set_selected(label_or_id, index) -> ok, changed
flux.list_selected_count(label_or_id) -> count
flux.list_selection_changed(label_or_id) -> boolean
flux.list_clear_selection(label_or_id) -> ok, changed
flux.list_select_all(label_or_id) -> ok, changed
flux.list_focused(label_or_id) -> index
flux.list_scroll_to(label_or_id, index [, "top" | "center" | "bottom"]) -> ok [, err]
```

`begin_list` 的 options 支持 `mode`（`single` / `multiple` / `extended`）和 `id`。`label_or_id` 可以是 label、整数 ID，或 `{ id = "..." }` / `{ id = 123 }`，与 `begin_list` 的 options 一致。

`begin_list` 返回 `ok, first, last`；`ok=false` 时 `first/last` 为 `0`。调用方只对 `first..last` 调用 `list_item`；options 的 `mode` 支持 `single` / `multiple` / `extended`。选择状态由框架内部持有，并按 `id`（未传时用 label）保存；查询和命令统一使用同一 `label_or_id`。支持鼠标、Ctrl/Shift、方向键/Home/End/PageUp/PageDown、Ctrl+A。`list_item` 负责行背景和选择，调用方用 `flux.item_rect()` 在行矩形内绘制内容。`list_item_ex` 允许自绘行：`chrome=false` 跳过内置背景、焦点环和分隔线，`focusable/tab_stop` 控制键盘焦点，并额外返回 hovered/active/focused。`list_set_selected` 可把服务端 current 或其他外部选择同步进列表（在 `begin_list` 后、`list_item` 前调用）；它和 `list_clear_selection`、`list_select_all` 返回 `ok, changed`，列表不存在时 `ok=false` 并返回错误字符串。`list_scroll_to` 在列表不存在或为空时返回 `false, err`。

虚拟化列表配合 `begin_child_fixed(label, w, h, content_height)` 使用；普通 child 的 content height 仍由本帧提交内容决定。

### 弹出层与 tooltip

```lua
flux.set_modal(open)                 -- 仅当前帧；优先用 begin_popup 的 modal 选项
flux.begin_popup(label, x, y, w, h [, options]) -> ok [, err]
-- options: { id = "...", alpha = -1, modal = true }
flux.popup_open(label [, options])          -- 显式标记打开
flux.popup_close([label_or_id])             -- 关闭当前/指定 popup
flux.popup_is_open([label_or_id]) -> bool
flux.popup_place(anchor, w, h [, opts]) -> x, y, w, h
flux.menu_item(label [, options]) -> clicked
flux.begin_menu(label [, options]) -> ok
-- options: { w = 160, h = 120, enabled = true, id = "..." }
flux.end_menu() -> ok [, err]
flux.menu_separator()
flux.menu_measure { items=n, separators=s, text_rows=r, text_row_height=h, extra_height=e }
    -> height
flux.popup_hovered() -> boolean
flux.end_popup() -> ok [, err]

flux.tooltip(text, x, y [, delay [, max_width]])
flux.rect_tooltip(text, x, y, w, h [, opts])
```

弹出菜单使用 `popup` 绘制缓冲。先用 `popup_open` 标记打开，`begin_popup` 只在 popup 已打开时绘制并返回 `true`；`popup_close` / `popup_is_open` 管理状态。`begin_popup(..., { modal = true })` 会按 popup 作用域屏蔽普通控件，popup 内的菜单项仍可交互。非 modal popup 在点击外部或按 Esc 时自动关闭；modal popup 保持到显式 `popup_close`。`flux.set_modal(true)` 仅阻塞当前帧，仍保留为低层逃生口。

`begin_menu` 在当前 popup 内绘制带箭头的子菜单入口；悬停或点击时打开子 popup，调用方在返回 `true` 时绘制子项并调用 `end_menu`。子菜单按行右侧放置，空间不足时翻到左侧；打开另一个同级子菜单会替换旧子菜单。`popup_place` 是纯放置函数：按锚点优先向下、边缘翻转并 clamp，`opts` 支持 `gap/margin/viewport_w/viewport_h`；它不管理打开/关闭或模态。

`rect_tooltip` 在鼠标位于指定矩形内且未被 modal 屏蔽时排队，帧末自动绘制；默认 0.35s delay，传 0 可立即显示，`max_width=0` 时按视口宽度自动限制，位置跟随鼠标并在边缘翻转/clamp。`tooltip` 是底层固定位置版本，调用方自行决定显示时机。

### 布局工具

```lua
flux.set_cursor_pos(x, y) / flux.cursor_pos() -> x, y
flux.set_item_spacing(x, y)             -- 修改后续布局使用的主轴/纵轴间距
flux.same_line() / flux.dummy(w, h) / flux.indent(w) / flux.unindent(w)
flux.begin_row() / flux.end_row() -> ok [, err]
flux.begin_column() / flux.end_column() -> ok [, err]

flux.begin_form_row(label_width [, gap [, row_height]])
flux.form_label(text [, color [, size]])
flux.form_align([x])
flux.end_form_row() -> ok [, err]

flux.begin_child(label, w, h [, options]) -> ok [, err]
flux.begin_child_fixed(label, w, h, content_height [, options]) -> ok [, err]
flux.end_child() -> ok [, err]
flux.child_scroll(label_or_id) / flux.set_child_scroll(label_or_id, value)
flux.child_content_height(label_or_id) -> height

flux.item_rect() -> x, y, w, h
flux.is_item_hovered() -> boolean
flux.is_item_active() -> boolean
flux.is_item_clicked() -> boolean
flux.is_item_double_clicked() -> boolean
flux.is_item_focused() -> boolean
flux.set_keyboard_focus(label)
flux.item_visible() -> boolean
flux.is_rect_visible(x, y, w, h) -> boolean
flux.time() -> seconds
flux.modal() -> boolean
```

`label_or_id` 可以是 begin 时的 label、整数 ID，或与 `begin_child(..., { id = ... })` 相同的 `{ id = "..." }` / `{ id = 123 }` 引用。`begin_child_fixed` 同样适用。child 和 list 会自动 push/pop 自己的 ID 作用域；其 label/ID 使用绝对哈希，因此可以在作用域外通过同一 label 或 ID 查询滚动和选择状态。帧结束时未配对的 clip / child / layout 会记录错误，并让 `end_frame` 返回 `false, err`。

`set_item_spacing(x, y)` 修改 base style 的 `item_spacing_x/y`，与 `set_style_var("item_spacing_x/y", ...)` 是同一来源，会持续影响后续帧；需要帧内作用域时用 `push_style_var` / `pop_style_var`。`is_item_*` 查询上一个控件的结果；`set_keyboard_focus(label)` 按 label 设置键盘焦点并显示焦点环。

`begin_form_row` 在布局栈上建立一个标签/控件两列表单行；`form_label` 绘制标签并把光标移到控件列，`form_align` 只重置光标位置，`end_form_row` 按行高推进。`menu_measure` 用真实的 popup padding、行高、分隔线和 `item_spacing_y()` 计算菜单高度。

## 控件动画

button / checkbox / selectable / tab / menu item / combo / input / list 行 / table 行使用按 `ui_id` 的即时模式 CPU 补间：hover、press、focus 和 checked/selected 状态变化会平滑过渡。动效时长集中在 `ui/motion.h`（fast/normal/slow），焦点环和外框绘制做 DPI 像素对齐。

- 键盘焦点使用 2px 外环 + 1px 内环的双层 focus ring，按钮底部有轻 elevation band。
- checkbox 的 box 和勾选 glyph 随 hover/press/value 缩放；selectable 按下时轻微收缩；tab 按下时填充和文字下沉。
- primary button 在 hover 时出现轻微底部阴影；list/table 行的选中和 hover 使用 value/hover 通道；menu/tooltip/combo popup 以 alpha 淡入（几何和命中矩形不变）；输入 caret 闪烁。
- 滚动条在鼠标进入/拖动时淡入、离开后淡出；determinate progress bar 的填充宽度平滑过渡。
- 动画只影响绘制，不改变布局、焦点或点击区域；静态控件不做动画。
- 首次出现时直接 snap 到当前状态，之后才补间。
- 系统“显示动画”关闭时全部 snap（`SPI_GETCLIENTAREAANIMATION`）。

## 样式与主题

```lua
flux.push_style_color(name, color) / flux.pop_style_color([n])
flux.push_style_var(name, value) / flux.pop_style_var([n])
flux.style_color(name) / flux.style_var(name)
flux.set_style_color(name, color) / flux.set_style_var(name, value)
flux.style_depth() / flux.assert_style_balanced()
```

颜色槽：`text`、`text_dim`、`panel`、`panel_hover`、`panel_active`、`border`、`accent`、`accent_hover`、`accent_active`、`white`、`disabled`、`track`、`popup_bg/border/shadow`、`menu_text/shortcut/item_hover/separator`、`tooltip_bg/border/shadow`、`tab_bar_bg/border`、`tab_bg`、`tab_bg_hover`、`tab_bg_selected`、`tab_text`、`tab_text_selected`、`tab_baseline`、`tab_focus`。

变量：`item_spacing_x/y`、`frame_padding_x/y`、`rounding`、`border_size`、`scrollbar_size`、`font_size`、`line_height`、`popup_padding_x/y`、`popup_shadow_spread/offset_y`、`menu_item_height`、`menu_item_padding_x`、`menu_separator_gap/height`、`menu_header_line_height`、`combo_item_height`、`combo_popup_padding/max_height/gap/min_width`、`tooltip_padding_x/y`、`tooltip_shadow_spread/offset_y`、`tab_height`、`tab_min_width`、`tab_gap`、`tab_padding_x`、`tab_rounding`、`tab_baseline_size`、`tab_font_size`、`scrollbar_idle_alpha`。

`set_style_color` / `set_style_var` 修改 base style，跨帧持续生效；`push_*` 在当前 style 上叠加一层改动，`pop_*` 移除该层。颜色和变量使用独立栈，可以分别 push/pop，不要求跨类型严格 LIFO；帧结束时任一栈未清空都会让 `end_frame` 报告样式失衡。宿主主题通常只需在窗口创建后应用一次。窗口重建或 `reset_ui` 会重置 base style，宿主应重新应用主题。

## 输入

- `flux.input.mouse_pos()`、`mouse_down/pressed/released/double_clicked(button)`、`mouse_wheel()`、`mouse_inside()`、`mouse_in_rect`。
- `flux.input.key_down/pressed/released(key)`，支持 `"tab"`、`"enter"`、`"escape"`、`"space"`、方向键、`"f1"`…`"f24"`、字母数字或 VK 码。
- `flux.input.text()`（WM_CHAR 累积的 UTF-8）、`delta_time()`、`shift()/ctrl()/alt()`。

滚轮从指针下最深的 child 开始，沿上一帧的 parent 链向上消费：内层到达边界后剩余量继续传给父级，同帧只路由一次。滚轮滚动使用 160ms ease-out 平滑到目标位置；`set_child_scroll`、`list_scroll_to` 和滚动条拖动仍然即时定位，系统关闭动画时滚轮同样直接落位。

## 窗口与帧调度

```lua
flux.create { title=, width=, height=, titlebar_height=, backdrop= } -> ok [, err]
flux.show()
flux.pump() -> running
flux.begin_frame([force_draw]) -> drew
flux.end_frame() -> ok [, err]
flux.request_frame()
flux.sleep([ms])
flux.vsync() / flux.set_vsync(enabled)
flux.size() -> width, height
flux.pixel_size() -> pixel_width, pixel_height
flux.dpi_scale() -> scale
flux.backdrop() / flux.backdrop_native()
flux.software() / flux.remote_session() / flux.gfx_limited()
flux.last_error()
flux.frame_timings() -> render_ms, wait_ms
flux.set_titlebar_client_rects(rects)
flux.close_rect() -> x, y, w, h
flux.shutdown()
```

`backdrop` 支持 `"none"` / `"mica"` / `"acrylic"`。`flux.begin_frame()` 内部判断是否需要绘制（输入、动画或显式请求），传 `true` 可强制开帧。空闲时宿主可调用 `flux.sleep(ms)`。框架不主动限帧或 Sleep；`flux.set_vsync(false)` 关闭垂直同步后完全由宿主决定帧率。

`flux.software()` 表示 WARP/软件渲染；`flux.remote_session()` 表示远端会话；`flux.gfx_limited()` 在任一为真时返回 true，宿主可据此关闭 blur/shadow 等昂贵效果。

`flux.set_titlebar_client_rects(rects)` 指定标题栏内需要接收鼠标的客户端区域，元素为 `{x, y, w, h}`。

## 绘制层与图层动画

整个窗口只有一个 WUC `SpriteVisual`，内容通过 DXGI flip-model swap chain 提交。`draw_list` 提供 `content` / `overlay` / `popup` / `tooltip` 四个逻辑绘制缓冲，按顺序绘制到同一个 D2D 后缓冲，只做一次 `Clear`，然后整帧 `Present`。

```lua
flux.layer.use("overlay")               -- 后续绘制进入该缓冲；无需先关闭 clip
flux.layer.reset()                      -- 回到 content
flux.layer.push("overlay") -> ok [, err] -- 压入并切换到该缓冲
flux.layer.pop() -> ok [, err]           -- 恢复上一层缓冲
flux.layer.active()                     -- 当前绘制缓冲名
flux.layer.ensure("overlay") -> ok [, err]
flux.layer.exists("overlay") -> boolean
flux.layer.list() -> { "content", "overlay", "popup", "tooltip" }
flux.layer.set("content", {
    opacity = 1,
    offset = { 0, 0 },                  -- 逻辑像素
    scale = { 1, 1 },
    rotation = 0,
})
flux.layer.get("content")               -- exists/opacity/offset/pixel_offset/scale/rotation/width/height

local id = flux.layer.animate("content", {
    opacity = { from = 0, to = 1, duration = 0.16, easing = "out-cubic" },
    offset  = { from = { 0, 12 }, to = { 0, 0 }, duration = 0.16, easing = "out-cubic" },
    -- scale / rotation 同样可用；spring = { damping=0.7, period=0.08 } 切换物理动画
    -- keyframes = { { t=0, value=0 }, { t=1, value=1 } }，iterations / delay / direction 可选
})
flux.anim.running(id) -> boolean
flux.anim.stop(id [, apply_final])      -- 默认应用终值
flux.anim.poll() -> { completed_id, ... }
flux.anim.reduced_motion() -> boolean

flux.layer.blur("content", 6)           -- GaussianBlur effect brush；0 关闭
flux.layer.shadow("content", { blur = 18, offset = { 0, 6 }, opacity = 0.55 })
flux.layer.shadow("content", nil)       -- 关闭；不传第二个参数返回是否启用
flux.layer.clip("content", { x = 80, y = 150, w = 240, h = 100, radius = 12 })
flux.layer.clip("content", nil)         -- 关闭；不传第二个参数返回是否启用
```

`set` / `animate` / `blur` / `shadow` / `clip` 只作用于 `content`；对 `overlay`、`popup`、`tooltip` 调用会返回 `false, err`，因为它们只是绘制缓冲，不是独立 surface。局部浮层动画应在 content 缓冲中绘制并用即时模式补间完成。

每个逻辑绘制缓冲维护独立的 draw clip 深度，允许在 clip 打开时切换层，replay 时各层 clip 独立平衡。`layer.push` / `layer.pop` 用于嵌套切换，`pop` 恢复 push 前的缓冲；`layer.use` / `layer.reset` 是绝对切换，不受栈影响。每帧开始时栈清空，未配对的 `pop` 会返回 `false, err`。`begin_popup` 会保存并清空逻辑 clip，`end_popup` 恢复，避免父级裁剪影响 popup。`layer.clip` 是基于 `CompositionGeometricClip` 的窗口级裁剪，和 `push_clip` / `pop_clip` 的 draw list 裁剪不是同一机制。

动画在 WUC 合成线程执行；完成事件经 HWND 消息回到 UI 线程，Lua 每帧调用 `poll()`。WUC 变换不参与命中测试。系统关闭“显示动画”时，动画直接应用终值。Blur 使用自定义 `IGraphicsEffect` + `IGraphicsEffectD2D1Interop` 适配器；阴影使用 `DropShadow` + `InheritFromVisualContent`；裁剪使用 `CompositionGeometricClip`（Windows 10 1809+）。

## 模块初始化

```lua
flux.set_font_family(name) -> ok [, err]
flux.font_family() -> name
flux.make_id(label) -> id
flux.keyboard_focus_id() -> id
```

测试专用 API（离屏渲染和输入注入）只在 `FLUX_ENABLE_TESTING` 构建中提供，见[测试文档](testing.md)。
