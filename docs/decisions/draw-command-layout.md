# draw_cmd 布局与文本 arena

## 决策

`draw_cmd` 固定为 56 字节、trivially copyable，按 kind 使用 union；文本不再内联 `std::string`，而是写入 `draw_list` 的独立 NUL 结尾 arena，命令只保存 `uint32_t offset/length`。

```cpp
struct draw_cmd {
    draw_kind kind;        // uint8_t
    uint8_t gradient_type; // 仅 gradient
    uint8_t flags;         // bit0 wrap, bit1 ellipsis
    uint8_t alpha_byte;    // 用于合成隐式 color2
    float x, y;
    uint32_t color;
    union {
        struct { float w, h, radius, thickness; } rect;
        struct { float w, h, radius; uint32_t color2; } gradient;
        struct { float w, h; } clip;
        struct { uint32_t offset, length; float size, text_width, text_height, line_height;
                 int32_t max_lines, align, valign, weight; } text;
    } payload;
};
```

读取一律通过 accessor（`width()`、`corner_radius()`、`stroke_thickness()`、`font_size()`、`color2()`、`text_of()` 等），禁止直接读未激活 union 成员。

## 为什么

- 原 `draw_cmd` 112 字节，其中 32 字节是 `std::string`；纯矩形命令也携带字符串对象，`clear()` 时销毁所有文本并释放非 SSO 缓冲区。
- 文本 arena 保留容量跨帧复用，命令变成 POD，vector 扩容只是字节移动。
- union 去掉按 kind 互斥的字段；Lua `draw_commands` 的 21 个字段由 accessor 合成历史默认值。

参考 `build/fluxent` 的 `XentDlCmd`：小 header + per-op union、文本 offset/length 存入 NUL 结尾 arena、builder 复用容量。未采用它的 retained 节点 display-list 缓存和约 200 字节的 `FluxRenderCommand`。

## 必须保持

- `sizeof(draw_cmd) == 56`，`is_trivially_copyable` / `is_standard_layout`。
- 非 gradient 的 `color2` 默认值 `0xFFFFFFFF`，在 `push_alpha` 下按烘焙后的 `alpha_byte` 合成，例如 alpha 0.5 → `0x80FFFFFF`；clip 不受 alpha 影响。
- 非 text 的 `size=14`、`thickness=1`、`weight=400`、text 字段为空/0；text 的 `w/h/radius=0`。
- 四层顺序、命令粒度、clip 平衡、offset/alpha 烘焙语义不变。
- 文本 offset 只在 `clear()` 时失效；不缓存跨 append 的 `string_view`。

## How to apply

- 新增 draw kind 时：在 union 加 payload、在 accessor 加分支、更新 replay 和 Lua exporter；不要把所有字段重新放回公共结构。
- 新增/修改文本：只通过 `append_text` 写入 arena，不要在命令里存指针或 `string_view`。
- 不要为了“优化”合并 draw command；draw-list 测试依赖 shadow/focus/elevation 的命令粒度。
