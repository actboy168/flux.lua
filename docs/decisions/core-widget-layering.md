# 模块分层与命名

## 决策

依赖方向：

```
ui / text <- platform <- gfx <- runtime <- widgets <- app <- lua
```

- **ui** `src/ui`，`namespace flux::ui`：类型、输入、draw list、render layer、`ui_context`、布局/样式/focus、像素/动效工具；不依赖 `gfx`、`platform`、`runtime`、`widgets`、`app`。
- **text** `src/text`，`namespace flux::text`：`text_options`、UTF-8、字符转换；不依赖其他模块。
- **platform** `src/platform`，`namespace flux::platform`：Win32 叶子工具（`os_win32.h`、`utf8_win32.h`），不依赖其他 flux 模块。
- **gfx** `src/gfx`，`namespace flux::gfx`：D2D/DWrite/WUC/offscreen；依赖 `ui` + `text` + `platform`。
- **runtime** `src/runtime`，`namespace flux::runtime`：`core_services` 服务门面（UI/draw/text/clipboard/IME）；实现依赖 `gfx`/`platform`，头文件保持平台中立。
- **widgets** `src/widgets`，`namespace flux::widgets`：即时模式控件；依赖 `ui` + `text` + `runtime`，不直接依赖 `gfx`/`platform`/`app`。
- **app** `src/app`，`namespace flux::app`：`app_state`、Win32 host loop、renderer 编排；可依赖所有下层。
- **lua** `src/lua`，`namespace flux::lua`：绑定层；可依赖所有模块，保持 Lua 模块名 `flux` 和公开 keys 不变。

跨模块引用一律显式限定（如 `flux::ui::ui_id`、`flux::gfx::composition_renderer`），不使用 `using namespace`。

## 入口

- `flux::app::current()`：返回 `app_state`，持有 `core_services services` 和 `composition_renderer renderer`。
- `flux::runtime::services()`：返回 `core_services`。
  - `services().ui`：`ui_context`
  - `services().draw`：`draw_list`
  - `services().measure_text` / `caret_metrics_text` / `hit_test_text`
  - `services().clipboard_text` / `set_clipboard_text` / `cancel_ime` / `update_ime_window`
- 控件通过 `widgets::widget_ui()`、`widgets::widget_runtime()` 访问 UI/跨帧状态。

## 边界规则

| 层 | 可包含 | 不可包含 |
|---|---|---|
| `ui` | 标准库、`ui/*` | `runtime/*`、`widgets/*`、`gfx/*`、`platform/*`、`app/*`、Windows/IMM |
| `text` | 标准库、`text/*` | 其他模块 |
| `platform` | 标准库、Windows、`platform/*` | `ui/*`、`text/*`、`gfx/*`、`widgets/*`、`runtime/*`、`app/*` |
| `gfx` | 标准库、`ui/*`、`text/*`、`platform/*`、`gfx/*`、图形 OS 头 | `widgets/*`、`runtime/*`、`app/*` |
| `runtime` | 标准库、`ui/*`、`text/*`、`gfx/*`、`platform/*`、`runtime/*` | `widgets/*`、`app/*` |
| `widgets` | 标准库、`ui/*`、`text/*`、`runtime/*`、`widgets/*` | `gfx/*`、`platform/*`、`app/*`、Windows/IMM、`VK_*` |
| `app` | 所有下层 | — |
| `lua` | 所有层 | — |

`platform` 是叶子工具层，因此 `gfx -> platform` 是允许的单向依赖；`ui` 和 `widgets` 仍不得接触 Windows/IMM/gfx。

## 命名

- 所有库自有 C++ 标识符使用 snake_case：类型、函数、方法、字段、枚举、枚举值、常量。
- 命名空间就是前缀：`flux::`、`flux::widgets`、`flux::platform`、`flux::lua`；不添加文本 `flux_` 前缀。
- 生命周期成对函数统一 `begin_*` / `end_*`，不保留旧键名。
- 跨帧状态：`*_state`；begin/end 栈记录：`*_scope`；选项：`*_options`；结果：`*_result`。
- C++ 索引 0-based；Lua 1-based，在绑定边界转换。
- 外部标识符保持原样：Win32/WinRT/Direct2D/DirectWrite 类型、宏、COM override 方法名。
- 按键常量使用 `flux::keys::*`，数值与 `VK_*` 一致；控件层不直接包含 Windows 头。

## 验证

- `luamake test`：85/85。
- `check_stability.ps1`：静态帧 diff=0。
- 结构检查：
  - `src/ui` 不出现 `runtime/`、`gfx/`、`platform/`、`widgets/`、`app/`、Windows/IMM。
  - `src/widgets` 不出现 `gfx/`、`platform/`、`app/`、Windows/IMM、`VK_`。
  - `src/gfx` 不出现 `widgets/`、`runtime/`、`app/`。
  - `src/runtime` 不出现 `widgets/`、`app/`。
