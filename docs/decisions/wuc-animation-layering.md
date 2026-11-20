# WUC 动画与合成层

## 决策

- 窗口只使用 **一个 WUC SpriteVisual**（`content`），覆盖整个客户区；它的画刷来自一个 **DXGI flip-model swap chain**（`CreateSwapChainForComposition` → `CreateCompositionSurfaceForSwapChain`）。`draw_list` 仍保留 `content` / `overlay` / `popup` / `tooltip` 四个逻辑绘制缓冲；渲染器按这个顺序把它们绘制到同一个 D2D 后缓冲，只做一次 `Clear`，然后 `Present(1, 0)` 整帧翻转，不创建额外的全窗口 visual/surface。
- 每帧路径是 `ID2D1DeviceContext::BeginDraw` → `Clear` → 逐层 replay → `EndDraw` → `IDXGISwapChain1::Present(1, 0)`。**不要再使用 `CompositionDrawingSurface` + `ICompositionDrawingSurfaceInterop::BeginDraw/EndDraw` 做连续动画**：原地 surface 在绘制期间会被 DWM 采样，出现只含 `Clear` 的空白帧。
- `Present(1, 0)` 同时承担帧节奏；不再额外调用 `DCompositionWaitForCompositorClock`。设备丢失由 `Present`/`EndDraw` 的 `HRESULT` 触发 `recover()`。
- `flux.layer.use(name)` 只切换后续命令进入哪个绘制缓冲；`overlay` / `popup` / `tooltip` 不是独立的合成层，也没有自己的 visual。
- 合成线程动画、`set`、blur、shadow、clip 只作用于 `content`（整个窗口）。对其他名字调用这些接口会返回 `false, err`，避免调用方误以为存在独立的可动画层。
- 模态 dim 直接画进 `content` 绘制缓冲，放在页面上、popup/tooltip 之前；不创建全窗口半透明 overlay surface。
- `draw_list` 为每个逻辑层维护独立命令缓冲和 `clip_depth`；允许在当前层仍有 clip 时切换层，replay 时各层 clip 独立平衡。`begin_popup` 同时保存并清空 `ui_context` 的逻辑 clips，`end_popup` 恢复，避免父级裁剪影响 popup 命中/绘制。
- 公开坐标是逻辑像素；WUC visual 的 `Offset` 按 `dpi_scale` 转换为物理单位。所有转换集中在 `composition_renderer::apply_layer_property`。
- 动画在合成线程执行；UI 线程只负责启动、取消和收尾。keyframe/spring/iteration/easing/direction 通过 `ICompositionAnimation` 启动，有限动画放在 `ICompositionScopedBatch` 中。
- 高级效果通过自定义 WRL `IGraphicsEffect` + `IGraphicsEffectSource` + `IGraphicsEffectD2D1Interop` 实现（GaussianBlur）。必须报告 D2D effect 的**全部**属性（GaussianBlur 为 StandardDeviation / Optimization / BorderMode），越界索引用 `E_BOUNDS`，属性值用 `Windows.Foundation.PropertyValue` 装箱；source 用 `CompositionEffectSourceParameter`，再 `CreateEffectFactory` → `CreateBrush` → `SetSourceParameter`。`LoadStatus` 可能为 Pending，以 `CreateBrush` 是否成功为准。
- `DropShadow` 使用 `SourcePolicy = InheritFromVisualContent`；`GeometricClip` 用 `ICompositor5/6` 的圆角矩形几何，坐标按 DPI 转换。
- `CompositionScopedBatch.Completed` handler 是 agile 的 WRL RuntimeClass，只做 `PostMessageW(WM_APP + 0x64, id_low, id_high)`；`window_proc` 在 UI 线程调用 `on_animation_completed`。Lua 通过 `flux.anim.poll()` 拉取完成 id，绝不在合成线程调用 Lua。
- 完成或 `apply_final=true` 的取消会把 base transform 更新为终值并 `StopAnimation`；纯取消保留启动时写入的初始 base。无限动画不创建 batch，依赖显式 `flux.anim.stop`。
- 系统 `SPI_GETCLIENTAREAANIMATION` 关闭时，`start_animation` 直接应用终值并将 id 放入完成队列，不创建 WUC 动画对象。
- 设备丢失/recover 会取消在途动画并把 id 入队；保留 base transform 后重建 content 层。
- WUC 的 offset/scale 不参与 `ui_context` 命中测试；交互内容与点击坐标仍以立即模式布局为准。

## Why

- **多个全窗口半透明 WUC surface 叠加会在 DWM 合成时产生像素级抖动/闪烁。** 实测全窗口 dim 放在 overlay surface、popup/tooltip 各自独立 surface 时，静态内容也会在连续截图里变化；把绘制合并到一个 surface 后，多 surface 的交替消失。
- **单个 `CompositionDrawingSurface` 也不能安全地做 60 Hz 连续原地更新。** 每次 `BeginDraw` 会解锁 surface，DWM 可能在 `Clear` 之后、命令重放完成之前采样，连续截图中约一半是只有背景色的空白帧；降低到 10 Hz 后明显减少，证明是 present 时序竞争而非绘制内容错误。
- **flip-model swap chain 提供整帧提交语义。** D2D 只画后缓冲；`Present` 将完整后缓冲翻到前台，DWM 不会看到半帧。实测在 60 Hz 连续绘制下，20 次连续截图的内容区域像素差为 0；`check_stability` 连续运行 5 次全部通过。
- 多 surface 的不稳定还来自跨 visual 的提交顺序不保证同一帧原子出现；单 visual + 单 swap chain 从根上消除了这一类问题。
- 逻辑层仍然有价值：它让 clip 按层平衡，让 popup/tooltip 不被父级 clip 裁剪，并让离屏重放和窗口重放使用同一顺序；这些都不需要独立 visual。
- 合成线程动画不受 UI 线程卡顿影响；完成回调经 HWND 消息回到 UI 线程，避免跨线程访问 Lua/WUC 状态。
- logical→physical 的转换集中在 renderer，便于在 150% DPI 下验证和调整。

## How to apply

- 需要整窗过渡（淡入、位移、缩放）时，对 `content` 使用 `flux.layer.animate`。
- 需要局部浮层动画（toast、菜单入场）时，先在 `content` 缓冲里绘制并用立即模式补间（demo 的 toast 就是 CPU tween），不要为了动画新建全窗口 surface。若将来确需合成线程动画，只创建一个**绑定到该元素矩形**的小 visual/surface，而不是全窗口 surface。
- 模态 dim、菜单背景等静态大面积半透明内容画在 `content`，不要额外创建全窗口半透明层。
- `overlay` / `popup` / `tooltip` 只用于绘制顺序和裁剪隔离；不要在它们上面调用 `animate` / `set` / `blur` / `shadow` / `clip`。
- 新增逻辑层时，同步更新 `draw_list` 的缓冲顺序和 `draw_replay.cpp` 的重放顺序；不会新增 visual。
- 每帧调用 `flux.anim.poll()`；同一 `content` 属性再次启动动画会取消旧动画，旧 id 会出现在 `poll()` 中；`flux.layer.set` 会取消该层所有在途动画，避免 base 与动画双写。
- 静态帧稳定性由 `tools/check_stability.ps1` 覆盖：固定 topmost 后连续截取两帧，要求内容区域（忽略 16 px 边缘）像素差为 0。边缘差异来自 DWM 圆角/边框、阴影、相邻窗口和鼠标指针，不属于框架绘制内容。截图或验证时使用 `shot.ps1 -KeepTopmost`，避免脚本自己切换 topmost 造成假变化。
- 任何新的全窗口绘制后端都必须满足“整帧提交”：优先保持 DXGI flip-model swap chain；不要为了省事退回 `CompositionDrawingSurface` 原地更新。
- 如果后续需要第二块连续更新的 surface（例如独立层动画），仍然只创建元素级小 surface，并分别使用自己的 swap chain；两个全窗口 surface 的提交顺序仍不可依赖。
- Mask/NineGrid、VisualSurface、表达式动画等尚未实现，不改变本决策。
