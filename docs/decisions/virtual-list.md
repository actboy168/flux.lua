# 虚拟化列表与选择模型

## 决策

- 列表使用**固定行高**的虚拟化窗口：`begin_list` 返回 `ok, first, last`，调用方只对 `first..last` 调用 `list_item`，不遍历全部行。
- 使用 `begin_child_fixed(label, w, h, content_height)` 直接提供总内容高度；`begin_child` 在 wheel/scrollbar 之前写入 `child.content_height`，`end_child` 不再从光标位置反推高度。首帧即可正确显示滚动条和响应滚轮。
- 选择状态由 `selection_model` 统一管理（single / multiple / extended），存放在 `list_state`，通过通用 `widget_state_store` 按 `ui_id` 复用。`list_state` 标记为 persistent，不参与 600 帧 prune，切页或暂时不绘制时选择不会丢失；`widget_runtime_reset()` 统一清理。
- `list_set_selected(label_or_id, index)` 返回 `ok, changed`：服务端 current、右键菜单或外部筛选后重排时，可在 `begin_list` 之后、`list_item` 之前把选中项同步到列表；`index=0/nil` 清空。它使用 `selection_model::select_only`，会同步 focused row 和 changed 标记。
- `list_item_ex(index, options)` 支持自绘行：`chrome=false` 跳过内置背景、焦点环和分隔线；`focusable/tab_stop` 控制键盘焦点；额外返回 hovered/active/focused。
- 行 ID 使用内部的 `vl<list_id>_<index>` 稳定键。重排行按 index 重映射选择；需要稳定选择时应保持行 key/index 稳定。
- 行背景、选中和焦点使用 `control_anim` 的 hover/press/focus/value 通道，只对当前可见行更新状态；滚动过的行由 600 帧 TTL 自动清理。

## Why

- 普通 `begin_child` 只能在 `end_child` 根据最终 cursor 推导 `content_height`，迫使列表/表格每帧走完所有行。虚拟化列表天然需要“总高度已知 + 可见窗口”，固定内容高度让首帧 wheel 和滚动条立即工作。
- 把选择放在 C++ 可以统一鼠标、键盘和右键行为，避免每个页面重复 Single/Multiple/Extended 逻辑；persistent 状态避免切页后选择被 TTL 清掉。
- 逐行 `control_anim` 只为当前可见行创建/更新状态；prune 仍会扫描动画 map，但条目数由可见窗口和历史滚动决定，60fps 下 600 帧 TTL 足以覆盖最近滚动过的行，开销可控。

## How to apply

使用列表：

```lua
local ok, first, last = flux.begin_list("项目列表", count, row_height, width, height, { mode = "extended", id = "project_list" })
for index = first, last do
    local clicked, double_clicked, right_clicked, selected =
        flux.list_item(index)
    -- 用 flux.item_rect() 在行矩形内绘制自定义内容
end
flux.end_list()
```

- 新增按 ID 的列表状态：定义状态结构并加 `widget_state_kind`；选择状态复用 `selection_model`，不要新增 per-list map。
- 新增列表相关查询/命令时，统一通过 `list_state`（`widget_runtime().states`）访问；外部在 `begin_list..end_list` 同一 ID 作用域内调用 `list_selected` / `list_scroll_to` 等查询。
- 已知限制：
  - 变量行高、树/Grid、矩形框选尚未实现。
  - 嵌套 child 的 wheel 已按最深 hover child 逐级消费，剩余量交给父级；触摸/触控板惯性仍未做。
  - 焦点行被滚轮完全滚出后可能丢失键盘焦点；键盘导航会先 scroll-into-view。
  - `list_item` 默认使用内置背景、每个可见行进入 Tab 顺序；自绘场景改用 `list_item_ex` 关闭 chrome，并用 `focusable/tab_stop` 决定是否参与键盘导航。列表本身不强制 roving tab index，由调用方为选中行设置 `tab_stop`。
