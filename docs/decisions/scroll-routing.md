# 滚轮链式消费与平滑滚动

## 背景

页面本身是一个可滚动 child，内部还嵌着列表、combo 下拉等 child。旧实现让每个 child 在绘制时判断指针是否在自身矩形内，并用 `deeper_hovered` 避免父子同帧消费。问题是：

- 判断依赖当前帧尚未完成的布局，嵌套层级和 clip 容易遗漏。
- 内层滚到边缘后，剩余滚轮量直接丢失，页面不会继续滚。
- 滚轮每次都直接跳 48 DIP，没有平滑过渡。

## 决策

### 每帧一次路由

`ui_context` 在每帧第一次 `begin_child_impl` 时调用 `route_wheel()`，同一帧只执行一次：

1. 在上一帧的 child 矩形中找指针下最深的一个（`last_frame + 1 >= frame_index`）。
2. 沿 `child.parent` 向上构建链；每层都验证指针仍在上一帧矩形内。
3. 从最深 child 开始，每层按 `wheel * 48` 尝试滚动，消费量为 `old_target - next_target`。
4. 到达 `[0, max_scroll]` 边界后，剩余量继续交给父级。
5. 未被任何 child 消费的剩余量写回 `input.wheel`，供页面级逻辑使用。

`begin_child_impl` 记录 `child.parent = child_frames_.back().id`，因此父子关系来自实际嵌套，不依赖 label 约定。modal 弹出时背景 child 不参与路由：

- 路由在 `modal && !popup_active` 时直接返回且不标记本帧已路由；等到 `begin_popup` 把 `popup_active` 置真后，弹层内的 child 再触发路由。
- `child.popup` 记录该 child 是否在弹层内；路由只考虑与当前 popup 状态一致的 child。
- 弹层 child 的 `parent` 视为 0，内层滚到边界后不会把剩余量传给被 modal 屏蔽的页面。

### 状态与动画

`child_view_state` 增加：

- `scroll_target`：滚轮目标位置。
- `scroll_anim_from` / `scroll_anim_start` / `scroll_animating`：160ms ease-out。

每帧进入 child 时推进动画：

```
t = (time - start) / 0.16
scroll = from + (target - from) * (1 - (1 - t)^3)
```

- 滚轮路径写 `scroll_target` 并启动动画。
- `set_child_scroll`、`list_scroll_to`、滚动条拖动、翻页写 `scroll` 和 `target` 并取消动画，保持即时定位。
- `end_child` 同时 clamp `scroll` 和 `scroll_target`，内容变短时不会留下越界目标。
- `animations_enabled_ == false`（系统关闭动画或 reduced motion）时滚轮直接落位。

### 滚动条

- 轨道用 `track`，thumb 用 `text_dim`（拖动时 `text`），不再使用低 alpha 的 `border`，避免在浅色主题上几乎不可见。
- `scrollbar_idle_alpha` 控制未 hover 时的可见度（默认 0）；需要让列表滚动条常显时可设为 0.45。
- 虚拟化列表必须用 `begin_child_fixed(label, w, h, content_height)` 提供总内容高度；普通 `begin_child` 的 content height 来自本帧提交内容，虚拟化时会导致 thumb 大小/位置随窗口变化，甚至无法滚到底。

## 限制

- 路由使用上一帧 rect，布局剧烈变化时可能有一帧误差；child 首次出现的帧不参与路由。
- `item_spacing_y()` 与 `style_var item_spacing_y` 仍是两套值；菜单测量使用前者，滚动与控件布局保持一致。
- 触摸/触控板惯性和边缘 chaining 尚未实现，本轮只处理滚轮。

## 验收

- `scroll.wheel`：滚轮后先移动、settle 到 48 DIP，golden 稳定。
- `scroll.nested_wheel`：指针在内层时只有内层滚动。
- `scroll.chain_to_parent`：内层到最大值后父级收到剩余量。
- `scroll.reduced_motion`：关闭动画时直接落到目标值。
- `virtual_list.wheel`：虚拟列表窗口随滚动推进。
- `combo.wheel`：modal 弹层内滚动列表，背景 child 不滚动；弹层 child 的 parent 视为根，不把剩余量传给页面。
