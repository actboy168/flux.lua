# 内部设计说明

这里记录当前实现中非显然的约束、取舍和阈值，面向维护者和贡献者，不是库使用文档。使用方式请看 [API 参考](../api.md) 和 [架构说明](../architecture.md)。

- [模块分层与命名](core-widget-layering.md) — 依赖方向、入口、边界规则和命名约定。
- [draw_cmd 布局与文本 arena](draw-command-layout.md) — 56 字节 POD、per-kind union、文本 offset/length arena。
- [WUC 动画与合成层](wuc-animation-layering.md) — 单 visual + DXGI flip swap chain、逻辑 draw buffer、合成线程动画。
- [控件动画](control-animation.md) — per-ui_id CPU 补间、首帧 snap、reduced motion 和 prune。
- [文本输入与 IME](text-input.md) — Lua 持有文本、编辑状态、composition 预览和候选窗定位。
- [滚轮链式消费与平滑滚动](scroll-routing.md) — 上一帧 rect 路由、剩余量交给父级、160ms ease-out。
- [控件运行时状态](widget-runtime.md) — 跨帧状态集中、统一 reset/prune、单窗口入口。
- [虚拟化列表与选择模型](virtual-list.md) — 固定行高窗口、固定内容高度 child、persistent selection。
- [测试代码编译宏](release-test-split.md) — `FLUX_ENABLE_TESTING`、发布/测试构建和产物路径。
