# flux.lua

Windows 上的原生即时模式 UI 框架，基于 Win32、Windows.UI.Composition、Direct2D 和 DirectWrite，提供原生 Fluent 观感与 DirectWrite 文本质量。

- **即时模式**：每帧重建 draw list，不维护 retained 控件树；状态通过 `ui_id` 跨帧保留。
- **原生渲染**：单个 WUC `SpriteVisual` + DXGI flip-model swap chain，Direct2D 绘制，DirectWrite 排版。
- **文本与输入**：单行文本编辑、选区、剪贴板、撤销/重做、IME composition 与候选窗定位。
- **内置控件**：按钮、复选框、页签、表格、虚拟化列表、combo、弹出菜单、tooltip、进度条、表单布局等。
- **动效**：控件 hover/press/focus/selected 补间，popup/tooltip 淡入，滚动平滑，WUC 图层动画、blur、阴影和裁剪。
- **可测试**：离屏渲染、确定性输入注入和 draw list 导出，用于 golden 与行为测试。

## 环境要求

- Windows 10 1809 或更高版本，x64
- Visual Studio 2022（MSVC，C++20）
- [luamake](https://github.com/actboy168/luamake)

## 构建

```powershell
luamake
```

## 快速开始

```lua
local flux = require "flux"

local ok, err = flux.create {
    title = "flux.lua",
    width = 560,
    height = 540,
    backdrop = "mica", -- "none" | "mica" | "acrylic"
}
assert(ok, err)
flux.show()

local count = 0
while flux.pump() do
    if flux.begin_frame() then
        flux.clear(0x00000000)
        flux.set_cursor_pos(24, 24)
        if flux.button("点击 +1", 120, 36) then
            count = count + 1
            flux.request_frame()
        end
        flux.text(24, 76, "count = " .. count, 0xFFFFFFFF, 14)
        flux.end_frame()
    else
        flux.sleep()
    end
end
flux.shutdown()
```

颜色格式为 `0xAABBGGRR`，所有公开坐标使用逻辑像素。`flux.begin_frame()` 在输入、动画或显式请求存在时返回 `true`；空闲帧可以直接跳过绘制，由宿主用 `flux.sleep()` 让出 CPU。

## Demo

```powershell
luamake lua demo.lua
luamake lua demo.lua 120 mica
luamake lua demo.lua 120 none 1.25
```

参数依次是帧数上限、背景材质、DPI scale 和初始页面。Demo 包含基础控件、输入、布局、虚拟列表、表格和视觉/动画六个页面：

```powershell
luamake lua demo.lua 120 none - visual
```

页面 key：`basics`、`input`、`layout`、`list`、`table`、`visual`。

## 文档

- [API 参考](docs/api.md)
- [架构与实现](docs/architecture.md)
- [测试与基准](docs/testing.md)
- [内部设计说明](docs/decisions/README.md)
