# 基础控件动画

## 决策

- 基础控件（button/checkbox/selectable/tab/menu item/combo/input/list 行/table 行）的 hover/press/focus 和 checked/selected 变化使用按 `ui_id` 的即时模式 CPU 补间，状态在 `src/widgets/control_anim.cpp` 的 map 中，不建立 per-control WUC visual。列表/表格只为当前可见行更新状态。
- `control_anim_state` 有四个通道：`hover`、`press`、`focus`、`value`。`value` 驱动 checked/selected/tab 选中/列表行选中。自绘控件可通过 Lua `flux.control_anim(label_or_id, { hovered, active, focused, selected })` 复用同一套包络，并用 `flux.mix_color` 混合颜色。
- **首次出现 snap**：hover/press/focus 在首次出现非零目标时直接到目标；`value` 首次出现就 arm，默认 snap 到目标。之后才指数平滑，避免首帧半透明/半选中，保证现有 golden 和静态设计不变。自绘控件可传 `snap=false` 让首次 value 从 0 过渡；不同数据切换时配合稳定 key 可避免选中高亮跳变。
- 控件通道使用 `1 - exp(-dt/tau)`，dt clamp 0.1s；接近 1e-3 时 snap 到精确目标；600 帧未见后 prune。
- 一次性 reveal/progress 使用 `control_anim_step`，参数是**时长**而不是 tau，按 `dt/duration` 线性推进；popup/tooltip/combo 的最终 alpha 再套 ease-out。
- 系统动画关闭（`SPI_GETCLIENTAREAANIMATION`）或 `flux.testing.debug_set_reduced_motion(true)` 时全部 snap；每帧缓存一次 enabled 状态。
- 颜色按 `0xAABBGGRR` 插值，透明色使用预乘 alpha 计算，避免从透明渐变时经过黑色；`mix_color(...,0/1)` 和 `with_alpha(...,1)` 返回精确端点。动画只改颜色、alpha、阴影和焦点环，不改 rect/布局/命中。
- `draw_list` 增加 scoped `alpha`；`begin_popup` 可选 alpha，popup 内所有 draw command 统一乘 alpha。combo popup 只做 alpha 淡入（`motion::fast`，83ms），**不移动几何**；关闭立即消失，逻辑 open/modal 状态不延迟。
- primary button 的 hover 阴影只在 hover/press > 0 时绘制，静止时不产生额外 draw command。
- 输入 caret 使用 0.53s 可见/0.53s 不可见的 blink；聚焦、编辑、点击或移动 caret 时重置为可见；reduced motion 下常亮。`update_ime_window` 每帧调用，不受 blink 影响。
- 滚动条用 `child_view_state::scrollbar_alpha` 淡入/淡出：首次绘制保持 1，之后鼠标在 child 内或拖动时回到 1，离开后约 100ms 过渡到 `scrollbar_idle_alpha`（默认 0；需要常显时可设为 0.45）；thumb 使用 `text_dim`，拖动时 `text`。
- determinate progress bar 用按位置 key 的 `progress_anim` 对填充宽度做 `motion::normal`（167ms）补间；首次出现 snap 到目标，实际值仍使用目标 fraction。
- `composition_renderer::software()` / `flux.remote_session()` / `flux.gfx_limited()` 暴露渲染环境，应用可据此关闭 blur/shadow；demo 在 remote/WARP 下自动禁用 blur。
- 测试钩子：`flux.testing.anim(label)`、`anim_reset()`、`combo_reveal(label)`；`helpers.frame(draw, delta)` 可指定固定步长。

## Why

- 控件没有 retained visual；WUC 只能整窗/页面动画。逐控件补间必须发生在 draw list 上，否则会引入 per-control surface/visual，违反架构约束和抖动规避决策。
- 首帧 snap 解决两个冲突：既有 golden 依赖静止/选中态精确像素；动画又不能让第一次交互从 0 开始。armed 通道在首次状态变化后正常补间。
- 只动颜色/alpha/阴影可以在不改变命中、布局或 clip 的前提下获得明显的 Fluent 质感；popup 几何不变保证 combo 的 hit rect 始终一致。
- 系统 reduced motion 必须同时作用于 WUC 和 CPU 补间，否则关闭系统动画后控件仍会动。

## How to apply

- 新控件需要 hover/press/focus 动画时，调用 `control_anim(id, targets, frame)`，用 `mix_color`/`scale_alpha`/`with_alpha` 绘制；不要改 `last_item_rect` 或命中矩形。
- 需要 checked/selected 动画时设置 `targets.has_value=true` 和 `targets.value`。
- 静止态必须使用 `mix_color(...,0)` 的精确原色；不要给 idle 添加阴影/ring/额外 alpha。
- 局部 popup 淡入用 `begin_popup(..., { alpha = ... })` + `draw_list.alpha`，不要移动 popup 矩形或延迟逻辑状态。
- 新动画必须有固定 delta 的测试；中间帧不做 golden，只测状态/探针和最终静止帧。
