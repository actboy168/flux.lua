# 文本输入与 IME

## 决策

- `input_text` 是单行编辑框；调用方（Lua）持有已提交的字符串，控件每帧返回 `changed, value, submitted, deactivated, focused`。调用方在 `changed` 为真时写回 `value`。
- 编辑状态按 `ui_id` 存在 `widget_runtime` 中：caret/anchor、水平滚动、拖拽状态、undo/redo、focus 起始文本。调用方传入的文本与上次记录不一致时，视为外部修改并清空 undo/redo。
- 光标和命中测试使用 `IDWriteTextLayout::HitTestTextPosition` / `HitTestPoint`；UTF-8 字节偏移与 UTF-16 单元偏移通过 `text/utf8.h` 转换。密码模式使用 U+25CF 掩码，并在掩码偏移和原始标量偏移之间映射。
- IME 在窗口层处理：`WM_IME_STARTCOMPOSITION` / `WM_IME_COMPOSITION` / `WM_IME_ENDCOMPOSITION`。`GCS_COMPSTR` 存入 `InputState::ime_composition`（UTF-8，光标为 UTF-8 偏移），只用于预览；`GCS_RESULTSTR` 追加到 `InputState::text`，由已聚焦的输入控件在下一帧插入。composition 不直接改缓冲区，避免取消时回滚困难。
- 候选窗和组字窗口由输入控件每帧用当前 caret 的物理客户区坐标更新（`ImmSetCompositionWindow` / `ImmSetCandidateWindow` / `ImmSetCompositionFontW`）。
- 剪贴板使用 `CF_UNICODETEXT`；粘贴到单行输入时去掉 CR/LF，按 UTF-8 标量边界截断。
- 文本输入聚焦时 Esc 只回退到 focus 起始文本，不关闭窗口；Tab/Shift+Tab 仍由焦点系统处理。
- 快捷键使用 `InputState::key_ctrl/key_shift/key_alt` 快照：在按键按下时记录当时修饰键状态，而不是在帧开始时读当前 `ctrl/shift`。这样 Ctrl+A 后快速松开 Ctrl、或整个按键序列在一批消息内处理时仍能正确识别快捷键。

## Why

- 立即模式没有 retained 文本控件；Lua 持有值是唯一事实来源，控件只保留编辑元数据，避免两份文本不一致。
- IME composition 必须可见但不提交；把 composition 作为预览 splice 到显示字符串，提交时走正常 `input.text` 插入路径，取消不影响缓冲区。
- `HitTestTextPosition` 能正确处理 kerning、grapheme 和后续多行布局，比逐前缀测量稳定。

## How to apply

- 新增文本类控件时复用 `edit_state` 和 `edit_core`，不要另建一套 caret/selection 逻辑。
- 调用方必须处理 `changed` 并写回 `value`；忽略返回值会被视为外部修改并重置编辑状态。
- 多行输入是后续工作：需要把 `text_options.wrap` / 高度、垂直滚动和跨行选择矩形接入同一套 caret 映射。
- 真实 IME 候选窗行为不同，验收必须在微软拼音/搜狗和 100%/150% DPI 下实测。
