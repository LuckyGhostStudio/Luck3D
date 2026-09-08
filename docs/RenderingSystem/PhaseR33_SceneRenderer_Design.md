# PhaseR33：SceneRenderer 抽象 ?? 正式设计文档

> **文档性质**：正式设计文档（Design）。基于 [PhaseR33_SceneRenderer_Abstraction.md](./PhaseR33_SceneRenderer_Abstraction.md)（初步分析）收敛决策空间，产出**可直接指导编码**的详细方案。
> **代码风格**：全文所有代码片段严格遵循 [Coding_Style_Guide.md](../Coding_Style_Guide.md)。

---

## 1. 概述

### 1.1 目标

把当前"全局静态 `Renderer3D` + `s_Data` 承载全部状态"的架构，重构为：

- **`SceneRenderer`**：可实例化的渲染管线执行器，持有一次完整场景渲染所需的独立状态（Framebuffer / Pipeline / UBO / DrawCommand 队列 / Stats / Outline / Shadow 参数 / PostProcess / Environment）
- **`Renderer3D`**：收缩为"全局共享资源与初始化服务"，只保留 ShaderLibrary、默认材质、默认纹理、`Init/Shutdown`

每个视口面板（`SceneViewportPanel` / `GameViewportPanel` / 未来的缩略图渲染器）持有独立的 `SceneRenderer` 实例，彻底解决 P0.6 落地后 Scene / Game 面板共用 `s_Data` 造成的隐式耦合。

### 1.2 前置依赖

- P0.1 `SceneState + OnUpdateEditor/OnUpdateRuntime` 已完成
- P0.2 `CameraComponent + CameraRenderData + Scene::OnRenderRuntime()` 已完成
- P0.6 `GameViewportPanel` 已完成（当前存在临时 workaround：`Renderer3D::SetTargetFramebuffer / SetOutlineEntities({})` 显式覆盖，本 Phase 落地后清理）

### 1.3 本 Phase **不做**的事

- **不重写渲染算法**：所有 Pass / Shader / 光照计算 / IBL / CSM 逻辑保持不动
- **不引入 RenderGraph**：`SceneRenderer` 只是 pipeline 实例化，Pass 之间依赖关系保持"注册顺序 + 分组名"现状
- **不合并 `IBLPrecompute`**：IBL 依然是全局静态（`IBLData` 作为场景级共享资源被 `SceneRenderer` 读入 `RenderContext`）
- **不改造 `Renderer2D` / `GizmoRenderer`**：Sprite 走 `SceneRenderer` 提交队列，`Sprite2DPass` 内部继续通过 `Renderer2D` 全局单例 flush（本 Phase 不动）；`GizmoRenderer` 完全独立于本 Phase
- **不引入专为共享阴影贴图而造的抽象层**（例如 SharedShadowResources）：多 SceneRenderer 同时开启时阴影显存与 CPU/GPU 时间翻倍属于可接受代价，与 Unity URP 多相机行为一致
- **不在 `IBLPrecompute` 内部加"按 Cubemap ID 去重缓存"之类的魔法**：IBL 重生成归 Scene 层单一驱动源，从根本上避免多 SceneRenderer 重复触发
- **不改 GPU 抽象层**：`RenderCommand / Shader / Material / Framebuffer / VertexArray / Texture` 完全不动
- **不改 Pass 内部实现**：Pass 从 `RenderContext` 取数据的方式不变

### 1.4 三个子阶段的产出

| 子阶段 | 关键产出 | 验证方式 |
|-------|--------|---------|
| **R33.1 骨架 + 转发** | 新建 `SceneRenderer` 空壳，方法体转发到 `Renderer3D` 全局静态实现；面板改为持有 `Ref<SceneRenderer>` | 编译通过、画面完全等价；`Renderer3D::s_Data` 未动 |
| **R33.2 状态字段下移** | `Renderer3D::s_Data` 中"每次渲染独立"的字段下移到 `SceneRenderer` 实例；`Renderer3D` 大幅瘦身 | Scene / Game 两个面板真正独立；两个面板 ClearColor 设不同值不干扰；DrawCall 统计各自独立 |
| **R33.3 Pipeline 实例化 + Spec 配置** | `RenderPipeline` 从 `s_Data` 下移到 `SceneRenderer`；每个 `SceneRenderer` 按 `SceneRendererSpec` 决定构造哪些 Pass | Game 面板关闭 Picking/Outline/Debug Pass 后显存显著下降；缩略图/反射探针场景可关闭 Shadow 完全省阴影开销 |

---

## 2. 总体架构

### 2.1 分层图

```
┌────────────────────────────────────────────────────────────────┐
│  应用/编辑器层                                                    │
│  ┌────────────────────────┐    ┌────────────────────────┐       │
│  │ SceneViewportPanel     │    │ GameViewportPanel      │       │
│  │  Ref<SceneRenderer>    │    │  Ref<SceneRenderer>    │  ...  │
│  │  Ref<Framebuffer>      │    │  Ref<Framebuffer>      │       │
│  └───────────┬────────────┘    └───────────┬────────────┘       │
│              │                             │                     │
├──────────────┼─────────────────────────────┼─────────────────────┤
│              ?                             ?                     │
│  ┌────────────────────────────────────────────────────┐          │
│  │  SceneRenderer （可实例化）                         │          │
│  │  持有：                                             │          │
│  │    - Framebuffer（目标 FBO）                        │          │
│  │    - RenderPipeline （独立 Pass 组合）              │          │
│  │    - CameraUBO / LightUBO                          │          │
│  │    - DrawCommand 队列（Opaque/Transparent/Sprite/  │          │
│  │      Outline）                                     │          │
│  │    - Stats                                         │          │
│  │    - Outline / PostProcess / Shadow / Environment  │          │
│  │      当前渲染值                                     │          │
│  │  API：                                              │          │
│  │    Init(spec)                                      │          │
│  │    OnViewportResize(w, h)                          │          │
│  │    BeginScene(cam, light)                          │          │
│  │    SubmitMesh / SubmitSprite                       │          │
│  │    EndScene()                                      │          │
│  │    RenderOutline()                                 │          │
│  │    Set*（PostProcessSettings / EnvironmentSettings │          │
│  │          / OutlineEntities / OutlineColor / ...）  │          │
│  │    GetFinalColorAttachmentID()                     │          │
│  │    GetStats()                                      │          │
│  └────────────────┬───────────────────────────────────┘          │
│                   │                                              │
│                   │ 调用全局共享资源                              │
│                   ?                                              │
│  ┌────────────────────────────────────────────────────┐          │
│  │  Renderer3D （全局静态，收缩为共享资源提供者）      │          │
│  │    - Init / Shutdown                               │          │
│  │    - GetShaderLibrary                              │          │
│  │    - GetDefaultMaterial / GetInternalErrorMaterial │          │
│  │    - GetSkyboxMaterial（默认）                      │          │
│  │    - GetDefaultTexture                             │          │
│  └────────────────────────────────────────────────────┘          │
│                                                                  │
│  ┌────────────────────────────────────────────────────┐          │
│  │  IBLPrecompute （全局静态，保持不动，作为场景级共享）│          │
│  │  RenderPass / RenderPipeline / RenderContext       │          │
│  │  RenderCommand / Shader / Material / Framebuffer / │          │
│  │  UniformBuffer / Buffer / VertexArray / Texture    │          │
│  └────────────────────────────────────────────────────┘          │
└──────────────────────────────────────────────────────────────────┘
```

### 2.2 数据流

```
每帧、每个 SceneRenderer：
  Panel.OnUpdate:
    1) sceneRenderer.OnViewportResize(w, h)         (若尺寸变化)
    2) sceneRenderer.SetPostProcessSettings(...)    (来自 Scene 收集的 Volume)
       sceneRenderer.SetEnvironmentSettings(...)    (来自 Scene 的 EnvironmentSettings)
       sceneRenderer.SetOutlineEntities/Color(...)  (来自 Panel 的选中集合)
    3) Scene.OnRenderEditor(editorCam, sceneRenderer)
        or Scene.OnRenderRuntime(sceneRenderer):
         - 遍历 ECS 收集 LightRenderData / CameraRenderData
         - sceneRenderer.BeginScene(cam, lightData)
         - 遍历 MeshFilter/MeshRenderer/Sprite → SubmitMesh/SubmitSprite
         - sceneRenderer.EndScene()
    4) sceneRenderer.RenderOutline()  (可选)
    5) ImGui::Image( sceneRenderer.GetFinalColorAttachmentID(), ... )
```

**关键约束**：`Scene` 层**不感知 Renderer3D 全局单例**，只通过传入的 `SceneRenderer&` 提交 Draw Command。

---

## 3. 关键设计决策

### 3.1 Framebuffer 归属

**推荐方案**：SceneRenderer 内部持有 `m_Framebuffer`，由 `SceneRendererSpec` 决定附件规格。

- **方案 A（推荐）**：`SceneRenderer` 内部持有，`SceneRendererSpec::EnablePicking` 决定是否附加 `RED_INTEGER` 附件
  - **优点**：Panel 完全不感知 FBO 规格差异；Scene / Game 面板天然使用不同 FBO 布局
  - **缺点**：无（当前 Panel 已经在做 FBO 创建，工作量转移而非增加）
- **方案 B**：Panel 持有 FBO 传给 SceneRenderer
  - **优点**：改造小
  - **缺点**：Panel 依然感知 FBO 规格细节；Panel 越权
- **方案 C**：SceneRenderer 内部持有，同时提供 `SetOverrideTargetFramebuffer` 逃生舱
  - **优点**：为将来"资产缩略图渲染到指定 FBO"预留能力
  - **缺点**：增加接口复杂度，且当前没有具体需求

**决策**：**采用方案 A**。逃生舱等真正有缩略图需求时再加，遵循 YAGNI。

### 3.2 Renderer3D 收缩程度

**推荐方案**：只保留全局共享资源与 `Init/Shutdown`，删除所有渲染状态和 `BeginScene/DrawXxx/SetXxx/GetPipeline` API。

- **方案 A（推荐）**：彻底收缩，`s_Data` 只剩 Shader/Material/Texture 共享资源
  - **优点**：分层最清晰；所有渲染状态都在 SceneRenderer；无隐式全局状态
  - **缺点**：改造面较大（但集中在 R33.2 一步完成）
- **方案 B**：保留部分 UBO/Pipeline 在 Renderer3D，通过"上下文推送"共享
  - **优点**：改造小
  - **缺点**：本质是多个 SceneRenderer 抢用一个全局 UBO；分层不清；未来加多相机时更痛苦

**决策**：**采用方案 A**。

### 3.3 RenderPass 实例归属

**推荐方案**：每个 SceneRenderer 完全独立持有所有 Pass 实例（含 ShadowPass 及其内部 Shadow Map / ShadowAtlas 贴图）。用 `SceneRendererSpec::EnableShadow` 让不需要阴影的用途（缩略图、反射探针）直接关掉。

- **方案 A（推荐）**：每个 SceneRenderer 完全独立持有所有 Pass 实例
  - **优点**：状态完全隔离；架构最简单、最正交；对齐 Unity URP 主流做法（Unity 每相机独立执行 `MainLightShadowCasterPass` / `AdditionalLightsShadowCasterPass`）
  - **缺点**：Scene + Game 面板同开时 Shadow 显存翻倍（约 +208 MB，估算见下）
- **方案 B**：Pass 实例共享，通过 RenderContext 传入不同上下文
  - **优点**：显存最省
  - **缺点**：Pass 内部的 FBO / Shader 编译状态难以剥离；`PostProcessPass` 的 HDR FBO 必须每 SceneRenderer 独立（否则两个面板会互相 clear/write），共享无法彻底
- **方案 C**：混合??**Pass 实例独立，但 Shadow 贴图存储共享**
  - `ShadowPass` 独立（每相机的 CSM cascade split 依赖相机视锥），但贴图存储抽出到 `SharedShadowResources`
  - **优点**：Shadow 显存不翻倍
  - **缺点**：引入 `SharedShadowResources` 特殊抽象层，架构非正交；**Shadow draw 依然要每 SceneRenderer 各跑一次**（只省显存，不省 CPU/GPU）；Unity 都不这么做??非主流优化

**决策**：**采用方案 A**。理由：

1. **Unity URP 就是这么做的**：Unity 每相机独立跑 Shadow Pass，官方文档也明确警告"多相机会导致 culling / light processing / shadow rendering 各跑一次"，把这当成用户自觉的性能权衡
2. **Shadow 显存代价可承受**：Luck3D 当前设置下单实例 Shadow 显存约 208 MB（CSM 64 + Translucent 80 + Atlas 64），两个面板同开 +208 MB，编辑器场景（PC 8GB 显存起）完全能承受
3. **架构正交才是长期收益**：`SharedShadowResources` 只解决显存单一维度，代价是引入一个特殊层，与"SceneRenderer 完全实例化"的整体方向矛盾
4. **提供逃生舱**：`SceneRendererSpec::EnableShadow = false` 让缩略图 / 反射探针烘焙场景直接不构造 ShadowPass，省掉 100% Shadow 显存 + CPU/GPU 时间

### 3.3.1 Shadow 显存开销估算

以 Luck3D 当前默认设置计算单个 SceneRenderer 的 Shadow 显存：

| 资源 | 尺寸 | 格式 | 显存 |
|---|---|---|---|
| CSM Texture2DArray（4 级联，2048×2048） | 2048 × 2048 × 4 | Depth24 | ≈ 64 MB |
| Translucent Shadow Map Array | 2048 × 2048 × 4 | RGBA8 + Depth | ≈ 80 MB |
| Shadow Atlas（聚光/点光共用） | 4096 × 4096 × 1 | Depth24 | ≈ 64 MB |
| **单 SceneRenderer 阴影合计** | | | **≈ 208 MB** |

**Scene + Game 面板同开的 Shadow 显存增量**：约 208 MB。可用 `EnableShadow = false` 在不需要阴影的场景完全回避。

**Shadow 渲染的 CPU/GPU 成本**：CSM 4 级联 = 4 次全场景 DrawCall；4 个聚光灯 + 1 个点光源 = 10 次全场景 DrawCall。Scene + Game 同开时这些 draw 每帧跑 2 遍??**这个成本比显存更值得关注**，也是选择 `EnableShadow` 逃生舱的另一个理由。

### 3.4 Pass 组合配置

**推荐方案**：`SceneRendererSpec` 用布尔开关表达可选 Pass。

- **方案 A（推荐）**：布尔开关（`EnablePicking / EnableOutline / EnableDebugVisualize / EnablePostProcess`）
  - **优点**：直观；扩展新维度只加字段；Panel 端配置一目了然
  - **缺点**：无
- **方案 B**：预设 enum（`Preset::SceneView / GameView / Thumbnail / ReflectionProbe`）
  - **优点**：语义清晰
  - **缺点**：新增用途需改 enum；灵活性差

**决策**：**采用方案 A**。

### 3.5 改造顺序

**推荐方案**：R33.1 → R33.2 → R33.3 增量迁移，每阶段独立可验证。

- **方案 A**：一次性重构
  - **缺点**：改动面过大，风险高
- **方案 B（推荐）**：三阶段增量
  - R33.1：新 API 骨架（转发到旧全局实现），Panel 迁移到新 API
  - R33.2：状态字段下移，`s_Data` 瘦身
  - R33.3：Pipeline 实例化 + Spec 配置
  - **优点**：每阶段独立可回滚，画面无回归可持续验证

**决策**：**采用方案 B**。

### 3.6 Scene 与 SceneRenderer 的接口

**推荐方案**：Scene 的渲染方法显式接收 `SceneRenderer&`，遍历 ECS 后调用 `renderer.SubmitMesh/SubmitSprite`。

- **方案 A（推荐）**：`Scene::OnRenderEditor(EditorCamera&, SceneRenderer&)` / `Scene::OnRenderRuntime(SceneRenderer&)`
  - **优点**：显式依赖，测试易；不依赖全局单例
  - **缺点**：Scene 层需要包含 `SceneRenderer` 头
- **方案 B**：Scene 只输出 `RenderView`（相机+光源+DrawCommand 列表），Panel 侧把它喂给 SceneRenderer
  - **优点**：Scene 完全解耦 SceneRenderer
  - **缺点**：多一层数据结构；Sprite/Mesh 都要序列化到中间层，与现状差异大

**决策**：**采用方案 A**。

### 3.7 Renderer2D / Sprite 提交路径

**推荐方案**：Sprite 走 SceneRenderer 队列，`Sprite2DPass` 内部继续使用 `Renderer2D` 全局单例 flush，本 Phase 不动 `Renderer2D`。

- **方案 A（推荐）**：`SceneRenderer::SubmitSprite` → 存到实例的 `SpriteDrawCommands` → `Sprite2DPass` 从 `RenderContext` 拿到 → 内部走 `Renderer2D::BeginScene/DrawSprite/EndScene`
  - **优点**：保持现状；`Renderer2D` 独立重构留给下一 Phase
  - **缺点**：`Renderer2D` 仍是全局单例，暂时存在"两个 SceneRenderer 交替 BeginScene 到同一个 Renderer2D"的行为??因为串行执行，实际无冲突
- **方案 B**：本 Phase 同步把 `Renderer2D` 实例化
  - **优点**：一次到位
  - **缺点**：改造面翻倍，本 Phase 目标失焦

**决策**：采用**方案 A**。

### 3.8 全文对"因性能引入的特殊处理"的审查结果

本次设计自检了全文，将一切"为了性能而引入的特殊抽象 / 不优雅方案"目录化处理：

| # | 候选方案 | 性能代价 | 最终决策 |
|---|---|---|---|
| 1 | `SharedShadowResources` 共享阴影贴图 | 多 SceneRenderer 阴影显存翻倍（~208 MB） | **删除**。接受显存代价，与 Unity URP 对齐；需要时用 `EnableShadow = false` 逃生 |
| 2 | `IBLPrecompute` 内部按 Cubemap ID 去重缓存 | 避免多 SceneRenderer 重复触发 IBL 生成 | **删除**。把 IBL 触发从 SceneRenderer 上移到 Scene，从根本上避免重复 |
| 3 | R33.2 阶段 HDR FBO 共用（"串行安全"） | 中间 HDR 内容不能跨帧保留 | **保留**。R33.2 过渡阶段的已知限制，R33.3 Pipeline 下移后自然消失 |
| 4 | `Renderer2D` 全局单例（"串行安全"） | Sprite 路径仍有全局依赖 | **保留**。本 Phase 明确不动 Renderer2D，独立 Phase 修 |
| 5 | Scene / Game 面板 aspect 写竞争（都调 `OnViewportResize`） | 相机 aspect 以最后 resize 者为准 | **保留（不属本 Phase）**。这是 CameraComponent 职责问题，待专项处理 |
| 6 | `Stats` 重置归属争议 | 多个多面板时重置时机不确定 | **地归 SceneRenderer::BeginScene 首行自动重置**，Panel / EditorLayer 不感知 |

**原则**：宁可接受可预测的性能代价（与 Unity URP 一致），不为了伪优化引入特殊抽象层。真正需要优化时提供逃生舱（如 `SceneRendererSpec::EnableShadow`），而非在架构层预埋不对称。

---

## 4. Renderer3D 收缩后的完整接口

### 4.1 头文件（`Renderer3D.h` 收缩后）

```cpp
#pragma once

#include "Texture.h"
#include "Material.h"

namespace Lucky
{
    class ShaderLibrary;

    /// <summary>
    /// 默认纹理类型
    /// </summary>
    enum class TextureDefault;

    /// <summary>
    /// 全局渲染服务：提供跨 SceneRenderer 共享的资源与初始化
    /// 具体的场景渲染由 SceneRenderer 实例承担
    /// </summary>
    class Renderer3D
    {
    public:
        /// <summary>
        /// 全局初始化：加载 ShaderLibrary、默认材质、默认纹理、IBLPrecompute
        /// 在 Application 启动、创建任何 SceneRenderer 之前调用一次
        /// </summary>
        static void Init();

        /// <summary>
        /// 全局释放：释放 ShaderLibrary、默认材质、IBLPrecompute
        /// 在 Application 关闭、所有 SceneRenderer 析构之后调用一次
        /// </summary>
        static void Shutdown();

        static Ref<ShaderLibrary>& GetShaderLibrary();
        static Ref<Material>& GetInternalErrorMaterial();
        static Ref<Material>& GetDefaultMaterial();

        /// <summary>
        /// 获取内置默认天空盒材质（初始化时从 Assets/Textures/Skybox 加载）
        /// 用户新场景会以此作为默认 EnvironmentSettings::SkyboxMaterial
        /// </summary>
        static Ref<Material>& GetDefaultSkyboxMaterial();

        static const Ref<Texture2D>& GetDefaultTexture(TextureDefault type);
    };
}
```

**关键变化**：

- **删除**：`BeginScene / EndScene / DrawMesh / DrawSprite`（迁移到 `SceneRenderer`）
- **删除**：`SetTargetFramebuffer / SetClearColor / SetOutlineEntities / SetOutlineColor / ResizePipeline / RenderOutline / GetPipeline / SetPostProcessSettings / SetEnvironmentSettings`（全部迁移到 `SceneRenderer`）
- **删除**：`GetStats / ResetStats`（Stats 下移到 SceneRenderer；EditorLayer 需要汇总时遍历所有 SceneRenderer 求和）
- **删除**：`Statistics` 结构体（迁移到 `SceneRenderer::Statistics`）
- **保留**：`Init / Shutdown / GetShaderLibrary / GetInternalErrorMaterial / GetDefaultMaterial / GetDefaultTexture`
- **重命名**：`GetSkyboxMaterial` → `GetDefaultSkyboxMaterial`（语义精确化：这是"默认那份"，非"当前场景的"）；同时删除 `SetSkyboxMaterial`??设置当前场景天空盒通过 `EnvironmentSettings` 传给 SceneRenderer

### 4.2 相关数据结构去向

| 原 `Renderer3D.h` 中的定义 | 去向 |
|---|---|
| `DirectionalLightData / PointLightData / SpotLightData / LightRenderData` | 保留在 `Renderer3D.h` 或抽出到 `LightRenderData.h`（**推荐：抽出**，与 Renderer3D 解耦） |
| `CameraRenderData` | 保留在独立头文件 `CameraRenderData.h`（Scene / SceneRenderer 都用） |
| `Statistics` | 迁移到 `SceneRenderer.h`（每实例统计） |
| `s_MaxDirectionalLights / s_MaxPointLights / s_MaxSpotLights / s_MaxCascadeCount` | 保留在 `LightRenderData.h`（与常量归属对齐） |

**推荐**：新建两个头文件 `Lucky/Source/Lucky/Renderer/LightRenderData.h`、`Lucky/Source/Lucky/Renderer/CameraRenderData.h`。这样 `Renderer3D.h` 只剩全局服务接口。

**方案权衡**：
- 方案 A（推荐）：新建两个头文件独立承载，`Renderer3D.h` 只剩全局服务
  - **优点**：职责清晰；Scene 层 include `CameraRenderData.h` 而非 `Renderer3D.h`
  - **缺点**：新增两个文件
- 方案 B：全部留在 `Renderer3D.h`
  - **优点**：不新增文件
  - **缺点**：`Renderer3D.h` 混杂全局服务和数据结构

---

## 5. SceneRenderer 的完整接口

### 5.1 头文件设计

**文件**：`Lucky/Source/Lucky/Renderer/SceneRenderer.h`

```cpp
#pragma once

#include "Framebuffer.h"
#include "RenderPipeline.h"
#include "UniformBuffer.h"
#include "CameraRenderData.h"
#include "LightRenderData.h"

#include "Lucky/Scene/Components/LightComponent.h"

#include <glm/glm.hpp>

#include <unordered_set>
#include <vector>

namespace Lucky
{
    struct DrawCommand;
    struct OutlineDrawCommand;
    struct SpriteDrawCommand;
    struct PostProcessSettings;
    struct EnvironmentSettings;

    /// <summary>
    /// SceneRenderer 构造参数：决定 FBO 附件与 Pass 组合
    /// </summary>
    struct SceneRendererSpec
    {
        uint32_t Width = 1280;                  // 初始视口宽
        uint32_t Height = 720;                  // 初始视口高

    bool EnableShadow = true;               // 启用 ShadowPass（含 CSM + ShadowAtlas，占约 208MB 显存）
    bool EnablePicking = false;             // 附加 RED_INTEGER 拾取附件；启用 PickingPass
    bool EnableOutline = false;             // 启用 SilhouettePass + OutlineCompositePass；RenderOutline() 可用
    bool EnableDebugVisualize = false;      // 启用 DebugVisualizePass（CSM 级联可视化等）
    bool EnablePostProcess = true;          // 启用 PostProcessPass（HDR FBO + Tonemapping + Bloom/FXAA/Vignette）
};

/// <summary>
/// 场景渲染器：可实例化的渲染管线执行器
/// 每个视口面板持有一个独立实例；持有完整的一次渲染所需状态
/// </summary>
class SceneRenderer
    {
    public:
        struct Statistics
        {
            uint32_t DrawCalls = 0;
            uint32_t TriangleCount = 0;

            uint32_t GetTotalVertexCount() const { return TriangleCount * 3; }
            uint32_t GetTotalIndexCount() const { return TriangleCount * 6; }
        };

        SceneRenderer() = default;
        ~SceneRenderer();

        /// <summary>
        /// 初始化：创建 Framebuffer、RenderPipeline、UBO；按 Spec 添加 Pass
        /// 必须在 Renderer3D::Init() 之后、任何 Begin/EndScene 之前调用
        /// </summary>
        void Init(const SceneRendererSpec& spec);

        /// <summary>
        /// 释放所有资源
        /// </summary>
        void Shutdown();

        /// <summary>
        /// 视口大小变更时调用：Resize m_Framebuffer 及 Pipeline 内各 Pass 的内部 FBO
        /// </summary>
        void OnViewportResize(uint32_t width, uint32_t height);

        // ---- 帧渲染 ----

        /// <summary>
        /// 开始一次场景渲染：写入 UBO、计算 CSM cascade 与点光/聚光矩阵、清空 DrawCommand 队列
        /// </summary>
        void BeginScene(const CameraRenderData& cam, const LightRenderData& lightData);

        /// <summary>
        /// 提交一次 Mesh 绘制到本 SceneRenderer 的 DrawCommand 队列
        /// </summary>
        void SubmitMesh(const glm::mat4& transform, Ref<Mesh>& mesh, const std::vector<Ref<Material>>& materials, int entityID = -1);

        /// <summary>
        /// 提交一次 Sprite 绘制到本 SceneRenderer 的 SpriteDrawCommand 队列
        /// </summary>
        void SubmitSprite(const glm::mat4& transform,
                          const Ref<Texture2D>& texture,
                          const glm::vec4& color,
                          bool flipX,
                          bool flipY,
                          const glm::vec4& uvRect,
                          float tilingFactor,
                          const Ref<Material>& material,
                          int sortingOrder,
                          int entityID = -1);

        /// <summary>
        /// 结束一次场景渲染：排序 DrawCommands、构建 RenderContext、执行 Shadow/Main/Debug/PostProcess 四个 Pass 分组
        /// EndScene 后 DrawCommand 队列被清空；描边所需的最小几何被提取到 m_OutlineDrawCommands
        /// </summary>
        void EndScene();

        /// <summary>
        /// 执行 Outline 分组（SilhouettePass + OutlineCompositePass）
        /// 仅当 Spec.EnableOutline == true 时可用；无 outline entities 时也可安全调用（会 no-op）
        /// 通常由 Panel 在 Gizmo 绘制之后调用，确保描边覆盖在 Gizmo 之上
        /// </summary>
        void RenderOutline();

        // ---- 每帧输入（Panel / Scene 设置） ----

        void SetClearColor(const glm::vec4& color) { m_ClearColor = color; }

        /// <summary>
        /// 设置需要描边的实体 ID 集合（空集合 = 无选中）
        /// </summary>
        void SetOutlineEntities(const std::unordered_set<int>& entityIDs) { m_OutlineEntityIDs = entityIDs; }
        void SetOutlineColor(const glm::vec4& color) { m_OutlineColor = color; }

        /// <summary>
        /// 设置后处理参数（Scene 每帧从 PostProcessVolume 收集后调用）
        /// 内部会同步到 PostProcessPass 的 PostProcessStack 中各 Effect
        /// </summary>
        void SetPostProcessSettings(const PostProcessSettings& settings);

        /// <summary>
        /// 设置环境设置（Scene 每帧调用）
        /// SceneRenderer 只存一份环境参数的副本供当帧 IBL 采样使用；不在此处触发 IBL 重生成（见 §7.5.3）
        /// </summary>
        void SetEnvironmentSettings(const EnvironmentSettings& settings);

        // ---- 只读访问 ----

        const Ref<Framebuffer>& GetFramebuffer() const { return m_Framebuffer; }

        /// <summary>
        /// 获取最终可显示的颜色附件纹理 ID（供 ImGui::Image 使用）
        /// 等价于 m_Framebuffer->GetColorAttachmentRendererID(0)
        /// </summary>
        uint32_t GetFinalColorAttachmentID() const;

        /// <summary>
        /// 获取 EntityID 拾取附件纹理 ID（仅 Spec.EnablePicking == true 时可用）
        /// 供 Scene 面板 OnMouseButtonPressed 内读取像素做实体拾取
        /// </summary>
        int ReadPixelEntityID(int x, int y) const;

        const RenderPipeline& GetPipeline() const { return m_Pipeline; }
        RenderPipeline& GetPipeline() { return m_Pipeline; }

        const Statistics& GetStats() const { return m_Stats; }
        void ResetStats() { m_Stats = {}; }

    private:
        /// <summary>
        /// 按 Spec 构造 Pass 组合，写入 m_Pipeline
        /// </summary>
        void BuildPipeline();

        /// <summary>
        /// 从 DrawCommands 中提取选中实体到 m_OutlineDrawCommands
        /// 在 EndScene() 末尾调用，为后续 RenderOutline() 做准备
        /// </summary>
        void ExtractOutlineDrawCommands();

    private:
        SceneRendererSpec m_Spec;

        Ref<Framebuffer> m_Framebuffer;                 // 目标 FBO（内部持有）
        RenderPipeline m_Pipeline;                      // 独立的 Pass 组合

        // ---- UBO（每实例独立） ----
        Ref<UniformBuffer> m_CameraUniformBuffer;
        Ref<UniformBuffer> m_LightUniformBuffer;
        CameraUBOData m_CameraBuffer;                   // 详细定义见 §5.2
        LightUBOData m_LightBuffer;

        // ---- 相机缓存 ----
        glm::vec3 m_CameraPosition{ 0.0f };
        glm::mat4 m_CameraViewMatrix{ 1.0f };
        glm::mat4 m_CameraProjectionMatrix{ 1.0f };

        // ---- DrawCommand 队列（每帧构建、EndScene 清空） ----
        std::vector<DrawCommand> m_OpaqueDrawCommands;
        std::vector<DrawCommand> m_TransparentDrawCommands;
        std::vector<SpriteDrawCommand> m_SpriteDrawCommands;
        std::vector<OutlineDrawCommand> m_OutlineDrawCommands;

        // ---- Outline 参数 ----
        std::unordered_set<int> m_OutlineEntityIDs;
        glm::vec4 m_OutlineColor = glm::vec4(1.0f, 0.4f, 0.0f, 1.0f);
        float m_OutlineWidth = 2.0f;

        // ---- 清屏色 ----
        glm::vec4 m_ClearColor = glm::vec4(0.0f, 0.0f, 0.0f, 1.0f);

        // ---- 阴影参数（BeginScene 中从 LightRenderData 计算） ----
        bool m_ShadowEnabled = false;
        float m_ShadowBias = 0.005f;
        float m_ShadowStrength = 1.0f;
        ShadowType m_ShadowShadowType = ShadowType::None;

        int m_CascadeCount = 4;
        glm::mat4 m_CascadeLightSpaceMatrices[s_MaxCascadeCount];
        float m_CascadeFarPlanes[s_MaxCascadeCount] = { 0.0f };
        int m_ShadowMapResolution = 2048;

        // 聚光灯 / 点光源 阴影缓存（结构与原 Renderer3DData::SpotShadowCacheData 一致）
        // 详细定义见 §5.3
        SpotShadowCacheData m_SpotShadowData[ShadowAtlas::s_MaxSpotLightShadows];
        int m_SpotShadowCount = 0;
        PointShadowCacheData m_PointShadowData[ShadowAtlas::s_MaxPointLightShadows];
        int m_PointShadowCount = 0;

        // ---- 后处理与环境（当前渲染当次使用的副本） ----
        PostProcessSettings m_PostProcess;
        EnvironmentSettings m_Environment;

        // ---- 统计 ----
        Statistics m_Stats;
    };
}
```

### 5.2 内部数据结构

原 `Renderer3DData::CameraUBOData` / `LightUBOData` 从 `.cpp` 内部 struct 提升到 `SceneRenderer.h`（或独立小头文件 `SceneRendererUBOData.h`）。字段与内存布局与现状**完全一致**，仅是持有位置从全局 `s_Data` 变为实例成员。

```cpp
struct CameraUBOData
{
    glm::mat4 ViewProjectionMatrix;
    glm::mat4 InvProjectionMatrix;
    glm::vec3 Position;
    char padding[4];
};

struct LightUBOData
{
    int DirectionalLightCount;
    int PointLightCount;
    int SpotLightCount;
    char padding[4];

    DirectionalLightData DirectionalLights[s_MaxDirectionalLights];
    PointLightData PointLights[s_MaxPointLights];
    SpotLightData SpotLights[s_MaxSpotLights];
};
```

### 5.3 阴影缓存结构

同样从原 `Renderer3DData` 中提升上来（放到 `SceneRenderer.h` 或独立小头文件）。字段与现状一致：

```cpp
struct SpotShadowCacheData
{
    int LightIndex = -1;
    glm::mat4 LightSpaceMatrix = glm::mat4(1.0f);
    float ShadowBias = 0.001f;
    float ShadowStrength = 1.0f;
    int ShadowType = 1;
};

struct PointShadowCacheData
{
    int LightIndex = -1;
    glm::vec3 LightPos = glm::vec3(0.0f);
    float FarPlane = 25.0f;
    glm::mat4 LightSpaceMatrices[6];
    float ShadowBias = 0.05f;
    float ShadowStrength = 1.0f;
    int ShadowType = 1;
};
```

### 5.4 生命周期时序

```
Application::Init
  Renderer3D::Init                           (加载 ShaderLibrary / 默认材质 / 默认纹理 / IBL)
  ...

EditorLayer::OnAttach
  m_PanelManager->AddPanel<SceneViewportPanel>(...)
    SceneViewportPanel ctor
      m_SceneRenderer = CreateRef<SceneRenderer>()
      SceneRendererSpec spec { EnableShadow, EnablePicking, EnableOutline, EnableDebugVisualize, EnablePostProcess = true }
      m_SceneRenderer->Init(spec)            (→ 创建 FBO / UBO / Pipeline / 按 Spec AddPass)
  m_PanelManager->AddPanel<GameViewportPanel>(...)
    GameViewportPanel ctor
      SceneRendererSpec spec { EnableShadow = true, EnablePostProcess = true, 其余 = false }
      m_SceneRenderer->Init(spec)

（运行时每帧）
  Panel::OnUpdate
    m_SceneRenderer->OnViewportResize(w, h) 若尺寸变化
    m_Framebuffer->Bind() ... m_SceneRenderer->SetClearColor(...) ...
    Scene->OnRenderEditor(editorCam, *m_SceneRenderer)
      → 内部 Submit* → m_SceneRenderer->EndScene()
    m_SceneRenderer->RenderOutline() (若 Spec.EnableOutline)

EditorLayer::OnDetach
  m_PanelManager 析构 → Panel 析构 → SceneRenderer 析构 → Shutdown()

Application::Shutdown
  Renderer3D::Shutdown
```

---

## 6. R33.1 骨架 + 转发

### 6.1 目标

- 引入 `SceneRenderer` 类，**成员只有 `SceneRendererSpec m_Spec` 和 `Ref<Framebuffer> m_Framebuffer`**
- `SceneRenderer` 所有渲染方法**转发到 `Renderer3D` 全局静态实现**
- Panel 改造为持有 `Ref<SceneRenderer>`
- Scene 的渲染方法增加 `SceneRenderer&` 参数
- **画面完全等价，行为无回归**

### 6.2 逐文件改动清单

#### 6.2.1 新建 `Lucky/Source/Lucky/Renderer/LightRenderData.h`

将 `Renderer3D.h` 中的 `DirectionalLightData / PointLightData / SpotLightData / LightRenderData` 及 `s_MaxDirectionalLights` 等常量整体搬出。**内容零改动，只是换文件**。

`Renderer3D.h` 保留 `#include "LightRenderData.h"` 转发（R33 全部完成后可删除）。

#### 6.2.2 新建 `Lucky/Source/Lucky/Renderer/CameraRenderData.h`

将 `Renderer3D.h` 中的 `CameraRenderData` 结构体搬出。**内容零改动**。

#### 6.2.3 新建 `Lucky/Source/Lucky/Renderer/SceneRenderer.h`

R33.1 版本仅暴露：

```cpp
#pragma once

#include "Framebuffer.h"
#include "CameraRenderData.h"
#include "LightRenderData.h"
#include "RenderContext.h"    // 供 PostProcessSettings / EnvironmentSettings 转发

namespace Lucky
{
    struct SceneRendererSpec
    {
        uint32_t Width = 1280;
        uint32_t Height = 720;
        bool EnableShadow = true;
        bool EnablePicking = false;
        bool EnableOutline = false;
        bool EnableDebugVisualize = false;
        bool EnablePostProcess = true;
    };

    class SceneRenderer
    {
    public:
        struct Statistics
        {
            uint32_t DrawCalls = 0;
            uint32_t TriangleCount = 0;

            uint32_t GetTotalVertexCount() const { return TriangleCount * 3; }
            uint32_t GetTotalIndexCount() const { return TriangleCount * 6; }
        };

        SceneRenderer() = default;
        ~SceneRenderer() = default;

        void Init(const SceneRendererSpec& spec);
        void Shutdown();

        void OnViewportResize(uint32_t width, uint32_t height);

        void BeginScene(const CameraRenderData& cam, const LightRenderData& lightData);
        void SubmitMesh(const glm::mat4& transform, Ref<Mesh>& mesh, const std::vector<Ref<Material>>& materials, int entityID = -1);
        void SubmitSprite(const glm::mat4& transform,
                          const Ref<Texture2D>& texture,
                          const glm::vec4& color,
                          bool flipX,
                          bool flipY,
                          const glm::vec4& uvRect,
                          float tilingFactor,
                          const Ref<Material>& material,
                          int sortingOrder,
                          int entityID = -1);
        void EndScene();
        void RenderOutline();

        void SetClearColor(const glm::vec4& color);
        void SetOutlineEntities(const std::unordered_set<int>& entityIDs);
        void SetOutlineColor(const glm::vec4& color);
        void SetPostProcessSettings(const PostProcessSettings& settings);
        void SetEnvironmentSettings(const EnvironmentSettings& settings);

        const Ref<Framebuffer>& GetFramebuffer() const { return m_Framebuffer; }
        uint32_t GetFinalColorAttachmentID() const;
        int ReadPixelEntityID(int x, int y) const;

        Statistics GetStats() const;
        void ResetStats();

    private:
        SceneRendererSpec m_Spec;
        Ref<Framebuffer> m_Framebuffer;
    };
}
```

#### 6.2.4 新建 `Lucky/Source/Lucky/Renderer/SceneRenderer.cpp`

**R33.1 关键实现规则：所有方法转发到 `Renderer3D` 全局静态实现**，不做任何状态迁移。

示例：

```cpp
void SceneRenderer::Init(const SceneRendererSpec& spec)
{
    m_Spec = spec;

    FramebufferSpecification fbSpec;
    if (m_Spec.EnablePicking)
    {
        fbSpec.Attachments =
        {
            FramebufferTextureFormat::RGBA8,
            FramebufferTextureFormat::RED_INTEGER,
            FramebufferTextureFormat::Depth
        };
    }
    else
    {
        fbSpec.Attachments =
        {
            FramebufferTextureFormat::RGBA8,
            FramebufferTextureFormat::Depth
        };
    }
    fbSpec.Width = m_Spec.Width;
    fbSpec.Height = m_Spec.Height;

    m_Framebuffer = Framebuffer::Create(fbSpec);
}

void SceneRenderer::Shutdown()
{
    m_Framebuffer.reset();
}

void SceneRenderer::OnViewportResize(uint32_t width, uint32_t height)
{
    m_Framebuffer->Resize(width, height);
    Renderer3D::ResizePipeline(width, height);        // R33.1 阶段依然走全局
}

void SceneRenderer::BeginScene(const CameraRenderData& cam, const LightRenderData& lightData)
{
    Renderer3D::SetTargetFramebuffer(m_Framebuffer);  // R33.1 阶段每次覆盖全局
    Renderer3D::BeginScene(cam, lightData);
}

void SceneRenderer::SubmitMesh(const glm::mat4& transform, Ref<Mesh>& mesh,
                               const std::vector<Ref<Material>>& materials, int entityID)
{
    Renderer3D::DrawMesh(transform, mesh, materials, entityID);
}

void SceneRenderer::EndScene()
{
    Renderer3D::EndScene();
}

void SceneRenderer::RenderOutline()
{
    if (!m_Spec.EnableOutline)
    {
        return;
    }
    Renderer3D::RenderOutline();
}

void SceneRenderer::SetClearColor(const glm::vec4& color)
{
    Renderer3D::SetClearColor(color);
}

void SceneRenderer::SetOutlineEntities(const std::unordered_set<int>& entityIDs)
{
    Renderer3D::SetOutlineEntities(entityIDs);
}

void SceneRenderer::SetOutlineColor(const glm::vec4& color)
{
    Renderer3D::SetOutlineColor(color);
}

void SceneRenderer::SetPostProcessSettings(const PostProcessSettings& settings)
{
    Renderer3D::SetPostProcessSettings(settings);
}

void SceneRenderer::SetEnvironmentSettings(const EnvironmentSettings& settings)
{
    Renderer3D::SetEnvironmentSettings(settings);
}

uint32_t SceneRenderer::GetFinalColorAttachmentID() const
{
    return m_Framebuffer->GetColorAttachmentRendererID(0);
}

int SceneRenderer::ReadPixelEntityID(int x, int y) const
{
    if (!m_Spec.EnablePicking)
    {
        return -1;
    }
    m_Framebuffer->Bind();
    int pixel = m_Framebuffer->GetPixel(1, x, y);
    m_Framebuffer->Unbind();
    return pixel;
}

SceneRenderer::Statistics SceneRenderer::GetStats() const
{
    // R33.1 阶段直接映射全局 Renderer3D 的统计
    auto s = Renderer3D::GetStats();
    Statistics stats;
    stats.DrawCalls = s.DrawCalls;
    stats.TriangleCount = s.TriangleCount;
    return stats;
}

void SceneRenderer::ResetStats()
{
    Renderer3D::ResetStats();
}
```

**注意**：R33.1 阶段 `SceneRenderer` 内部**不持有 UBO / Pipeline / DrawCommand 队列**，只持有 `m_Framebuffer + m_Spec`。所有状态仍在 `Renderer3D::s_Data`。

#### 6.2.5 修改 `Scene.h / Scene.cpp`

**签名变更**：

```cpp
// 变更前
void OnRenderEditor(EditorCamera& camera);
void OnRenderRuntime();
private:
    void RenderSceneImpl(const CameraRenderData& cam);

// 变更后
void OnRenderEditor(EditorCamera& camera, SceneRenderer& renderer);
void OnRenderRuntime(SceneRenderer& renderer);
private:
    void RenderSceneImpl(const CameraRenderData& cam, SceneRenderer& renderer);
```

`RenderSceneImpl` 内部原来的 `Renderer3D::BeginScene / DrawMesh / DrawSprite / SetPostProcessSettings / SetEnvironmentSettings / EndScene` 全部替换为 `renderer.BeginScene / SubmitMesh / SubmitSprite / SetPostProcessSettings / SetEnvironmentSettings / EndScene`。

`Scene.h` 需要 `#include "Lucky/Renderer/SceneRenderer.h"` 或前向声明 `class SceneRenderer;`（推荐前向声明，减少头文件传播）。

#### 6.2.6 修改 `SceneViewportPanel.h / .cpp`

`.h` 成员追加：

```cpp
Ref<SceneRenderer> m_SceneRenderer;
```

（`m_Framebuffer` 依然保留在 R33.1 阶段??它已由 SceneRenderer 内部持有，但为了减少 R33.1 的改动面，让 SceneRenderer 内部的 `m_Framebuffer` 和 Panel 的 `m_Framebuffer` **共用同一份**是不成立的。**建议 R33.1 就把 Panel 的 `m_Framebuffer` 删掉，改为 `m_SceneRenderer->GetFramebuffer()`**??这个改动是必要的，因为 SceneRenderer 内部会 `SetTargetFramebuffer(m_Framebuffer)`。）

`.cpp` ctor：

```cpp
SceneViewportPanel::SceneViewportPanel(const Ref<Scene>& scene)
    : m_Scene(scene),
      m_EditorCamera(30.0f, 1280.0f / 720.0f, 0.01f, 1000.0f)
{
    SetFlags(ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);

    SceneRendererSpec spec;
    spec.Width = 1280;
    spec.Height = 720;
    spec.EnableShadow = true;
    spec.EnablePicking = true;
    spec.EnableOutline = true;
    spec.EnableDebugVisualize = true;
    spec.EnablePostProcess = true;

    m_SceneRenderer = CreateRef<SceneRenderer>();
    m_SceneRenderer->Init(spec);

    m_SceneChangedSub = SceneManager::Subscribe([this](const Ref<Scene>& newScene)
    {
        SetScene(newScene);
    });
}
```

`.cpp OnUpdate` 中所有 `Renderer3D::SetXxx` 全部替换为 `m_SceneRenderer->SetXxx`；`m_Framebuffer->Bind()` 替换为 `m_SceneRenderer->GetFramebuffer()->Bind()`；`m_Scene->OnRenderEditor(m_EditorCamera)` 替换为 `m_Scene->OnRenderEditor(m_EditorCamera, *m_SceneRenderer)`；`Renderer3D::RenderOutline()` 替换为 `m_SceneRenderer->RenderOutline()`。

拾取逻辑用 `m_SceneRenderer->ReadPixelEntityID(x, y)` 替换原来读 HDR FBO 的代码??但要注意：**R33.1 阶段** PickingPass 依然渲染到全局 HDR FBO 的 Attachment 1，需要通过 `Renderer3D::GetPipeline().GetPass<PostProcessPass>()->GetHDR_FBO()` 拿到 HDR FBO 再读。这段代码保留原状即可（R33.3 才会真正让 HDR FBO 归 SceneRenderer 持有）。

#### 6.2.7 修改 `GameViewportPanel.h / .cpp`

同 SceneViewportPanel 的模式，但 `SceneRendererSpec` 设为：

```cpp
SceneRendererSpec spec;
spec.EnableShadow = true;
spec.EnablePicking = false;
spec.EnableOutline = false;
spec.EnableDebugVisualize = false;
spec.EnablePostProcess = true;
```

同时**清理 P0.6 引入的应急代码**：
- 删除 `Renderer3D::SetOutlineEntities({})`（Game 面板 SceneRenderer 天然不启用 Outline）
- **R33.1 阶段依然保留** `Renderer3D::SetTargetFramebuffer` 显式覆盖（因为 Scene / Game 面板还共用全局 Pipeline，需要覆盖）??**R33.2 完成后彻底删除**

`m_Scene->OnRenderRuntime()` 替换为 `m_Scene->OnRenderRuntime(*m_SceneRenderer)`。

#### 6.2.8 修改 `RenderPipelinePanel.cpp`

R33.1 阶段：**暂时保留** `Renderer3D::GetPipeline()`（因为 pipeline 还在全局）。R33.2 完成后，改为通过某种方式获取"当前活动 SceneRenderer"的 pipeline??见 §8.2 讨论。

`Renderer3D::GetStats()` 保留不动。

#### 6.2.9 修改 `EditorLayer.cpp`

`OnUpdate` 中的 `Renderer3D::ResetStats(); Renderer2D::ResetStats();` **R33.1 阶段保持不动**（Stats 依然全局）。R33.2 落地后再改为遍历 Panel 收集 SceneRenderer Stats 求和（或者由 Panel 自己在 OnUpdate 起始处 `m_SceneRenderer->ResetStats()`）。

### 6.3 R33.1 验收标准

1. 编译通过
2. Scene 面板 / Game 面板画面与 R33.1 之前完全等价（可比对截图）
3. Scene 面板的拾取、Gizmo、Outline、DebugVisualize、DragDrop 场景切换、CSM 阴影、后处理均正常
4. Game 面板行为不变（有 Primary Camera 时正常渲染、无 Primary Camera 时纯黑）
5. Renderer3D::s_Data 未发生任何字段变化（数据侧完全没动）
6. RenderPipelinePanel 显示 Stats / Passes 列表与之前一致

---

## 7. R33.2 状态字段下移

### 7.1 目标

将 `Renderer3D::s_Data` 中"每次渲染独立"的字段整体下移到 `SceneRenderer` 实例成员。**`Renderer3D` 内部不再持有 UBO / DrawCommands / Stats / Outline / Shadow / PostProcess / Environment / ClearColor 等**；但 `Pipeline` 暂留在 `s_Data`（R33.3 才下移）。

### 7.2 下移的字段清单（从 `Renderer3DData` 到 `SceneRenderer` 实例）

| 字段 | 类型 | 说明 |
|---|---|---|
| `CameraUniformBuffer / LightUniformBuffer` | `Ref<UniformBuffer>` | 每实例独立 |
| `CameraBuffer / LightBuffer` | `CameraUBOData / LightUBOData` | 每实例独立 |
| `OpaqueDrawCommands / TransparentDrawCommands / SpriteDrawCommands / OutlineDrawCommands` | `std::vector<...>` | 每实例独立 |
| `CameraPosition / CameraViewMatrix / CameraProjectionMatrix` | `glm::vec3 / glm::mat4` | 每实例独立 |
| `Stats` | `Statistics` | 每实例独立 |
| `TargetFramebuffer` | `Ref<Framebuffer>` | 已由 SceneRenderer 内部持有的 `m_Framebuffer` 承担；**从 s_Data 删除** |
| `OutlineEntityIDs / OutlineColor / OutlineWidth / OutlineEnabled` | (mixed) | 每实例独立 |
| `ClearColor` | `glm::vec4` | 每实例独立 |
| `ShadowEnabled / ShadowBias / ShadowStrength / ShadowShadowType` | (mixed) | 每实例独立 |
| `CascadeCount / CascadeLightSpaceMatrices / CascadeFarPlanes / ShadowMapResolution` | (mixed) | 每实例独立 |
| `SpotShadowData / SpotShadowCount / PointShadowData / PointShadowCount` | (mixed) | 每实例独立 |
| `PostProcess` | `PostProcessSettings` | 每实例独立 |
| `Environment` | `EnvironmentSettings` | 每实例独立 |

### 7.3 保留在 `Renderer3D::s_Data` 的字段

| 字段 | 类型 | 说明 |
|---|---|---|
| `ShaderLib` | `Ref<ShaderLibrary>` | 全局共享 |
| `InternalErrorShader / StandardShader / SkyboxShader` | `Ref<Shader>` | 全局共享 |
| `InternalErrorMaterial / DefaultMaterial` | `Ref<Material>` | 全局共享 |
| `DefaultSkyboxMaterial`（原 `Environment.SkyboxMaterial`，语义精确化） | `Ref<Material>` | 全局共享的**默认**天空盒；新场景以此为初始值 |
| `DefaultTextures` | `unordered_map<TextureDefault, Ref<Texture2D>>` | 全局共享 |
| `Pipeline` | `RenderPipeline` | R33.2 阶段**暂留**；R33.3 下移 |

### 7.4 实施步骤（6 个原子提交）

**为控制风险，R33.2 拆成 6 个连续 commit，每个 commit 后编译通过、画面等价**：

- **Commit 1**：`Stats` 下移 ?? 在 SceneRenderer 添加 `m_Stats` 字段，把 `SceneRenderer::GetStats/ResetStats` 从"转发 Renderer3D 全局"改为"读写自己的 m_Stats"；Pass 通过 `RenderContext::Stats` 写入的地址改为指向 SceneRenderer 的 `m_Stats`（`RenderContext` 已经支持 `Statistics*`，只是当前指向 `s_Data.Stats`）
- **Commit 2**：DrawCommand 队列下移 ?? `m_OpaqueDrawCommands / m_TransparentDrawCommands / m_SpriteDrawCommands / m_OutlineDrawCommands` 迁入 SceneRenderer；`SubmitMesh / SubmitSprite / EndScene` 内部读写实例字段；`RenderContext` 指针指向实例字段
- **Commit 3**：UBO 与相机缓存下移 ?? `m_CameraUniformBuffer / m_LightUniformBuffer / m_CameraBuffer / m_LightBuffer / m_CameraPosition / m_CameraViewMatrix / m_CameraProjectionMatrix` 迁入；`BeginScene` 逻辑迁入 SceneRenderer 实现（不再转发）
- **Commit 4**：Shadow 参数下移 ?? `m_ShadowEnabled / m_ShadowBias / m_ShadowStrength / m_ShadowShadowType / m_CascadeXxx / m_SpotShadowData / m_PointShadowData` 迁入；CSM cascade split 计算与 spot/point shadow matrix 计算迁入 `SceneRenderer::BeginScene`
- **Commit 5**：Outline / ClearColor / TargetFramebuffer 下移 ?? Panel 侧的 `SetTargetFramebuffer` 调用**彻底删除**（SceneRenderer 内部持有 FBO，`EndScene` 时直接构建 `RenderContext.TargetFramebuffer = m_Framebuffer`）；P0.6 的 `Renderer3D::SetOutlineEntities({})` 应急代码**彻底删除**
- **Commit 6**：PostProcess / Environment 下移 ?? `m_PostProcess / m_Environment` 迁入；`SetPostProcessSettings` 内部同步 PostProcessStack 的逻辑迁入 SceneRenderer（但要注意 R33.2 阶段 Pipeline 还在全局，`GetPipeline().GetPass<PostProcessPass>()` 依然从全局拿??这是 R33.3 才处理的问题）

### 7.5 关键陷阱

#### 7.5.1 R33.2 阶段 Pipeline 仍然全局，Pass 内部 FBO 也全局

R33.2 阶段 `RenderPipeline` 及其 Pass 实例还在 `Renderer3D::s_Data.Pipeline` 里。**两个 SceneRenderer 交替执行 `EndScene` 时，会串行使用同一个 Pipeline**：Scene 面板执行 EndScene，Pass 内部 FBO 被写入 Scene 面板的数据；然后 Game 面板执行 EndScene，Pass 内部 FBO 被覆盖为 Game 面板的数据。**串行执行下不冲突**。

但 `PostProcessPass::m_HDR_FBO` 是全局的??两个面板的 HDR 内容会互相覆盖。**这在 R33.2 阶段仍然存在**，但因为每个面板 EndScene 后立即把结果 blit 到自己的 m_Framebuffer，最终显示正确，只是**中间 HDR 内容不能跨帧保留**??当前也不需要保留。

**结论**：R33.2 阶段可以接受这个限制。R33.3 完成后 HDR FBO 归 SceneRenderer 独立持有，问题自动消失。

#### 7.5.2 `RenderContext.Stats` 指针必须指向 SceneRenderer 的 m_Stats

原代码 `context.Stats = &s_Data.Stats;`??下移后必须改为 `context.Stats = &m_Stats;`。Pass 内部通过 `context.Stats->DrawCalls++` 累加即可。

#### 7.5.3 `IBLPrecompute` 全局，`m_Environment` 改变时的 IBL 重生成

原 `Renderer3D::SetEnvironmentSettings` 在 `SkyboxMaterial` 变化时调 `IBLPrecompute::GenerateFromCubemap`。如果直接把这个逻辑下移到 `SceneRenderer::SetEnvironmentSettings`，会出现"两个 SceneRenderer 各自持有 `m_Environment` 副本、各自检测到同一 SkyboxMaterial 变化、各自触发一次 IBL 重生成"的重复开销。

**修正方案**：把 IBL 重生成的触发点从"SceneRenderer 接受 EnvironmentSettings 时"**上移到**"Scene 内 EnvironmentSettings 变更时"。IBL 本就是场景级共享资源，由场景一次触发，不应该由多个消费方重复驱动。

具体实现（不引入权宜去重）：

- `Scene::GetEnvironmentSettings()` 返回引用供编辑器修改。引入一个 `Scene::SetEnvironmentSettings(const EnvironmentSettings&)` 写入接口，Scene 内部检测 `SkyboxMaterial` 或 `ReflectionResolution` 变化，变化则直接调 `IBLPrecompute::GenerateFromCubemap`
- `SceneRenderer::SetEnvironmentSettings` 只拷贝参数到 `m_Environment`，**不再检测变化、不再触发 IBL 重生成**
- LightingPanel / 其他修改 EnvironmentSettings 的入口都走 Scene 接口（而非直接写入 SceneRenderer）

好处：
- **单一驱动源**：IBL 重生成的触发点唯一，与 SceneRenderer 数量无关
- **不需要在 `IBLPrecompute` 内部加"Cubemap ID 去重缓存"这种为了避免重复触发而引入的魔法（**避免“为了性能的不优雅处理”**）
- 预留未来多 Scene 场景（如开两个 Scene 同时预览）时自然无冲突

### 7.6 R33.2 验收标准

1. 编译通过
2. `Renderer3DData` struct 只剩 §7.3 表中的字段（Pipeline 除外，暂留 R33.3 处理）
3. **两个面板真正独立**：
   - 关闭 Scene 面板的 CSM Debug Visualize，Game 面板不受影响
   - Scene 面板选中一个实体（描边），Game 面板不显示描边
   - Scene 面板的 DrawCall Stats 与 Game 面板独立（RenderPipelinePanel 需选择显示哪个 SceneRenderer 的 Stats??R33.2 简化处理：只显示 Scene 面板的 SceneRenderer Stats）
4. P0.6 引入的 workaround 全部清理（无 `Renderer3D::SetOutlineEntities({})` / `Renderer3D::SetTargetFramebuffer` 显式覆盖）
5. Renderer3D 头文件不再暴露 `SetXxx / BeginScene / EndScene / DrawMesh / DrawSprite / RenderOutline / GetPipeline / GetStats / ResetStats`；所有对旧 API 的引用要么迁移到 SceneRenderer，要么删除

---

## 8. R33.3 Pipeline 实例化 + Spec 配置

### 8.1 目标

- `RenderPipeline` 从 `Renderer3D::s_Data` 下移到 `SceneRenderer::m_Pipeline`
- 每个 SceneRenderer 按 `SceneRendererSpec` 布尔开关**选择性构造** Pass 实例
- 每个 SceneRenderer 独立持有完整 ShadowPass（含 CSM FBO / ShadowAtlas）??与 Unity URP 对齐，不引入共享抽象
- `RenderPipelinePanel` 支持展示"当前激活 SceneRenderer"

### 8.2 RenderPipelinePanel 展示哪个 SceneRenderer 的 pipeline

**方案 A（推荐）**：面板暴露"当前活动 SceneRenderer"下拉，默认选 Scene 面板的 SceneRenderer

- 通过 `SceneRendererRegistry` 单例注册所有活动 SceneRenderer（Panel ctor 里注册、dtor 里注销）
- RenderPipelinePanel 顶部下拉框选一个
- **优点**：任意 SceneRenderer 都可调试
- **缺点**：新增一个 Registry 抽象

**方案 B**：固定展示 Scene 面板的 SceneRenderer

- 通过某种"主 SceneRenderer"约定（例如 EditorLayer 提供 `GetPrimarySceneRenderer()`）
- **优点**：简单
- **缺点**：Game 面板的调试信息看不到

**推荐**：**方案 A**，`SceneRendererRegistry` 实现极简（只是 `std::vector<SceneRenderer*>`）。

### 8.3 Pass 组合按 Spec 构造

```cpp
void SceneRenderer::BuildPipeline()
{
    // Shadow 分组：可选（缩略图 / 反射探针烘焙时关闭）
    if (m_Spec.EnableShadow)
    {
        m_Pipeline.AddPass(CreateRef<ShadowPass>());
    }

    // Main 分组：Opaque / Skybox / Transparent / Sprite2D 始终需要
    m_Pipeline.AddPass(CreateRef<OpaquePass>());
    m_Pipeline.AddPass(CreateRef<SkyboxPass>());
    m_Pipeline.AddPass(CreateRef<TransparentPass>());
    m_Pipeline.AddPass(CreateRef<Sprite2DPass>());

    // Picking 分组：可选
    if (m_Spec.EnablePicking)
    {
        m_Pipeline.AddPass(CreateRef<PickingPass>());
    }

    // Debug 分组：可选
    if (m_Spec.EnableDebugVisualize)
    {
        m_Pipeline.AddPass(CreateRef<DebugVisualizePass>());
    }

    // PostProcess 分组：可选（Panel 需要 HDR 输出时启用）
    if (m_Spec.EnablePostProcess)
    {
        auto postProcessPass = CreateRef<PostProcessPass>();
        m_Pipeline.AddPass(postProcessPass);

        auto bloomEffect = CreateRef<BloomEffect>();
        bloomEffect->Order = 0;
        bloomEffect->Enabled = false;
        auto vignetteEffect = CreateRef<VignetteEffect>();
        vignetteEffect->Order = 10;
        vignetteEffect->Enabled = false;
        auto fxaaEffect = CreateRef<FXAAEffect>();
        fxaaEffect->Order = 0;
        fxaaEffect->Enabled = false;

        postProcessPass->GetPostProcessStack().AddEffect(bloomEffect);
        postProcessPass->GetPostProcessStack().AddEffect(vignetteEffect);
        postProcessPass->GetPostProcessStack().AddEffect(fxaaEffect);
    }

    // Outline 分组：可选
    if (m_Spec.EnableOutline)
    {
        auto silhouettePass = CreateRef<SilhouettePass>();
        auto outlineCompositePass = CreateRef<OutlineCompositePass>();
        outlineCompositePass->SetSilhouettePass(silhouettePass);
        m_Pipeline.AddPass(silhouettePass);
        m_Pipeline.AddPass(outlineCompositePass);
    }

    m_Pipeline.Init();
}
```

### 8.4 Shadow 多实例的行为确认

- 每个 SceneRenderer 持有自己的 ShadowPass，内部自行创建 CSM FBO / ShadowAtlas，归本实例生命周期管理
- Scene 面板用 EditorCamera 的 cascade split 写入自己的 CSM；Game 面板用 Primary Camera 的 cascade split 写入自己的 CSM??**互不干扰**
- 缩略图 / 反射探针等未来用途可以设 `EnableShadow = false`，完全不构造 ShadowPass，省掉阴影相关的全部显存与 CPU/GPU 时间

### 8.5 R33.3 验收标准

1. `Renderer3DData` 只剩全局共享资源（无 Pipeline）
2. Scene 面板 spec 启用全部 Pass、Game 面板关闭 Picking/Outline/Debug 后：
   - Game 面板 FBO 附件数 = 2（RGBA8 + Depth），Scene 面板 = 3（+ RED_INTEGER）
   - Game 面板 Pipeline 中无 SilhouettePass / OutlineCompositePass / PickingPass / DebugVisualizePass
3. 构造一个 `EnableShadow = false` 的 SceneRenderer：其 Pipeline 中无 ShadowPass，Shadow 相关显存 = 0
4. RenderPipelinePanel 支持选择 SceneRenderer（顶部下拉），切换后正确显示对应 pipeline 的 Pass 列表与 Stats
5. Scene 面板 / Game 面板画面无回归

---

## 9. 关键陷阱汇总（供实施时参考）

### 9.1 头文件 `Renderer3D.h` 的引用需要清理

R33.1 之后 `Renderer3D.h` 仍被大量 `.cpp` 引用（例如 Pass 内部的 `Renderer3D::GetDefaultMaterial`）。R33.2 完成后要检查每个 include：

- Pass `.cpp` 内如果只是拿 `GetDefaultMaterial / GetShaderLibrary / GetDefaultTexture`，可以继续 include `Renderer3D.h`
- 如果是拿 `LightRenderData / CameraRenderData / Statistics`，应改为 include `LightRenderData.h / CameraRenderData.h / SceneRenderer.h`
- Panel 层应主要 include `SceneRenderer.h`

### 9.2 `Renderer3D::Statistics` 与 `SceneRenderer::Statistics`

R33.1 阶段两者并存（`SceneRenderer::GetStats` 转发到 `Renderer3D::GetStats`）；R33.2 完成后 `Renderer3D::Statistics` 删除，`RenderContext::Stats` 类型改为 `SceneRenderer::Statistics*`。

### 9.3 `EditorLayer::OnUpdate` 的 `ResetStats`

变更前：
```cpp
Renderer3D::ResetStats();
Renderer2D::ResetStats();
```

变更后（R33.2 之后）：
- `Renderer2D::ResetStats();` 保留（Renderer2D 仍全局单例）
- 移除 `Renderer3D::ResetStats();`
- 每个 SceneRenderer 在自己的 OnUpdate 开头 `m_SceneRenderer->ResetStats()`??**由 Panel 负责，而非 EditorLayer**

推荐：**在 SceneRenderer::BeginScene 内部第一行 `m_Stats = {};` 自动重置**，Panel 不感知。

### 9.4 `Renderer2D` 的全局单例仍然存在

`Sprite2DPass` 内部调 `Renderer2D::BeginScene / DrawSprite / EndScene`。多 SceneRenderer 串行执行时 Renderer2D 也是串行使用，安全。**本 Phase 不动**，将来单独重构。

### 9.5 `IBLPrecompute` 依然是全局

`IBLPrecompute` 生成的 3 张贴图（Irradiance / Prefilter / BRDF LUT）对所有 SceneRenderer 可见，属于场景级共享资源。IBL 重生成的**触发点归 Scene 层**（而非 SceneRenderer）：Scene 保留 `EnvironmentSettings`，在 SkyboxMaterial / ReflectionResolution 变化时直接调 `IBLPrecompute::GenerateFromCubemap`；SceneRenderer 只是消费者，不重复触发。这符合"IBL 是场景级共享资源"的定位。

### 9.6 `PostProcessPass::m_HDR_FBO` 归属

R33.2 阶段 HDR FBO 依然全局（因为 Pass 还在全局 Pipeline）；R33.3 之后每个 SceneRenderer 独立 Pipeline，HDR FBO 也独立。Scene 面板的 PickingPass 从 HDR FBO 读 EntityID 的代码要迁移到 `SceneRenderer::ReadPixelEntityID` 内部??**通过 `m_Pipeline.GetPass<PostProcessPass>()->GetHDR_FBO()` 拿到自己的 HDR FBO 再读**。

### 9.7 Scene 面板的 `CameraProjectionMatrix` for Sprite2DPass

`RenderContext::CameraProjectionMatrix` 供 `Sprite2DPass` 里 `Renderer2D::BeginScene` 构造 VP。R33.2 之后 SceneRenderer::EndScene 构建 RenderContext 时从 `m_CameraProjectionMatrix` 填入，与现状等价。

### 9.8 `Renderer3D::SetSkyboxMaterial` 删除后的迁移

原 API 允许"用代码调 `SetSkyboxMaterial` 覆盖当前场景天空盒"。R33 后**这个入口消失**??设置天空盒统一走 `Scene::GetEnvironmentSettings().SkyboxMaterial = xxx`，Scene 每帧把 EnvironmentSettings 传给 SceneRenderer，SceneRenderer 内部检测变化后触发 IBL 重生成。

现有调用点搜索：需要在 R33.1 前 grep 一次 `SetSkyboxMaterial` 确认（预估仅少量测试代码）。

---

## 10. 影响面清单（跨 R33.1 / R33.2 / R33.3）

### 10.1 新建文件

| 文件 | 说明 | Phase |
|---|---|---|
| `Lucky/Source/Lucky/Renderer/SceneRenderer.h` | SceneRenderer 类声明 | R33.1（骨架）→ R33.2（补字段）→ R33.3（Pipeline 下移） |
| `Lucky/Source/Lucky/Renderer/SceneRenderer.cpp` | SceneRenderer 实现 | R33.1（转发）→ R33.2（实装）→ R33.3（Pipeline 构造） |
| `Lucky/Source/Lucky/Renderer/LightRenderData.h` | 光照数据结构从 Renderer3D.h 抽出 | R33.1 |
| `Lucky/Source/Lucky/Renderer/CameraRenderData.h` | 相机数据结构从 Renderer3D.h 抽出 | R33.1 |

### 10.2 修改文件

| 文件 | R33.1 改动 | R33.2 改动 | R33.3 改动 |
|---|---|---|---|
| `Renderer3D.h` | 去掉 `LightRenderData / CameraRenderData`（改为 include） | 删除 `SetXxx / BeginScene / EndScene / DrawMesh / DrawSprite / SetPostProcessSettings / SetEnvironmentSettings / GetStats / ResetStats / Statistics / SetTargetFramebuffer / SetOutlineEntities / SetOutlineColor / SetClearColor / RenderOutline / ResizePipeline / GetPipeline / SetSkyboxMaterial`；`GetSkyboxMaterial` 改名 `GetDefaultSkyboxMaterial` | 删除 `Pipeline` 相关辅助（若有） |
| `Renderer3D.cpp` | 无 | 大幅瘦身 `s_Data` 与实现 | Pipeline 构造代码删除 |
| `RenderContext.h` | 无 | `Statistics*` 类型改为 `SceneRenderer::Statistics*` | 无 |
| `Scene.h` | `OnRenderEditor / OnRenderRuntime` 签名增加 `SceneRenderer&` 参数 | 无 | 无 |
| `Scene.cpp` | `RenderSceneImpl` 内部改为调 `renderer.Xxx` | 无 | 无 |
| `SceneViewportPanel.h/.cpp` | 持有 `Ref<SceneRenderer>`，删除 `m_Framebuffer`，所有 `Renderer3D::SetXxx` 迁移为 `m_SceneRenderer->Xxx` | 简化：SceneRenderer 自动处理 Target FBO / Outline Entities | 拾取代码迁移到 `m_SceneRenderer->ReadPixelEntityID` |
| `GameViewportPanel.h/.cpp` | 同上；`SceneRendererSpec` 关闭 Picking/Outline/Debug | 删除 P0.6 workaround（`Renderer3D::SetTargetFramebuffer / SetOutlineEntities({})`） | 无 |
| `RenderPipelinePanel.cpp` | `Renderer3D::GetPipeline / GetStats` 暂保留 | 迁移到"当前 SceneRenderer"（简化：Scene 面板的） | 支持下拉切换 SceneRenderer |
| `EditorLayer.cpp` | 无 | 移除 `Renderer3D::ResetStats()`（SceneRenderer::BeginScene 自动重置） | 无 |
| 所有 `Passes/*.cpp` | 无 | 极少数：`Renderer3D::GetInternalErrorMaterial` 调用点保持 | 无 |

### 10.3 不动的文件

- 所有 Shader（`Assets/Shaders/**`）
- `RenderCommand / Buffer / VertexArray / Shader / Material / Framebuffer / Texture / UniformBuffer` 底层 GPU 抽象
- `Renderer2D / GizmoRenderer`（本 Phase 保持全局单例）
- `IBLPrecompute`（保持当前形态：全局单例，由 Scene 层驱动重生成）
- `RenderPipeline / RenderPass / RenderContext`（结构不变，只是实例的持有者变化）

---

## 11. 与 P0.6 应急代码的对应关系

P0.6 落地时因为 `Renderer3D::s_Data` 是全局的，Game 面板加了几处应急代码：

| 现有应急代码 | R33 落地后 |
|---|---|
| `Renderer3D::SetTargetFramebuffer(m_Framebuffer);` (GameViewportPanel::OnUpdate) | **R33.2 删除**??SceneRenderer 内部自动使用 `m_Framebuffer` |
| `Renderer3D::SetOutlineEntities({});` (GameViewportPanel::OnUpdate) | **R33.2 删除**??Game 面板 SceneRenderer 天然无 Outline 数据 |
| `Renderer3D::SetClearColor(blackClear);` (GameViewportPanel::OnUpdate) | **R33.1 保留**（迁移为 `m_SceneRenderer->SetClearColor(blackClear)`） |
| GameViewportPanel 不调 `Renderer3D::ResizePipeline` | **R33.3 起**每个 SceneRenderer 独立 Pipeline，Game 面板自己 Resize 自己的 Pipeline，不再与 Scene 面板抢 |
| Scene / Game 面板 aspect 写竞争（都调 `Scene::OnViewportResize`） | **不由 R33 解决**??这是 CameraComponent 的 aspect 由谁负责的问题，本 Phase 不涉及 |

---

## 12. 验收标准（整体）

### 12.1 功能验证

- Scene 面板：拾取 / Gizmo / Outline / DebugVisualize / DragDrop 场景切换 / CSM 阴影 / 后处理 / 天空盒 / IBL ?? 全部无回归
- Game 面板：主相机构图正确 / 无主相机时纯黑 / 后处理与 Scene 面板独立
- Play / Pause / Stop：三态下 Scene 与 Game 面板均正确渲染
- RenderPipelinePanel：Stats 与 Pass 列表显示正确（R33.3 之后支持选择 SceneRenderer）

### 12.2 架构验证

- `Renderer3DData` 结构体只剩全局共享资源字段（ShaderLib / 默认材质 / 默认纹理 / IBL 相关）
- `Renderer3D` 公开接口只剩：`Init / Shutdown / GetShaderLibrary / GetInternalErrorMaterial / GetDefaultMaterial / GetDefaultSkyboxMaterial / GetDefaultTexture`
- SceneRenderer 完整封装一次渲染所需的全部状态
- P0.6 应急代码全部清理

### 12.3 性能与显存预期

- Scene 面板 + Game 面板同时开启时，阴影相关显存与 CPU/GPU 时间**均翻倍**（与 Unity URP 多相机行为一致），属于可接受代价
- IBL 相关贴图（Irradiance / Prefilter / BRDF LUT）因为归属 Scene，多 SceneRenderer 不重复占用
- 以 `EnableShadow = false` 构造的 SceneRenderer（未来缩略图 / 反射探针烘焙）阴影显存 = 0，阴影 CPU/GPU = 0
- FPS 与 R33 前无显著回归（±5% 以内）

### 12.4 代码质量

- 遵循 [Coding_Style_Guide.md](../Coding_Style_Guide.md)：私有成员 `m_` 前缀、结构体注释列对齐、无阶段性注释
- 无"文档式注释"（不写"详细设计参见 xxx.md"、"分层原则：XXX 不感知 XXX"等）
- 头文件只 include 必要项，能前向声明的用前向声明

---

## 13. 风险与规避

| 风险 | 影响 | 规避 |
|---|---|---|
| R33.2 一次改动面过大 | 编译红一大片，难以逐步验证 | 严格按 §7.4 的 6 个原子 commit 推进，每 commit 后编译通过 + 画面等价 |
| RenderPipelinePanel 面临"展示谁"的产品问题 | R33.3 阻塞 | R33.2 阶段先固定展示 Scene 面板 SceneRenderer；R33.3 补下拉 |
| Scene + Game 同开时阴影显存/时间翻倍 | 编辑器需 8GB+ 显存；弱机卡顿 | 与 Unity URP 多相机行为一致，属可接受代价；未来缩略图 / 反射探针场景用 `EnableShadow = false` 回避 |
| `IBLPrecompute` 依然全局 | 架构不完全统一 | 接受当前形态，未来独立 PhaseR34 处理 |
| `Renderer2D` 全局单例未处理 | Sprite 路径仍有全局依赖 | 本 Phase 不做，独立 Phase 处理 |
| Panel 的 `m_Framebuffer` 与 SceneRenderer 的 `m_Framebuffer` 双写 | R33.1 阶段可能出现两份 FBO 引用 | R33.1 就把 Panel 的 `m_Framebuffer` 删除，统一用 `m_SceneRenderer->GetFramebuffer()` |
| Scene::OnRenderXxx 签名变更影响未来 P0.7+ 脚本系统 | 脚本系统的钩子挂载点被间接影响 | 无影响??`OnUpdateRuntime(dt)` 与 `OnRenderRuntime(SceneRenderer&)` 是两个独立入口，脚本钩子只挂 `OnUpdateRuntime` |

---

## 14. 时序建议

```
R33.1 骨架 + 转发                           （约 1 天）
  ├─ 新建 SceneRenderer.h/.cpp（转发实现）
  ├─ 新建 LightRenderData.h / CameraRenderData.h
  ├─ Scene::OnRenderXxx 增加 SceneRenderer& 参数
  ├─ SceneViewportPanel / GameViewportPanel 迁移
  └─ 验收：画面完全等价

R33.2 状态字段下移                           （约 3-4 天）
  ├─ Commit 1: Stats 下移
  ├─ Commit 2: DrawCommand 队列下移
  ├─ Commit 3: UBO + 相机缓存下移
  ├─ Commit 4: Shadow 参数下移
  ├─ Commit 5: Outline + ClearColor + TargetFramebuffer 下移（清理 P0.6 workaround）
  ├─ Commit 6: PostProcess + Environment 下移
  └─ 验收：两个面板真正独立

R33.3 Pipeline 实例化 + Spec 配置                    （约 2 天）
  ├─ Pipeline 下移到 SceneRenderer
  ├─ BuildPipeline 按 Spec 布尔开关选择 Pass
  ├─ SceneRendererRegistry + RenderPipelinePanel 下拉
  └─ 验收：EnableShadow=false 时无 ShadowPass；Pass 组合按 Spec 动态构造
```

**建议**：R33.1 可以立即开始，与 Script System 的 P0.7+ 完全并行不冲突（P0.7+ 与 Renderer 层解耦）。

---

## 15. 参考

- [PhaseR33_SceneRenderer_Abstraction.md](./PhaseR33_SceneRenderer_Abstraction.md)：初步分析文档
- [Coding_Style_Guide.md](../Coding_Style_Guide.md)：代码规范
- Hazel Engine `SceneRenderer` 类：`Hazel-dev/Hazel/src/Hazel/Renderer/SceneRenderer.{h,cpp}`
- Unity SRP `ScriptableRenderer` + `RenderingData` + `CameraData`
- Unreal `FSceneRenderer` + `FSceneView` + `FSceneViewFamily`
