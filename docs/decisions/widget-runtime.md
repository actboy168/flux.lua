# 控件运行时状态（widget_runtime_state）

## 决策

- 保留 `flux::widgets::widget_runtime()` 作为单窗口/单 UI 实例的跨帧状态入口，但把所有**跨帧控件状态**集中到进程级 `widget_runtime_state`，不再在 `basic_controls.cpp`、`text_input.cpp`、`control_anim.cpp` 里各自维护文件静态 map。
- `widget_runtime_state` 拥有：
  - `states`：按 `ui_id` 的类型化通用状态槽（`widget_state_store`），承载 `combo_state`、`edit_state`、`tab_anim`、`progress_anim`、`popup_anim` 等
  - `combo_open_id`
  - `anims` + reduced-motion 缓存（所有控件共享的动画通道）
  - tooltip reveal 状态
  - 绘制期间的 `popup_scopes`、`table_scopes`、`list_scopes` 栈
- 访问统一通过 `flux::widgets::runtime()`；测试通过 `runtime_reset()` 做完整隔离。
- 各模块只保留纯函数/绘制逻辑和必要的局部临时变量；新增控件状态通过 `states.get<T>(id, frame)` 获取，不能新增文件静态 map 或新的 per-type map。
- `runtime()` 每帧首次访问时统一 prune 通用状态槽，并清理已失效的 `combo_open_id`；控件实现不再各自写 prune 循环。
- `flux::app::current()` 的“单例”性质暂时保留：当前只支持单窗口。未来若支持多窗口，再把内部函数签名改为接收 `app_state*`，Lua 侧通过 extraspace/upvalue 传上下文；这是独立的大重构，不在本轮混做。

## Why

- 即时模式控件是自由函数，没有 `this`，跨帧状态必须按 `ui_id` 存；之前分散在多个 `g_*` map，生命周期、prune 和测试隔离都靠约定。
- 早期集中化仍按控件类型各建一个 map，新增控件要同时改 map 成员、`reset()` 和 prune 循环，样板代码随控件数量线性增长。
- 改为一个按 `ui_id` 的类型化状态槽后：
  - 所有权明确（`widget_runtime()` 持有）；
  - 一次 `clear()` / `prune()` 覆盖所有控件状态；
  - 新控件只需定义状态结构和 `kind`，不再新增 map；
  - `context.h` 仍只暴露应用级状态，不把 retained 控件树引入核心。
- 没有引入锁：UI 仍是单线程。

## How to apply

- 新增持久控件状态：
  1. 在 `widget_runtime.h` 定义状态结构，并声明 `static constexpr widget_state_kind kind`；
  2. 在 `widget_state_kind` 增加唯一枚举值；
  3. 控件内通过 `runtime().states.get<T>(id, frame_index())` 获取引用。

  不需要新增 map 成员、reset 行或 prune 循环。
- 小而共享的状态（例如 `control_anim_entry`）仍可保留独立容器；popup/table 的嵌套帧继续用栈。
- 控件实现通过 `flux::widgets::runtime()` 访问；不要恢复 `g_*` 文件静态 map。
- 测试 `flux.testing.reset()` 会调用 `runtime_reset()`；需要单独清理动画时仍可用 `flux.testing.anim_reset()`。
- 若未来支持多窗口：把 `flux::app::current()` 替换为显式 window/context 参数，并把 `widget_runtime_state` 改为每个窗口一份；Lua 绑定用 userdata/registry 绑定上下文。
