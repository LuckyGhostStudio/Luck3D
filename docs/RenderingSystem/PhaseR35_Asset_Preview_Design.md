# PhaseR35：资产预览系统 — 正式设计文档

> **文档性质**：正式设计文档（Design）。覆盖本阶段的两个子模块：
> - **P1 · AssetPreviewRenderer**：离屏渲染器，为 Material / Mesh 产出一张 2D 纹理。
> - **P3 · UI 集成**：把预览纹理接入 ProjectAssetsPanel 的资产格子（Inspector 的资产引用字段**本阶段不改**，保持固定图标）。
>
> **代码风格**：全文所有代码片段严格遵循 [Coding_Style_Guide.md](../Coding_Style_Guide.md)。
> **本文档不包含**：P2 持久化缓存、P4 MaterialEditor 嵌入球体预览窗、异步 / LRU 等优化（见 §10）。

---

## 1. 概述

### 1.1 目标

目前 `ProjectAssetsPanel::GetThumbnail` 恒返回 `nullptr`（`ProjectAssetsPanel.cpp:656`），资产面板和 Inspector 里所有资产都用 `EditorIconManager::GetAssetTypeIcon(type)` 统一的类型图标 PNG，无法从视觉上区分同类资产。本阶段：

- **P1**：新建离屏渲染服务 `AssetPreviewRenderer`，用一个独立的 `SceneRenderer` 实例在小尺寸 FBO 上渲染一张图：
  - `Material` → 一颗球 + 目标材质
  - `Mesh` → 目标 Mesh + 默认白色材质 + 按包围盒自动框选的相机
- **P3**：
  - `ProjectAssetsPanel::GetThumbnail` 真正返回缩略图纹理（命中则显示在资产格子图标位置）。
  - **本阶段不改 `UI::PropertyAsset`**：Inspector 里 Mesh / Material 字段统一用静态类型图标。原因：Inspector 字段的图标尺寸很小（约 16~20px），128×128 缩略图下采样后无视觉收益，且会把失效联动的面扩大到所有组件 UI；留给 P4（MaterialEditor 嵌入预览）里有真正价值的地方再接。

### 1.2 前置依赖

- **PhaseR33 SceneRenderer 抽象**（已完成）：`SceneRenderer` 已可实例化，内部持有独立 FBO / Pipeline / UBO，由 `SceneRendererSpec` 决定附件与 Pass 组合。本阶段**完全复用**，不改渲染管线主干。
- **AssetSystem Phase A / B / C**（已完成）：`AssetHandle`、`AssetManager::GetAsset<T> / GetAssetHandle / GetAssetType / GetAssetFilePath` 可用。
- **MaterialSerializer**（已完成）：Material 保存时可注入"失效回调"。
- **MeshFactory::CreatePrimitive(Sphere)**（已完成）：Material 预览的球体由此生成。

### 1.3 本 Phase **不做**的事

- **不做持久化缓存**：所有缓存只在内存里，程序退出丢弃。磁盘 `.thumb.png` 侧车文件留给 P5。
- **不做异步渲染**：首次打开大目录会同步阻塞渲染 N 张纹理。按帧预算的异步队列见 §10。
- **不做 LRU 淘汰**：缓存只增不减，直到项目关闭或资产被删除。
- **不做 MaterialEditor 嵌入球体预览窗**：那是 P4 的事，依赖本阶段的 `AssetPreviewRenderer`。
- **不做 Mesh 预览的鼠标拖拽旋转**：缩略图是固定视角的"静态图"，拖拽旋转留给 P4。
- **不改渲染 Pass**：OpaquePass / SkyboxPass 等照常，`SceneRendererSpec` 关掉其他可选 Pass 即可。

### 1.4 子阶段产出

| 子阶段 | 关键产出 | 验证方式 |
|---|---|---|
| **R35.1 Renderer 骨架** | `AssetPreviewRenderer::Init / Shutdown`，内部实例化 128×128 `SceneRenderer`；`RenderMaterial` / `RenderMesh` 跑通一次离屏渲染，返回 `const Ref<Framebuffer>&` | 临时测试代码点击按钮触发 `RenderMaterial(默认材质)`，直接 `ImGui::Image` 它的 colorAttachment（Y 翻 UV）能看到球体 |
| **R35.2 Cache + 失效 + Blit** | `AssetPreviewCache::GetOrRender / Invalidate / Clear`；每个 Entry 持有一张独立的 `Ref<Texture2D>`，内部通过 `glBlitFramebuffer` 把 Renderer 的 FBO 像素 Blit 到 Texture2D（Y 翻转）；MaterialSerializer::Serialize / MaterialEditor 属性改动 / AssetManager 删除移动都触发 Invalidate | 同一 handle 二次 Get 命中缓存零 OpenGL 调用；资产面板同屏多张不同缩略图可区分、互不覆盖；Material 改属性保存后缩略图自动更新 |
| **R35.3 UI 接入** | `ProjectAssetsPanel::GetThumbnail` 实际调 Cache；`DrawAssetItem` 优先用缩略图 | 资产面板里 .lmat 显示该材质球、.lmesh 显示默认白色剪影、.png 直接显示纹理；无缩略图的类型（.scene / .cs）继续显示类型图标 |

---

## 2. 总体架构

### 2.1 分层图

```
┌─────────────────────────────────────────────────────────────────┐
│  编辑器层                                                        │
│  ┌─────────────────────────┐    ┌───────────────────────────┐   │
│  │ ProjectAssetsPanel      │    │ InspectorPanel            │   │
│  │   DrawAssetItem         │    │   UI::PropertyAsset       │   │
│  │   (格子图标)             │    │   (资产引用字段)           │   │
│  └───────────┬─────────────┘    └─────────────┬─────────────┘   │
│              │                                │                  │
│              └──────────────┬─────────────────┘                  │
│                             ↓                                    │
│  ┌───────────────────────────────────────────────────────────┐   │
│  │  AssetPreviewCache  (单例)                                 │   │
│  │    GetOrRender(handle, type) → Ref<Texture2D>              │   │
│  │    Invalidate(handle)                                      │   │
│  │    Clear()                                                 │   │
│  │    内部：unordered_map<AssetHandle, PreviewEntry>           │   │
│  └───────────────────────────┬───────────────────────────────┘   │
│                              │ miss 时调用                        │
│                              ↓                                    │
│  ┌───────────────────────────────────────────────────────────┐   │
│  │  AssetPreviewRenderer  (单例)                              │   │
│  │    RenderMaterial(Ref<Material>) → uint32_t colorTexID     │   │
│  │    RenderMesh(Ref<Mesh>)         → uint32_t colorTexID     │   │
│  │    内部：Ref<SceneRenderer>  (128×128, 关掉所有可选 Pass)   │   │
│  └───────────────────────────┬───────────────────────────────┘   │
│                              │ 调用现有渲染路径                    │
│                              ↓                                    │
├──────────────────────────────────────────────────────────────────┤
│  渲染层                                                           │
│  ┌───────────────────────────────────────────────────────────┐   │
│  │  SceneRenderer  (PhaseR33 已有)                            │   │
│  │    BeginScene / SubmitMesh / EndScene / GetFramebuffer     │   │
│  └───────────────────────────────────────────────────────────┘   │
└──────────────────────────────────────────────────────────────────┘
```

### 2.2 数据流（一次缩略图请求）

```
Panel 绘制资产格子：
  thumb = AssetPreviewCache::GetOrRender(handle, type)
    ├─ 命中 → 返回 Ref<Texture2D>（纹理 ID = SceneRenderer 的 FBO color attachment）
    └─ miss →
        AssetPreviewRenderer::RenderMaterial(material)：
          1. 内部 SceneRenderer 清状态
          2. BeginScene(fixedCam, fixedLight)
          3. SubmitMesh(identity, sphereMesh, { material })
          4. EndScene()  ←  一次性把所有 Pass 跑完，结果留在 FBO 里
          5. 返回 fb->GetColorAttachmentRendererID(0)
        包装成 Ref<Texture2D>（代理纹理），存入 Cache
  若 thumb != nullptr：
    ImGui::Image(thumb->GetRendererID(), iconSize)
  否则 fallback 到 EditorIconManager::GetAssetTypeIcon(type)
```

### 2.3 关键不变量

1. **预览渲染只在 UI 绘制阶段触发**：此时主视口的 `SceneRenderer::EndScene` 已经执行完，默认 FBO 已恢复。预览调用结束后 FBO 状态由 `SceneRenderer::EndScene` 内部保证解绑。
2. **预览 Renderer 的 FBO 不被 ImGui 持续采样**：每次 `GetOrRender` 返回的纹理 ID 虽然指向同一个 FBO 的 color attachment，但 ImGui 的渲染是在 **当前帧**结束时才真正 submit draw call。只要我们在同一帧内不重复写入这同一个 FBO，就不会出现"显示半张图"。
3. **一次只渲染一张**：Renderer 内部 `SceneRenderer` 是**唯一实例**，并发请求会串行。P2 之后可以加队列。

---

## 3. 关键设计决策

### 3.1 预览渲染器的实现方式

**问题**：如何在小尺寸 FBO 上渲染一次目标资产？

- **方案 A（推荐）**：**复用 `SceneRenderer` 实例**，用 `SceneRendererSpec` 关闭所有可选 Pass
  - 优点：
    - 零渲染代码重复。Material 的 Shader、UBO、Pass 实现保持一致，所见即所得（和主视口一个光照模型）。
    - PhaseR33 的 `SceneRendererSpec` 本身就有 `EnableShadow / EnablePicking / EnableOutline / EnablePostProcess` 开关，天然适用于缩略图场景。
    - 未来 Material 增加新 Shader 变体，预览自动跟进。
  - 缺点：
    - 一个 SceneRenderer 实例占用的最小显存（关掉 Shadow / Picking / Outline / PostProcess 后）仍有一个 HDR FBO + Opaque / Transparent Pass 的少量开销，评估约 1~2 MB。
- **方案 B**：**手写最小渲染路径**，只调 `Shader::Bind` + `Material::Bind` + `glDrawElements`
  - 优点：开销最低，一张纹理可能只有 1 MB 显存。
  - 缺点：
    - 需要独立维护 Camera UBO / Light UBO 的写入逻辑。
    - Material 的 Shader 对 `u_LightCount` 等 uniform 有依赖，复刻一遍容易漏，Shader 变体一多就僵化。
    - Material 预览和主视口光照模型会脱节（主视口升级到 PBR / IBL 时预览还停在 Blinn-Phong）。
- **方案 C**：**在主 SceneRenderer 上开子视口**，把渲染结果 Blit 到预览 FBO
  - 优点：共享状态。
  - 缺点：主 SceneRenderer 的 Camera / Light / DrawCommand 队列和预览需求完全不一样，强行共用会污染状态；而且这意味着预览只能和主视口渲染同帧触发，无法独立刷新。

**决策**：**采用方案 A**。
**理由**：PhaseR33 的 Spec 开关正是为这种"小尺寸、少 Pass、独立 FBO"场景准备的（见 R33 §3.3 决策理由第 4 条）。工作量最低、长期可维护性最好。

### 3.2 缓存粒度（key 的选择）

- **方案 A（推荐）**：**按 `AssetHandle` 缓存**
  - 优点：Handle 稳定（文件 rename / move 时 `AssetManager::MoveAsset` 保持 Handle 不变），天然对齐资产的生命周期。
  - 缺点：无 handle 的"临时 Ref<Material>"（例如 Inspector 上编辑中的材质副本，还没落盘）需要额外处理 —— §3.4 的失效机制会覆盖。
- **方案 B**：按文件路径缓存
  - 优点：直观。
  - 缺点：rename 后缓存条目就残留并失效；跨会话无意义。

**决策**：**采用方案 A**。

### 3.3 Renderer 返回类型 与 Cache 存储格式（Q1 的落实）

**问题**：`AssetPreviewRenderer::RenderXxx` 返回什么？Cache Entry 存什么？

**先排除掉"PreviewTexture 代理"路线**。查了 `Lucky/Source/Lucky/Renderer/Texture.h`：`Texture2D` **不是接口而是具体类**，构造函数 `Texture2D(w, h)` / `Texture2D(path)` 都会**真实创建一张 OpenGL 纹理**；私有字段 `m_RendererID / m_Width / m_Height` 子类不可见。所以如果写一个 `PreviewTexture : public Texture2D` 代理：
- 要么**重复定义**自己的 `m_RendererID / m_Width / m_Height`（字段冗余、容易脱节）
- 要么走基类构造 → **白白创建一张永远不用的真实 OpenGL 纹理**（显存浪费 + 析构时还要 glDeleteTextures）
- 要么把 Texture2D 的字段改成 protected（污染基类，影响面大）

**所以放弃代理路线**，把渲染结果从 Renderer 的 FBO 拷到真实 `Texture2D` 上。方案对比：

- **方案 A（推荐）**：**Renderer 返回 `const Ref<Framebuffer>&`；Cache 用 `glBlitFramebuffer` 把像素 Blit 到自己持有的真实 Texture2D 上**
  - 每个 Cache Entry 持有一张通过 `Texture2D::Create(w, h)` 创建的空纹理；miss 时渲染一次到共享 FBO，再 Blit 到 Entry 的 Texture2D
  - Blit 时顺手做 **Y 翻转**（dstY0 > dstY1），这样 Texture2D 像素行序和 ImGui 常规纹理一致，UI 层用默认 UV `(0,0)-(1,1)` 消费
  - 优点：
    - UI 层签名完全不变（`AssetField` / `BeginRenamableTreeNode` 继续吃 `Ref<Texture2D>`）
    - Entry 的 Texture2D 独立且 Blit 完就定格，共享 FBO 下次被覆盖也不影响
    - 不需要动 `SceneRenderer` / `Framebuffer` 基础设施
  - 缺点：
    - 每张缩略图比"纯 FBO 共享"多占一张纹理显存（128×128 RGBA8 ≈ 64 KB，100 张 ≈ 6.4 MB，可接受）
    - 需要用一行原生 `glBlitFramebuffer` 突破 Framebuffer 抽象层 —— 封装进 `AssetPreviewCache` 的匿名命名空间函数即可，影响范围自限
- **方案 B**：**Renderer 返回 `uint32_t rendererID`，UI 层签名改成 `uint32_t`**
  - 优点：无 Blit 开销
  - 缺点：
    - `AssetField` / `BeginRenamableTreeNode` 签名都要改，影响面大
    - 共享单 FBO + 单 TexID → 同屏多张缩略图会相互覆盖（上一次 miss 渲染的 TexID 还被 UI 引用着，但像素已经被下一次 miss 覆盖）
- **方案 C**：**扩展 `SceneRenderer::SetOverrideTargetFramebuffer`，Entry 独占 FBO**
  - 优点：无 Blit，Entry 完全独立
  - 缺点：
    - 要改 PhaseR33 的 SceneRenderer 接口 + Pass 内部用 FBO 的路径（可能牵涉 Spec 判断）
    - 每张缩略图多占一个 FBO 对象（显存和方案 A 差不多，但 FBO 对象本身的 GL state 更"重"）

**决策**：**采用方案 A**。工程改动面最小、UI 层零侵入、同屏多张互不覆盖的问题自然解决。方案 C 等真有编辑器全局都要吃 override FBO 的场景（例如反射探针烘焙）再做。

### 3.4 失效挂接点

**问题**：Material 属性改了、Mesh 文件改了、资产被删除了，缩略图什么时候 invalidate？

- **方案 A（推荐）**：**在三个明确节点推送事件**
  1. `MaterialSerializer::Serialize` 保存完成后 → `AssetPreviewCache::Invalidate(handle)`
  2. `MaterialEditor` 内任何属性 setter 触发时 → 立即 Invalidate（**内存里改了还没落盘**也要刷新，所见即所得）
  3. `AssetManager::MoveAsset / DeleteAsset / UnregisterAsset` → Invalidate

- **方案 B**：按文件 mtime 轮询
  - 优点：对第三方工具改文件也能响应
  - 缺点：每帧要 stat 所有缓存条目；延迟不确定

- **方案 C**：注册到 `AssetManager` 的通用"资产变更总线"
  - 优点：接口干净，所有订阅者统一
  - 缺点：当前 `AssetManager` 没有事件总线，要先铺一层

**决策**：**采用方案 A**。方案 C 更干净但依赖 AssetManager 侧的新基础设施，不属于本阶段。方案 A 的三个节点是**真正能触发缩略图变化的全部路径**，覆盖完整。

### 3.5 Mesh 预览的相机框选

- **方案 A（推荐）**：根据 Mesh 的 AABB 计算包围球外切距离
  - 具体：`center = (min + max) * 0.5`；`radius = length(max - min) * 0.5`；相机距离 `d = radius / sin(fov * 0.5) * 1.3`（1.3 留 30% 边距）
  - 优点：对任意尺度的 Mesh 都能正好框住
  - 缺点：Mesh 需要先算出 AABB（当前 `Mesh` 类**没有**暴露 AABB，但顶点都在 `GetVertices()` 里，一次遍历就能算）
- **方案 B**：固定相机位置（例如 Z=3）
  - 优点：零计算
  - 缺点：巨型 Mesh 看不到完整，微小 Mesh 看不见

**决策**：**采用方案 A**。顺手在 `Mesh` 上加一个 `GetBoundingBox() const` getter（遍历一次 `m_Vertices` 算 AABB，缓存起来；Mesh 本身是不可变的，算一次就够）。

### 3.6 Material 预览的默认光照

- **方案 A（推荐）**：**一盏主方向光（右上 45°）+ 环境光（弱）**
  - 优点：三点布光里最关键的一盏，能同时体现 Albedo / Normal / Roughness，和 Unity Material Preview 的默认一致
  - 缺点：无
- **方案 B**：三点布光（主光 + 补光 + 顶光）
  - 优点：高光更漂亮
  - 缺点：对缩略图小图意义不大，三光计算还比单光更贵
- **方案 C**：IBL 环境光
  - 优点：PBR 体验最真实
  - 缺点：需要加载并保持一张 HDR cubemap，预览时也要跑 IBL 卷积，开销显著

**决策**：**采用方案 A**。背景清为中灰 (0.2, 0.2, 0.2, 1.0) 或透明。

### 3.7 预览触发时机

- **方案 A（推荐）**：**同步按需渲染**（第一次调 `GetOrRender` 时阻塞渲染一次）
  - 优点：实现最简单，正确性最高
  - 缺点：首次打开大目录时，一帧可能触发几十次渲染（估算 §9）
- **方案 B**：异步队列 + 每帧预算（每帧最多渲 N 张）
  - 优点：首帧不卡
  - 缺点：miss 时第一帧返回占位，几帧后才替换；需要额外的"等待态"图标
- **方案 C**：预加载（启动时就扫描所有资产并渲染）
  - 优点：使用时零延迟
  - 缺点：项目很大时启动卡顿

**决策**：**采用方案 A**。128×128 单张渲染耗时 << 1 ms（相比主视口 FullHD 几百万像素），即使一帧 50 张也可控。方案 B 留给 P5 异步化。

---

## 4. 模块与 API 设计

### 4.1 文件布局

```
Lucky/Source/Lucky/Editor/Preview/
├── AssetPreviewRenderer.h          # 渲染服务
├── AssetPreviewRenderer.cpp
├── AssetPreviewCache.h             # 缓存层 + 失效机制 + Blit 工具
└── AssetPreviewCache.cpp
```

`Build-Lucky.lua` 已经用 `Source/**.h` 和 `Source/**.cpp` glob 自动包含，只需 `premake5 vs2022` 重跑一次。

### 4.2 AssetPreviewRenderer

**职责**：拥有一个专用 `SceneRenderer`，提供两个阻塞式接口返回其内部 FBO 的引用。单例，由 `EditorLayer::OnAttach` 时 Init / `OnDetach` 时 Shutdown。

```cpp
#pragma once

#include "Lucky/Core/Base.h"
#include "Lucky/Renderer/Mesh.h"
#include "Lucky/Renderer/Material.h"
#include "Lucky/Renderer/Framebuffer.h"

namespace Lucky
{
    class SceneRenderer;

    /// <summary>
    /// 资产预览离屏渲染服务
    /// 内部持有一个专用 SceneRenderer（128×128，关闭所有可选 Pass），
    /// 为 Material / Mesh 一次性渲染到共享 FBO 后将 FBO 引用返回；
    /// 像素留在该 FBO 的 Color Attachment 中，由调用方（Cache）立即 Blit 到独立 Texture2D 保存
    /// </summary>
    class AssetPreviewRenderer
    {
    public:
        /// <summary>
        /// 初始化：创建专用 SceneRenderer 和 Sphere 预览网格
        /// 必须在 Renderer::Init（Renderer3D / Renderer2D / GizmoRenderer）之后调用
        /// </summary>
        static void Init();

        /// <summary>
        /// 释放专用 SceneRenderer 和 Sphere 预览网格
        /// 必须在 Renderer::Shutdown 之前、AssetPreviewCache::Shutdown 之后调用
        /// </summary>
        static void Shutdown();

        /// <summary>
        /// 渲染一张材质预览（球体 + 该材质 + 固定布光）
        /// 结果留在共享 FBO 的 Color Attachment 中，返回该 FBO
        /// 调用方必须在下一次调用 RenderXxx 之前 Blit 走像素
        /// </summary>
        /// <param name="material">目标材质，不可为空</param>
        /// <returns>共享 FBO 引用；material 为空时返回 nullptr</returns>
        static const Ref<Framebuffer>& RenderMaterial(const Ref<Material>& material);

        /// <summary>
        /// 渲染一张网格预览（Mesh + 默认白色材质 + 按包围盒自动框选相机）
        /// 结果留在共享 FBO 的 Color Attachment 中，返回该 FBO
        /// </summary>
        /// <param name="mesh">目标网格，不可为空</param>
        /// <returns>共享 FBO 引用；mesh 为空时返回 nullptr</returns>
        static const Ref<Framebuffer>& RenderMesh(const Ref<Mesh>& mesh);

        /// <summary>
        /// 预览纹理尺寸（宽 = 高 = 128，供 Cache 创建目标 Texture2D 时读取）
        /// </summary>
        static uint32_t GetPreviewSize();
    };
}
```

**内部关键逻辑**（.cpp 片段）：

```cpp
namespace Lucky
{
    namespace
    {
        constexpr uint32_t s_PreviewSize = 128;

        struct PreviewData
        {
            Ref<SceneRenderer> Renderer;        // 专用离屏渲染器
            Ref<Mesh> SphereMesh;               // 内置球体，供材质预览使用

            CameraRenderData FixedCamera{};     // 固定相机数据（Build 时按每次目标重算）
            LightRenderData FixedLight{};       // 固定布光数据
        };
        static Scope<PreviewData> s_Data;

        /// <summary>
        /// 按目标中心点和相机距离构造 FixedCamera（相机在 +Z 轴上，朝 -Z 看）
        /// 预览全部用透视投影，FOV 固定 30°
        /// </summary>
        void BuildFixedCamera(const glm::vec3& target, float distance)
        {
            constexpr float fovDeg = 30.0f;
            glm::vec3 camPos = target + glm::vec3(0.0f, 0.0f, distance);
            glm::mat4 view = glm::lookAt(camPos, target, glm::vec3(0.0f, 1.0f, 0.0f));
            glm::mat4 proj = glm::perspective(glm::radians(fovDeg), 1.0f, 0.01f, distance * 10.0f);

            s_Data->FixedCamera.ViewMatrix = view;
            s_Data->FixedCamera.ProjectionMatrix = proj;
            s_Data->FixedCamera.Position = camPos;
            s_Data->FixedCamera.Projection = ProjectionType::Perspective;
            s_Data->FixedCamera.FOV = fovDeg;
            s_Data->FixedCamera.AspectRatio = 1.0f;
            s_Data->FixedCamera.NearClip = 0.01f;
        }
    }

    void AssetPreviewRenderer::Init()
    {
        s_Data = CreateScope<PreviewData>();

        // 1) 专用 SceneRenderer：128×128，关掉一切可选 Pass
        SceneRendererSpec spec;
        spec.Width = s_PreviewSize;
        spec.Height = s_PreviewSize;
        spec.EnableShadow = false;
        spec.EnablePicking = false;
        spec.EnableOutline = false;
        spec.EnableDebugVisualize = false;
        spec.EnablePostProcess = false;

        s_Data->Renderer = CreateRef<SceneRenderer>();
        s_Data->Renderer->Init(spec);
        s_Data->Renderer->SetClearColor({ 0.2f, 0.2f, 0.2f, 1.0f });

        // 2) 球体网格（Material 预览用）
        s_Data->SphereMesh = MeshFactory::CreateSphere();

        // 3) 固定布光：一盏方向光（右上 45°）
        s_Data->FixedLight.DirectionalLightCount = 1;
        DirectionalLightData& mainLight = s_Data->FixedLight.DirectionalLights[0];
        mainLight.Direction = glm::normalize(glm::vec3(-0.3f, -1.0f, -0.5f));
        mainLight.Color = glm::vec3(1.0f);
        mainLight.Intensity = 1.2f;
    }

    void AssetPreviewRenderer::Shutdown()
    {
        if (s_Data && s_Data->Renderer)
        {
            s_Data->Renderer->Shutdown();
        }
        s_Data.reset();
    }

    const Ref<Framebuffer>& AssetPreviewRenderer::RenderMaterial(const Ref<Material>& material)
    {
        static Ref<Framebuffer> s_Null;
        if (!material || !s_Data || !s_Data->SphereMesh)
        {
            return s_Null;
        }

        // 球体半径 0.5，相机距离 1.5 单位能完整框住
        BuildFixedCamera(glm::vec3(0.0f), 1.5f);

        s_Data->Renderer->BeginScene(s_Data->FixedCamera, s_Data->FixedLight);
        std::vector<Ref<Material>> materials = { material };
        Ref<Mesh> sphere = s_Data->SphereMesh;
        s_Data->Renderer->SubmitMesh(glm::mat4(1.0f), sphere, materials);
        s_Data->Renderer->EndScene();

        return s_Data->Renderer->GetFramebuffer();
    }

    const Ref<Framebuffer>& AssetPreviewRenderer::RenderMesh(const Ref<Mesh>& mesh)
    {
        static Ref<Framebuffer> s_Null;
        if (!mesh || !s_Data)
        {
            return s_Null;
        }

        const AABB& bounds = mesh->GetBoundingBox();
        float radius = glm::length(bounds.Max - bounds.Min) * 0.5f;
        if (radius < 0.001f) { radius = 0.5f; }     // 退化 Mesh 兜底
        float distance = radius / std::sin(glm::radians(s_Data->FixedCamera.FOV) * 0.5f) * 1.3f;
        BuildFixedCamera(bounds.GetCenter(), distance);

        s_Data->Renderer->BeginScene(s_Data->FixedCamera, s_Data->FixedLight);
        std::vector<Ref<Material>> materials = { Renderer3D::GetDefaultMaterial() };
        Ref<Mesh> target = mesh;
        s_Data->Renderer->SubmitMesh(glm::mat4(1.0f), target, materials);
        s_Data->Renderer->EndScene();

        return s_Data->Renderer->GetFramebuffer();
    }

    uint32_t AssetPreviewRenderer::GetPreviewSize()
    {
        return s_PreviewSize;
    }
}
```

> **注意（务必）**：`SubmitMesh` 签名是 `SubmitMesh(const glm::mat4&, Ref<Mesh>&, const std::vector<Ref<Material>>&, int)`，第二参是**非 const 引用**。直接把 `s_Data->SphereMesh` 当实参传会编译失败，必须用一个 **非 const 的局部 `Ref<Mesh>` 承接**（上面示例的 `Ref<Mesh> sphere = ...` / `Ref<Mesh> target = mesh`）。

**需要在 Mesh 上补的接口**：

```cpp
// Lucky/Source/Lucky/Renderer/Mesh.h
struct AABB
{
    glm::vec3 Min{ 0.0f };
    glm::vec3 Max{ 0.0f };
    glm::vec3 GetCenter() const { return (Min + Max) * 0.5f; }
};

class Mesh : public Asset
{
public:
    // ... 原有接口
    /// <summary>
    /// 获取局部空间轴对齐包围盒
    /// 首次调用时遍历 m_Vertices 计算并缓存；mesh 顶点在构造后不可变，缓存永久有效
    /// </summary>
    const AABB& GetBoundingBox() const;
private:
    mutable AABB m_BoundingBox{};
    mutable bool m_BoundingBoxValid = false;
};
```

### 4.3 AssetPreviewCache

**职责**：按 `AssetHandle` 缓存预览纹理。miss 时调用 Renderer 渲染，然后用 `glBlitFramebuffer` 把共享 FBO 的 color attachment 拷贝到自己持有的真实 `Texture2D`（Y 翻转），返回这张独立纹理供 UI 使用。

```cpp
#pragma once

#include "Lucky/Core/Base.h"
#include "Lucky/Asset/AssetHandle.h"
#include "Lucky/Asset/AssetType.h"
#include "Lucky/Renderer/Texture.h"

namespace Lucky
{
    /// <summary>
    /// 资产预览缓存：按 AssetHandle 存放独立的 Ref<Texture2D>
    /// 单例，由 EditorLayer::OnAttach 时 Init / OnDetach 时 Shutdown
    /// </summary>
    class AssetPreviewCache
    {
    public:
        static void Init();
        static void Shutdown();

        /// <summary>
        /// 获取（或按需渲染）指定资产的预览纹理
        /// 仅支持 AssetType::Material / AssetType::Mesh，其他类型返回空 Ref
        /// miss 时会立即触发一次离屏渲染 + Blit，返回新建的 Texture2D
        /// </summary>
        /// <param name="handle">资产 Handle，无效时返回空 Ref</param>
        /// <param name="type">资产类型（由上层传入，避免重复查询 AssetManager）</param>
        /// <returns>独立预览纹理（可直接 ImGui::Image 使用）；不支持的类型或资产加载失败时返回空 Ref</returns>
        static const Ref<Texture2D>& GetOrRender(AssetHandle handle, AssetType type);

        /// <summary>
        /// 使指定资产的预览失效，下次 Get 时重新渲染
        /// Material 保存 / Material 属性改动 / Mesh 替换 / 资产重命名移动都应调用
        /// </summary>
        static void Invalidate(AssetHandle handle);

        /// <summary>
        /// 清空所有预览（Project 关闭时调用）
        /// </summary>
        static void Clear();
    };
}
```

**内部数据结构**（.cpp）：

```cpp
#include "lcpch.h"
#include "AssetPreviewCache.h"
#include "AssetPreviewRenderer.h"

#include "Lucky/Asset/AssetManager.h"
#include "Lucky/Renderer/Framebuffer.h"
#include "Lucky/Renderer/Renderer3D.h"
#include "Lucky/Renderer/Material.h"
#include "Lucky/Renderer/Mesh.h"

#include <glad/glad.h>

namespace Lucky
{
    namespace
    {
        struct PreviewEntry
        {
            Ref<Texture2D> Texture;     // 独立持有的 Texture2D，像素由 Blit 填入
        };

        static std::unordered_map<AssetHandle, PreviewEntry> s_Entries;

        /// <summary>
        /// 把源 FBO 的 Color Attachment 0 Blit 到目标 Texture2D（Y 翻转）
        /// 用一个常驻的辅助 FBO 挂目标纹理，调一次 glBlitFramebuffer，用完解绑
        /// Y 翻转通过指定 dstY0 > dstY1 实现，使目标纹理像素行序与 ImGui 常规纹理一致
        /// </summary>
        void BlitFramebufferToTexture(const Ref<Framebuffer>& src, const Ref<Texture2D>& dst, uint32_t size)
        {
            static GLuint s_HelperFBO = 0;
            if (s_HelperFBO == 0)
            {
                glGenFramebuffers(1, &s_HelperFBO);
            }

            glBindFramebuffer(GL_FRAMEBUFFER, s_HelperFBO);
            glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, dst->GetRendererID(), 0);

            GLuint srcFB = /* 从 src 取出 OpenGL FBO ID 的方式 */;
            glBindFramebuffer(GL_READ_FRAMEBUFFER, srcFB);
            glBindFramebuffer(GL_DRAW_FRAMEBUFFER, s_HelperFBO);

            // Y 翻转：dstY0 = size, dstY1 = 0
            glBlitFramebuffer(
                0, 0, size, size,       // src rect
                0, size, size, 0,       // dst rect（Y 翻转）
                GL_COLOR_BUFFER_BIT, GL_NEAREST);

            glBindFramebuffer(GL_FRAMEBUFFER, 0);
        }
    }

    const Ref<Texture2D>& AssetPreviewCache::GetOrRender(AssetHandle handle, AssetType type)
    {
        static Ref<Texture2D> s_Null;

        if (!handle.IsValid())
        {
            return s_Null;
        }
        if (type != AssetType::Material && type != AssetType::Mesh)
        {
            return s_Null;
        }

        auto it = s_Entries.find(handle);
        if (it != s_Entries.end() && it->second.Texture)
        {
            return it->second.Texture;
        }

        // miss：按类型分派
        Ref<Framebuffer> srcFB;
        if (type == AssetType::Material)
        {
            Ref<Material> material = AssetManager::GetAsset<Material>(handle);
            srcFB = AssetPreviewRenderer::RenderMaterial(material);
        }
        else // Mesh
        {
            Ref<Mesh> mesh = AssetManager::GetAsset<Mesh>(handle);
            srcFB = AssetPreviewRenderer::RenderMesh(mesh);
        }

        if (!srcFB)
        {
            return s_Null;
        }

        uint32_t size = AssetPreviewRenderer::GetPreviewSize();
        Ref<Texture2D> target = Texture2D::Create(size, size);
        BlitFramebufferToTexture(srcFB, target, size);

        PreviewEntry& entry = s_Entries[handle];
        entry.Texture = target;
        return entry.Texture;
    }

    void AssetPreviewCache::Invalidate(AssetHandle handle)
    {
        s_Entries.erase(handle);
    }

    void AssetPreviewCache::Clear()
    {
        s_Entries.clear();
    }

    void AssetPreviewCache::Init() { /* 当前无状态；预留接口 */ }
    void AssetPreviewCache::Shutdown() { Clear(); }
}
```

> **Blit 实现要点**：
> - `glBlitFramebuffer` 的 src 要是**真实 GL FBO ID**。Luck3D 的 `Framebuffer` 类目前没有暴露 `GetRendererID()`；本阶段需要在 `Framebuffer.h` 加一个 `uint32_t GetRendererID() const { return m_RendererID; }`（已经有 private 字段，补一个 getter 即可）。
> - 辅助 FBO 用 `static GLuint` 缓存一次，不释放（随进程退出由驱动回收）。
> - 不走 Framebuffer 抽象层是**有意的局部豁免**：这块代码只在 Cache miss 时触发，复杂度和影响面都极小。真要全局性 Blit 封装时再考虑给 Framebuffer 加 `BlitColorToTexture2D(target)`。

---

## 5. UI 集成（P3）

### 5.1 ProjectAssetsPanel::GetThumbnail 真正实现

**现状**：`ProjectAssetsPanel.cpp:656` 恒 `return nullptr`。
**改为**：

```cpp
Ref<Texture2D> ProjectAssetsPanel::GetThumbnail(const std::filesystem::path& filepath)
{
    AssetType type = GetAssetTypeFromPath(filepath);

    // Texture2D 直接返回自身：最快最直观
    if (type == AssetType::Texture2D)
    {
        AssetHandle handle = AssetManager::GetAssetHandle(Project::GetActive()->MakeRelative(filepath));
        return AssetManager::GetAsset<Texture2D>(handle);
    }

    // Material / Mesh 走预览缓存
    if (type == AssetType::Material || type == AssetType::Mesh)
    {
        AssetHandle handle = AssetManager::GetAssetHandle(Project::GetActive()->MakeRelative(filepath));
        return AssetPreviewCache::GetOrRender(handle, type);
    }

    // 其他类型：无缩略图，由调用方 fallback 到 TypeIcon
    return nullptr;
}
```

### 5.2 DrawAssetItem 优先用缩略图

**现状**（`ProjectAssetsPanel.cpp:340`）：

```cpp
const Ref<Texture2D>& icon = isDirectory
    ? EditorIconManager::GetFolderIcon(false)
    : EditorIconManager::GetAssetTypeIcon(GetAssetTypeFromPath(path));
```

**改为**：

```cpp
Ref<Texture2D> icon;
if (isDirectory)
{
    icon = EditorIconManager::GetFolderIcon(false);
}
else
{
    // 优先用缩略图，没有则 fallback 到静态类型图标
    icon = GetThumbnail(path);
    if (!icon)
    {
        icon = EditorIconManager::GetAssetTypeIcon(GetAssetTypeFromPath(path));
    }
}
```

> **注意**：原类型是 `const Ref<Texture2D>&`，改为按值持有 `Ref<Texture2D>`（因为 `GetThumbnail` 可能返回临时值）。后续传入 `UI::BeginRenamableTreeNode` 时按引用传依然可用。

### 5.3 UI::PropertyAsset（本阶段不改）

`UI::PropertyAsset`（`PropertyGrid.h:188~243`）继续用 `EditorIconManager::GetAssetTypeIcon(assetType)`，不接入缩略图。原因：
- Inspector 字段框图标尺寸约 16~20px，128×128 下采样后视觉收益不足
- 一旦接入，每个 MeshRenderer / SpriteRenderer / ScriptComponent 的材质字段 / 纹理字段都要跟 Cache 的失效机制联动，面扩大且难验收
- 真正能发挥缩略图优势的是 MaterialEditor 自己（P4 的 256×256 嵌入预览），不是 Inspector 的小字段

### 5.4 BeginRenamableTreeNode 的兼容性

`UI::BeginRenamableTreeNode` 签名第 1 参是 `const Ref<Texture2D>& icon`，直接传改造后的 `icon` 即可。缩略图尺寸是 128×128，比当前列表格子的图标尺寸大 —— ImGui::Image 会自动按目标 size 采样缩小，不额外处理也能用。未来切 Grid 布局（Unity 风格的大图标视图）时缩略图优势会更明显。

---

## 6. 失效挂接点清单

本阶段**只接 1 处**，其他 2 处留到后续阶段（分层约束，详见下方说明）：

| 阶段 | 挂接点 | 代码位置 | 做什么 |
|---|---|---|---|
| **R35.2 做** | MaterialEditor 的 Save Material 按钮分支（紧跟 `SerializeToFile`） | `Lucky/Source/Lucky/Editor/MaterialEditor.cpp:156` 附近 | `AssetPreviewCache::Invalidate(material->GetHandle());` |
| **P4 做** | MaterialEditor 任何属性改动（实时同步） | `Lucky/Source/Lucky/Editor/MaterialEditor.cpp` 的 `material->MarkDirty()` 调用点 | 包装一个 `MarkDirtyAndInvalidate(material)` helper 替换所有 15 处 `MarkDirty` 调用 |
| **留给 AssetSystem 后续** | AssetManager::DeleteAsset / MoveAsset | `Lucky/Source/Lucky/Asset/AssetManager.cpp` | `AssetPreviewCache::Invalidate(handle);` |

> **为什么本阶段只做 Serialize 挂钩**：
> - `AssetManager` 在 **Lucky/Asset/** 层，`AssetPreviewCache` 在 **Lucky/Editor/Preview/** 层 —— 引擎资产层不应该反向 include 编辑器层。要挂需要先铺 "AssetManager 变更事件总线"（见 §10），不属于本阶段。
> - MaterialEditor 的实时同步属于"编辑中体验"，不保存就不渲染缩略图在当前 Inspector 不显示缩略图的场景下几乎看不出差别，推 P4。
> - DeleteAsset / MoveAsset 的残留 Entry 只是内存占用（每张 128×128 RGBA8 ≈ 64 KB），项目几千资产下也只有 MB 级，MVP 可接受。

---

## 7. 工程集成

### 7.1 初始化顺序

挂到 `EditorLayer::OnAttach / OnDetach`（`Luck3DApp/Source/EditorLayer.cpp:60` 附近，`EditorIconManager::Init` 后面）：

```cpp
// EditorLayer::OnAttach 中（EditorIconManager::Init 之后、创建场景面板之前）
EditorIconManager::Init();          // 已有
ComponentRegistry::RegisterAll();   // 已有
AssetPreviewRenderer::Init();       // 新增（依赖 Renderer3D，已在更外层 Renderer::Init 完成）
AssetPreviewCache::Init();          // 新增

// EditorLayer::OnDetach 中反序
AssetPreviewCache::Shutdown();      // 新增
AssetPreviewRenderer::Shutdown();   // 新增
EditorIconManager::Shutdown();      // 已有
```

> **顺序硬约束**：
> - `AssetPreviewRenderer::Init` 内部实例化 `SceneRenderer`，需要 `Renderer3D` 已初始化。`Renderer3D::Init` 在 `Renderer.cpp::Init`（更外层）中已完成，到 `EditorLayer::OnAttach` 时必然已就绪。
> - `AssetPreviewCache::Shutdown` 必须在 `AssetPreviewRenderer::Shutdown` 之前：Cache 持有的 Texture2D 自己管生命周期（不依赖 Renderer），所以其实两者都可以安全运行，但按反序更规范。

### 7.2 Build-Lucky.lua

无需改动。`Source/**.h / **.cpp` glob 自动包含 `Source/Lucky/Editor/Preview/*`。
新文件加完后在 Luck3D 根目录执行：

```
Vendor\Binaries\Premake\Windows\premake5.exe --file=Build.lua vs2022
```

---

## 8. 验收标准

### 8.1 R35.1 Renderer 骨架

- [ ] `AssetPreviewRenderer::Init()` 不崩溃，内部 SceneRenderer 成功创建
- [ ] 临时测试代码调用 `RenderMaterial(Renderer3D::GetDefaultMaterial())` 返回非空 Framebuffer
- [ ] `ImGui::Image((ImTextureID)fb->GetColorAttachmentRendererID(0), {128, 128}, {0,1}, {1,0})` 能看到灰色球体
- [ ] 调 `RenderMesh(cubeMesh)` 能看到灰色立方体剪影

### 8.2 R35.2 Cache + Blit + 失效

- [ ] 同一 handle 连续 Get 两次，第二次零 OpenGL 调用（命中缓存）
- [ ] 同屏显示 ≥ 3 张不同的 Material 预览，各自颜色正确、互不覆盖
- [ ] 修改一个 Material 的 `u_Albedo` **并按保存按钮后**，资产面板的缩略图在下一帧更新

### 8.3 R35.3 UI 接入

- [ ] 资产面板里 `.lmat` 文件显示带该材质颜色的球体缩略图，不同 Material 可区分
- [ ] 资产面板里 `.lmesh` 文件显示该 Mesh 的默认白色渲染剪影
- [ ] 资产面板里 `.png` 文件直接显示纹理本身
- [ ] 没有缩略图的资产类型（.scene / .cs）继续显示原类型图标，不出现空白
- [ ] Inspector 面板中 Material / Mesh 字段仍显示**静态类型图标**（本阶段不变）

---

## 9. 风险与性能评估

### 9.1 R35.1 阶段方案 A3 的性能

- 128×128 单张 RGBA8 ≈ 64 KB。假设资产面板一次打开显示 50 个 Material + 50 个 Mesh：
  - **首帧渲染耗时**：每张 2~3 个 draw call（Opaque + Skybox 可选），50 张 ≈ 150 draw call。GPU 侧 << 2 ms（对比：主视口 FullHD 常规场景一帧几千 draw call）
  - **A3 的共享 FBO 覆盖问题**：因为所有 Entry 共享同一张 FBO，最后一次渲染会覆盖之前的 TexID 内容。**意味着 R35.1 阶段只有"最后被渲染的那张"是对的**，其余条目像素会被覆盖。R35.1 可接受（先验证流程），R35.2 必须切 A1。

### 9.2 显存

- A1 完成后，每张 128×128 RGBA8 ≈ 64 KB。假设全项目 200 张可预览资产：**12.8 MB**。可接受。
- A1 的 FBO 池上限设为 **256 张**（≈ 16 MB），超出时按 LRU 淘汰（LRU 本身留给 P5，先用无上限）。

### 9.3 多 SceneRenderer 并行调用

- UI 渲染阶段 Application 主循环**单线程**，不存在两个预览请求同时发生的情况。
- 若未来引入异步渲染队列（P5），需确认 `SceneRenderer::BeginScene / EndScene` 的 UBO 覆盖不与主视口竞争。本阶段的"UI 绘制阶段"位于主视口 EndScene 之后、ImGui 真正 submit 之前，UBO 覆盖是无害的（主视口的 UBO 下帧 BeginScene 时会被重写）。

### 9.4 PreviewTexture 的生命周期

- 底层 TexID 归 `AssetPreviewRenderer::s_Data->Renderer` 所有，`PreviewTexture` 析构时**不**删除 OpenGL 纹理。
- 若 `AssetPreviewRenderer::Shutdown` 在 Cache 还活着时被调用，Cache 里的 TexID 全部悬空。约定 **Shutdown 顺序反向**（见 §7.1），Cache 必须先 Shutdown 清空 `s_Entries` 再轮到 Renderer Shutdown。

---

## 10. 后续扩展（本阶段不做）

| 后续阶段 | 内容 | 价值 |
|---|---|---|
| **P2 持久化** | 把渲染结果写到 `.thumb.png` 侧车文件，启动时直接读磁盘；文件 mtime 校验 | 大项目启动零渲染 |
| **P4 MaterialEditor 嵌入 preview** | MaterialEditor 顶部 256×256 球体预览区，鼠标拖拽旋转镜头 | Unity 风格的材质编辑体验 |
| **P5 异步 + LRU** | 每帧渲染预算 N 张；缓存上限 M 条；淘汰最旧条目 | 首次打开大目录不卡；长时间运行显存稳定 |
| **P6 Mesh 预览旋转** | 让 Mesh 缩略图自动绕 Y 轴转 15°，比正视图更能体现立体感 | 视觉可读性 |
| **P7 Scene 预览** | 为 `.luck3d` 场景文件渲染一张"场景顶视图"缩略图 | — |

---

## 11. 一句话任务清单（给编码者）

1. **R35.1**：
   - 给 `Mesh` 补 `AABB` struct + `GetBoundingBox()`
   - 给 `Framebuffer` 补 `GetRendererID() const` getter
   - 新建 `Lucky/Source/Lucky/Editor/Preview/AssetPreviewRenderer.{h,cpp}`
   - 在 `EditorLayer::OnAttach / OnDetach` 挂上 `AssetPreviewRenderer::Init / Shutdown`（紧跟 `EditorIconManager`）
   - 可选：临时点个按钮调 `RenderMaterial(Renderer3D::GetDefaultMaterial())` + `ImGui::Image(FBO.ColorAttachment, {128,128}, {0,1}, {1,0})` 看一眼球体
2. **R35.2**：
   - 新建 `AssetPreviewCache.{h,cpp}`（含 `BlitFramebufferToTexture` 匿名命名空间函数）
   - `MaterialSerializer::SerializeToFile` 返回成功前挂 `AssetPreviewCache::Invalidate(material->GetHandle())`
   - `EditorLayer::OnAttach / OnDetach` 挂上 `AssetPreviewCache::Init / Shutdown`
3. **R35.3**：
   - `ProjectAssetsPanel::GetThumbnail` 按类型分派：Texture 返回自身 / Material / Mesh 走 Cache / 其他 nullptr
   - `DrawAssetItem` 的 `icon` 改按值 `Ref<Texture2D>`，先 `GetThumbnail` 后 fallback 到 `GetAssetTypeIcon`
   - **本阶段不改 `UI::PropertyAsset`**（保持固定类型图标）
   - 根目录执行 `Vendor\Binaries\Premake\Windows\premake5.exe --file=Build.lua vs2022` 重跑
   - 验收 §8.3 的勾选项
