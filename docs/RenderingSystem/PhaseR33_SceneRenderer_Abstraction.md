# PhaseR33：SceneRenderer 抽象 ?? 初步分析

> **文档性质**：初步分析（Pre-Design），不是完整设计文档。目的是把"是否需要引入 SceneRenderer 抽象、以及如何引入"这件事讨论清楚，为后续正式设计文档打基础。

---

## 1. 概述

### 背景

Luck3D 当前的渲染系统架构可以概括为一句话：**"`Renderer3D` 是一个全局静态类，通过全局 `s_Data` 状态跑一条硬编码的渲染管线"**。

具体现状：

| 组件 | 现状 |
|------|------|
| `Renderer3D` | 全局静态类（`class Renderer3D { static void Init(); static void BeginScene(...); ... };`），内部 `static Renderer3DData s_Data;` 承载全部状态 |
| `s_Data` | 持有 `RenderPipeline / RenderContext / CameraUBO / LightUBO / Stats / OpaqueDrawCommands / ...` 全部渲染状态 |
| `RenderPipeline` / `RenderPass` / `RenderContext` | ? 已经数据驱动、Pass 可组合，但**整条 pipeline 是全局单实例** |
| `Framebuffer` | ? 归属**面板层**（`SceneViewportPanel` 自己 `new`），不是渲染器内部资源 |
| `IBLPrecompute` | ? 独立的全局静态类，与 `Renderer3D` 平级（"缺 SceneRenderer 抽象只好各自单写"的活证据） |
| 面板 → 渲染器通信 | 通过 `SetTargetFramebuffer / SetClearColor / SetOutlineEntities / SetOutlineColor / ResizePipeline` 等**全局 setter** |

### 问题

Luck3D 短期能工作、长期存在**架构性缺陷**：

1. **无法同时维护多套独立渲染状态** ?? P0.6 Game 面板落地后，Scene 面板与 Game 面板共用 `Renderer3D::s_Data`，阴影贴图 / 后处理链路 / DrawCommand 列表都会互相污染（当前"能凑合工作"是因为 BeginScene/EndScene 严格同步串行）
2. **无法做资产预览缩略图** ?? 要给 Material / Mesh / Prefab 生成 128×128 缩略图，需要一个独立小相机 + 独立小 RT + 简化管线；当前的全局架构做不到"跳过阴影 / 后处理只走 Simple Lighting"
3. **无法做反射探针 / IBL 烘焙的抽象复用** ?? `IBLPrecompute` 就是被迫独立写的一套渲染逻辑，未来加反射探针（Reflection Probe）会再重复一次
4. **无法做 PIP / 后视镜 / 小地图** ?? 这类"多相机独立渲染"能力对全局 `s_Data` 是致命的
5. **面板与渲染器高耦合** ?? Panel 通过一堆全局 setter 与 Renderer3D 对话，Panel 隐式承担了"渲染上下文管理者"的角色（不合理）

### 目标

引入 **`SceneRenderer` 抽象**：把"一次完整的场景渲染"封装成**可实例化的对象**，让 Luck3D 能像 Unity / Unreal / Hazel 一样，同时跑多个独立的渲染上下文。

产出：

1. 新增 `SceneRenderer` 类：**可实例化的渲染管线执行器**，持有自己的 Framebuffer / Pipeline / UBO / Stats
2. `Renderer3D` 降级为"低级 API"：只保留 `DrawMesh / DrawSprite / DrawArrays / GetShaderLibrary` 等无状态原语和全局共享资源
3. `Framebuffer` 归属从"面板层"下移到"SceneRenderer 内部"
4. 面板通过 `SceneRenderer` 对象进行渲染 ?? 每个面板持有自己的实例
5. `IBLPrecompute` 等特殊管线也统一到 `SceneRenderer` 抽象下（长期目标，可分阶段）

### 前置依赖
- 无强依赖，但**建议在 P0.6 GameViewportPanel 落地前完成**，否则 Game 面板会以"污染全局 s_Data"的方式落地，之后重构成本更高

### 本 Phase **不做**的事
- **不重写渲染算法**：所有 Pass / Shader / 光照计算保持不动
- **不引入 RenderGraph**：SceneRenderer 只是"pipeline 实例化"，不是"pass 依赖图"
- **不改造 `IBLPrecompute`**：本 Phase 保留其独立形态，未来独立 Phase 再合并
- **不动 Renderer2D / GizmoRenderer**：先让 Renderer3D 走通，2D / Gizmo 按同样模式后续跟进

---

## 2. 主流引擎的对标参考

| 引擎 | 相机→图像的抽象 | 特点 |
|------|-------------|------|
| **Unreal** | `FSceneView` + `FSceneViewFamily` + `FSceneRenderer` | 每次 `Draw` 构造一个 SceneRenderer 实例；View 是相机数据 |
| **Unity SRP** | `ScriptableRenderer` + `RenderingData` + `CameraData` | 每个相机对应一次 SRP 执行，状态实例化 |
| **Godot 4** | `RenderSceneBuffers` / `RenderSceneData` | 每个视口拥有一份 |
| **Hazel** | `SceneRenderer` | 可 `new` 出多个实例（Scene 视图 / Game 视图 / 缩略图各持有一个） |
| **Filament** | `View` + `Renderer` | View 是 Camera+RT，Renderer 是执行器 |

**共同点**：所有主流引擎都把"一次场景渲染"实例化，与"全局渲染服务（低级 API）"分层。**Luck3D 现在停留在"全局单例"阶段**，是 Hazel 早期形态。

---

## 3. 现状盘点

### 3.1 `Renderer3D::s_Data` 到底承载了什么

从 [Renderer3D.cpp](../../Lucky/Source/Lucky/Renderer/Renderer3D.cpp) 里 `Renderer3DData` 结构体现有字段（按"能否实例化"分类）：

| 类别 | 字段（示例） | 是否可实例化 |
|------|------------|-------------|
| **A. 全局共享资源** | `ShaderLib / DefaultMaterial / InternalErrorMaterial / DefaultTextures / InternalErrorShader / StandardShader` | ? 应保持全局 |
| **B. 每次渲染独立的 UBO** | `CameraUniformBuffer / LightUniformBuffer / CameraBuffer / LightBuffer` | ? 应下移到 SceneRenderer |
| **C. 每次渲染独立的 Pipeline** | `Pipeline`（含 ShadowPass / OpaquePass / SkyboxPass / ... / PostProcessPass / SilhouettePass / OutlineCompositePass） | ? 应下移到 SceneRenderer |
| **D. 每次渲染独立的 DrawCommand 队列** | `OpaqueDrawCommands / TransparentDrawCommands / SpriteDrawCommands / OutlineDrawCommands` | ? 应下移到 SceneRenderer |
| **E. 每次渲染独立的相机/光照缓存** | `CameraPosition / CameraViewMatrix / CameraProjectionMatrix / ShadowEnabled / CascadeLightSpaceMatrices` | ? 应下移到 SceneRenderer |
| **F. 每次渲染独立的输出目标引用** | `TargetFramebuffer / ClearColor / OutlineEntityIDs / OutlineColor` | ? 应下移到 SceneRenderer |
| **G. 每次渲染独立的统计** | `Stats` | ? 应下移到 SceneRenderer |
| **H. 环境/场景无关配置** | `PostProcess / Environment / SkyboxMaterial`（相机无关但场景相关） | ?? 归属场景或 SceneRenderer（讨论） |

**观察**：`s_Data` 里**大部分字段都是"每次渲染独立"性质**，只是被硬塞到全局。这是 SceneRenderer 抽象后要下移的主体内容。

### 3.2 `RenderPipeline` 已经具备的能力

好消息是 [RenderPipeline.h](../../Lucky/Source/Lucky/Renderer/RenderPipeline.h) 已经是**可实例化**的：

```cpp
class RenderPipeline
{
    void Init();
    void AddPass(const Ref<RenderPass>& pass);
    void Execute(const RenderContext& context);
    void Resize(uint32_t width, uint32_t height);
    ...
private:
    std::vector<Ref<RenderPass>> m_Passes;
};
```

**它天然支持"多实例"** ?? 只是当前只有 `s_Data.Pipeline` 一个实例。SceneRenderer 抽象后，每个实例持有自己的 `RenderPipeline`，各自 `AddPass` 想要的 pass 组合即可。

### 3.3 `RenderContext` 已经是数据驱动

[RenderContext.h](../../Lucky/Source/Lucky/Renderer/RenderContext.h) 承担"每次渲染的输入包"，Pass 从中取数据。这套机制**已经很干净**，SceneRenderer 抽象后只需让 `RenderContext` 由 SceneRenderer 实例构造，而不是全局。

### 3.4 Framebuffer 归属现状

Framebuffer 目前**由面板持有**：
```cpp
// SceneViewportPanel
Ref<Framebuffer> m_Framebuffer;   // Panel 自己 new
```

然后 Panel 每帧调 `Renderer3D::SetTargetFramebuffer(m_Framebuffer)` 把自己的 FBO **推给全局渲染器**。这个交互模式**是全局架构的必然产物** ?? SceneRenderer 抽象后，FBO 应该由 SceneRenderer 实例持有，Panel 只是"我需要显示某个 SceneRenderer 的最终图像"的消费者。

### 3.5 `IBLPrecompute` 作为"独立渲染"的先例

[IBLPrecompute.cpp](../../Lucky/Source/Lucky/Renderer/IBLPrecompute.cpp) 是一段**完全独立**的渲染代码：
- 自己的 static 数据 (`s_IBLData`)
- 自己的临时 FBO / Cube VAO
- 自己走 `SaveRenderState() / RestoreRenderState()` 保护主渲染状态
- 不复用 Renderer3D 的 Pipeline

**这正是"没有 SceneRenderer 抽象只好每种独立渲染都单写"的证据**。未来加反射探针会重复这种模式。

---

## 4. 目标架构

### 4.1 分层图

```
┌───────────────────────────────────────────────────────────────┐
│  应用/编辑器层                                                 │
│  ┌────────────────────┐  ┌────────────────────┐               │
│  │ SceneViewportPanel │  │ GameViewportPanel  │  ...          │
│  │ ↓ 持有             │  │ ↓ 持有             │               │
│  │ SceneRenderer#A    │  │ SceneRenderer#B    │               │
│  └─────────┬──────────┘  └─────────┬──────────┘               │
│            │                       │                          │
├────────────┼───────────────────────┼──────────────────────────┤
│  渲染系统层 │                       │                          │
│            ?                       ?                          │
│  ┌────────────────────────────────────────────────┐           │
│  │ SceneRenderer （可实例化）                     │           │
│  │  - Framebuffer                                 │           │
│  │  - RenderPipeline (自己的 Pass 组合)           │           │
│  │  - CameraUBO / LightUBO                        │           │
│  │  - DrawCommand 列表                            │           │
│  │  - Stats                                       │           │
│  │  - Outline / Post Process / Target 配置        │           │
│  │  ─────────────────────────────────────────     │           │
│  │  API:                                          │           │
│  │    Init(spec)                                  │           │
│  │    SetViewportSize(w, h)                       │           │
│  │    BeginScene(CameraRenderData, LightData)     │           │
│  │    SubmitMesh / SubmitSprite                   │           │
│  │    EndScene()                                  │           │
│  │    RenderOutline()                             │           │
│  │    GetFinalPassImage()                         │           │
│  └───────────────────────┬────────────────────────┘           │
│                          │ 调用低级 API                        │
│                          ?                                    │
│  ┌────────────────────────────────────────────────┐           │
│  │ Renderer3D （低级 API，全局静态）              │           │
│  │  - ShaderLibrary                               │           │
│  │  - DefaultMaterial / InternalErrorMaterial     │           │
│  │  - DefaultTextures                             │           │
│  │  - DrawMesh / DrawSprite （无状态提交）         │           │
│  │  - Init / Shutdown                             │           │
│  └────────────────────────────────────────────────┘           │
│                                                               │
│  ┌────────────────────────────────────────────────┐           │
│  │ RenderCommand / RenderPass / RenderContext /   │           │
│  │ Framebuffer / Shader / Material / UBO / ...    │           │
│  └────────────────────────────────────────────────┘           │
└───────────────────────────────────────────────────────────────┘
```

### 4.2 SceneRenderer 的职责边界

**SceneRenderer 负责**：
- 拥有一次完整渲染所需的**独立状态**（FBO / Pipeline / UBO / DrawCommand 队列）
- 提供 `BeginScene / SubmitMesh / EndScene` 高层 API
- 提供 `GetFinalPassImage()` 输出图像
- Framebuffer resize / Pass 组合配置

**SceneRenderer 不负责**：
- 全局着色器库、默认材质、默认纹理（仍在 Renderer3D）
- 具体的 GPU 命令（走 RenderCommand）
- 场景遍历与 Component 收集（这是 Scene 的职责，SceneRenderer 只接受"已折算好的 CameraRenderData + LightData + DrawCommand"）

**分层清晰之后的调用**：
```cpp
// 面板层
sceneRenderer->SetViewportSize(w, h);
sceneRenderer->SetTargetFramebuffer(m_Framebuffer);   // 或者 SceneRenderer 内部持有 FBO
sceneRenderer->BeginScene(cameraRenderData, lightData);
scene->SubmitAll(sceneRenderer);   // Scene 遍历，把每个 Entity 的 Mesh 塞进来
sceneRenderer->EndScene();

// 显示
ImGui::Image(sceneRenderer->GetFinalPassImage(), size);
```

---

## 5. 关键设计问题（初步讨论）

以下是本 Phase 落地时必须面对的关键问题，**每个问题先给出思路**，正式设计文档时再逐个方案对比。

### 5.1 【问题 1】Framebuffer 归属：SceneRenderer 内部 vs 外部注入

#### 思路 A：SceneRenderer 内部持有 FBO（**倾向推荐**）

- SceneRenderer 构造时接受 `FramebufferSpecification`，自己创建 FBO
- Panel 只调 `GetFinalPassImage()` 拿最终纹理去 ImGui::Image
- **优点**：封装干净，Panel 不再操心 FBO 细节；对齐 Unity/Unreal
- **缺点**：Panel 现有代码需改造（当前 Panel 自己 new Framebuffer）

#### 思路 B：Panel 传入 FBO，SceneRenderer 只用不建

- 保留当前 Panel 持有 FBO 的模式
- SceneRenderer 通过 `SetTargetFramebuffer` 接收
- **优点**：改造小
- **缺点**：Panel 依然要感知 FBO 规格；两个 Panel 想共享/不共享 FBO 时逻辑变复杂

**倾向**：思路 A，但可以给个"外部 Framebuffer override"接口作为逃生舱。

### 5.2 【问题 2】`Renderer3D` 的边界收缩到什么程度

#### 思路 A：只留全局共享资源 + 无状态 Draw API（**倾向推荐**）

`Renderer3D` 只剩：
- `Init / Shutdown`（全局初始化：ShaderLib / 默认材质 / 默认纹理）
- `GetShaderLibrary / GetDefaultMaterial / GetInternalErrorMaterial / GetDefaultTexture`
- （可选）静态无状态 `DrawMesh` 直接绕过 SceneRenderer（少用）

`s_Data` 大幅瘦身：只保留 A 类字段（Shader / DefaultMaterial / DefaultTextures）。

- **优点**：分层最清晰
- **缺点**：改造面最大

#### 思路 B：Renderer3D 保留部分状态字段，与 SceneRenderer 混用

- 部分字段（如 CameraUBO）保留在 Renderer3D，多个 SceneRenderer 通过某种"上下文推送"共享
- **优点**：改造小
- **缺点**：分层不清，本质是"多个 SceneRenderer 抢用一个全局 UBO"，性能上可能反而变差（每次切换都要重推 UBO）

**倾向**：思路 A。

### 5.3 【问题 3】RenderPass 实例的归属

现在所有 Pass 是 `s_Data.Pipeline` 里的 `Ref<RenderPass>`，Pass 内部可能持有自己的 FBO / Shader / State。**多个 SceneRenderer 各自持有独立的 Pass 实例？还是共享？**

#### 思路 A：每个 SceneRenderer 各自构造自己的 Pass 实例（**倾向推荐**）

- 每个 SceneRenderer 有独立的 ShadowPass / OpaquePass / PostProcessPass 等
- 每个 Pass 内部的 FBO 也独立
- **优点**：状态完全隔离；不同 SceneRenderer 可以启用不同 Pass 组合
- **缺点**：Pass 内的 FBO 会重复占显存（Shadow Map 尤其）

#### 思路 B：Pass 实例共享，`RenderContext` 传入不同上下文

- 所有 SceneRenderer 共享同一份 Pass 实例
- Pass 从 `RenderContext` 拿"这次要用的 FBO / Shader"
- **优点**：显存节省
- **缺点**：Pass 逻辑变复杂；Shadow Pass 的 Shadow Map 需要每帧重建，不适合共享

#### 思路 C：混合：Shadow Pass 等重资源 Pass 共享，轻资源 Pass 独立

- 阴影贴图 / IBL 等重资源 Pass 由 Renderer3D 全局管
- 后处理 / Silhouette 等轻资源 Pass 每 SceneRenderer 独立
- **优点**：性能与隔离折中
- **缺点**：分层不完全统一

**倾向**：思路 A（默认每个 SceneRenderer 独立 Pass 实例），配 SceneRenderer 构造参数按需启用/禁用 Pass。**Shadow Map / IBL 属于场景级共享资源**，可以由 Scene 层管理并推给所有 SceneRenderer（未来展开）。

### 5.4 【问题 4】SceneRenderer 的配置能力：Pass 组合可配

不同用途的 SceneRenderer 需要不同 Pass 组合：

| 用途 | 需要的 Pass |
|------|-------------|
| Scene 面板 | Shadow + Opaque + Skybox + Transparent + Sprite2D + Picking + DebugVisualize + PostProcess + Silhouette + OutlineComposite |
| Game 面板 | Shadow + Opaque + Skybox + Transparent + Sprite2D + PostProcess （无 Picking / Silhouette / OutlineComposite / DebugVisualize） |
| 资产缩略图 | Opaque + Skybox （无阴影 / 无后处理 / 无描边） |
| 反射探针烘焙 | Opaque + Skybox （无阴影 / 无后处理） |

**方案**：SceneRenderer 构造时接受一个 "Pass 组合配置"（可以是预设 enum，也可以是显式 `AddPass` 列表）。

### 5.5 【问题 5】改造顺序：一步到位 vs 增量

#### 思路 A：一次性重构（**不推荐**）
- 一次改完 `Renderer3D` + 所有 Panel + 所有 Pass
- 改动面大，一次验证难

#### 思路 B：增量迁移（**倾向推荐**）
分三个子阶段：
- **R33.1**：新建 `SceneRenderer` 骨架，内部**转发**到 `Renderer3D::s_Data`（保持全局单实例的行为，只是包一层 API）
- **R33.2**：把 `s_Data` 里"每次渲染独立"的字段（CameraUBO / DrawCommand 队列 / Stats / Outline / Target）真正下移到 SceneRenderer 实例，Panel 改造为持有 SceneRenderer
- **R33.3**：把 Pass 实例也下移（每 SceneRenderer 独立 Pass），启用 Pass 组合配置

每个子阶段独立可验证，风险可控。

**倾向**：思路 B。

---

## 6. 影响面预估

### 6.1 必须改动的文件

| 文件 | 改动性质 | 工作量 |
|------|--------|-------|
| `Lucky/Source/Lucky/Renderer/SceneRenderer.h/.cpp` | 新建 | 大 |
| `Lucky/Source/Lucky/Renderer/Renderer3D.h/.cpp` | `s_Data` 大幅瘦身；一部分 API 迁移到 SceneRenderer | 大 |
| `Lucky/Source/Lucky/Scene/Scene.cpp` | `RenderSceneImpl` 改为向 SceneRenderer 提交 DrawCommand 而非全局 Renderer3D | 中 |
| `Luck3DApp/Source/Panels/SceneViewportPanel.h/.cpp` | 持有 SceneRenderer 实例；Framebuffer 归属可能变化；一堆全局 setter 调用改成 SceneRenderer 方法 | 中 |
| `Luck3DApp/Source/Panels/GameViewportPanel.*` | 未来 P0.6 落地时对齐 SceneRenderer 模式 | 中（未来） |
| 各 `Passes/*.cpp` | 从 `RenderContext` 取数据的方式基本不变，但 Pass 实例的持有者从 Renderer3D 换成 SceneRenderer | 小 |

### 6.2 不动的文件

- `RenderCommand / Shader / Material / Framebuffer / Buffer / VertexArray / Texture / ...` ?? 底层 GPU 抽象层完全不动
- `RenderPass / RenderPipeline / RenderContext` ?? 已是数据驱动，本身无需改动，只是使用者从 Renderer3D 变成 SceneRenderer
- 所有 Shader ?? 完全不动
- Renderer2D / GizmoRenderer ?? 本 Phase 保持全局单例，未来独立 Phase 跟进

### 6.3 与其他 Phase 的关系

| Phase | 与 R33 的关系 |
|-------|-------------|
| **P0.2 CameraComponent** | 已产出 `CameraRenderData`，正好作为 SceneRenderer `BeginScene` 的相机侧输入。R33 与 P0.2 无冲突 |
| **P0.6 GameViewportPanel** | **建议先做 R33，再做 P0.6**。否则 Game 面板会以"污染 s_Data"的方式落地，R33 之后要返工 |
| **未来反射探针 / 缩略图 Phase** | R33 是前置依赖 |

---

## 7. 时序建议

```
PhaseR32 Camera 基类重构 ?（已完成）
    ↓
P0.2 CameraComponent + SceneCamera（进行中/接下来）
    ↓
PhaseR33 SceneRenderer 抽象 ← 本 Phase
  ├─ R33.1 骨架 + 转发
  ├─ R33.2 状态下移到实例
  └─ R33.3 Pass 实例化 + 配置化
    ↓
P0.5 Play/Stop 工具条
P0.6 GameViewportPanel（此时基于 SceneRenderer 落地，天然多面板独立）
    ↓
未来：资产缩略图 / 反射探针 / PIP / ...
```

**核心结论**：**PhaseR33 应在 P0.6 之前完成**，脚本系统 P0.2-P0.5 与本 Phase **并行不冲突**（P0.2 只影响相机侧输入，P0.3/P0.4/P0.5 与 Renderer 关系不大）。

---

## 8. 风险与顾虑

1. **改造面较大**：`Renderer3D::s_Data` 的字段被项目里许多地方引用，逐个迁移需要仔细
2. **Pass 实例复制的显存成本**：每个 SceneRenderer 独立持有 ShadowPass 意味着独立的 Shadow Map，多面板并存时显存翻倍 ?? 可能需要"场景级共享 Shadow Map"的机制（问题 3 思路 C）
3. **Panel 交互改造**：现有 Panel 通过一堆全局 setter 与渲染器对话，改造成 SceneRenderer 方法调用后，语义更清晰但改动点多
4. **`IBLPrecompute` 暂不合并**：本 Phase 结束时 IBL 依然是独立全局，架构上不完全统一 ?? 需要在未来独立 Phase 处理（可以叫 PhaseR34_IBLProbe_SceneRenderer_Merge）

---

## 9. 本文档的下一步

本文档是**初步分析**，目的是把决策空间厘清。正式启动本 Phase 前，需要产出一份完整的**设计文档 PhaseR33_SceneRenderer_Design.md**，包含：

1. `SceneRenderer` 类的完整头文件设计（含所有字段、方法、构造参数）
2. `Renderer3D` 收缩后的完整头文件设计
3. R33.1 / R33.2 / R33.3 三个子阶段的**逐步改造清单**
4. 关键设计问题（本文档 §5）的**方案对比与最终决策**
5. 逐文件的迁移前后代码骨架对比
6. 验收标准（含性能对比：改造前后帧率 / 显存占用）
7. 与 P0.2 CameraComponent 的接口对齐验证

**建议触发时机**：P0.2 完成后、P0.5 之前，开始起草 PhaseR33 正式设计文档。
