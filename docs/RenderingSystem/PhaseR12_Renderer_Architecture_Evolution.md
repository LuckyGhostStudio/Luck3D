# R12 渲染器架构演进分析

> **文档版本**：v1.1  
> **创建日期**：2026-04-16  
> **更新日期**：2026-04-17  
> **文档说明**：本文档分析当前 `Renderer3D` 的架构问题，参考 Hazel 引擎的渲染器设计，提出渐进式演进方案，涵盖 Renderer2D（2D 批处理渲染器）、SceneRenderer（统一场景渲染器）、渲染器实例化等远期规划。v1.1 版本添加与 R13 渲染状态设计的关联引用。  
> **文档性质**：远期规划分析文档，不涉及当前阶段的代码修改。  
> **关联文档**：  
> - `PhaseR9_DrawCommand_Sorting.md`（DrawCommand 排序 + RenderQueue 演进）  
> - `PhaseR13_RenderState_PerMaterial.md`（Per-Material 渲染状态：CullMode / ZWrite / ZTest / BlendMode / RenderingMode）

---

## 目录

- [一、当前 Renderer3D 架构分析](#一当前-renderer3d-架构分析)
  - [1.1 当前架构概览](#11-当前架构概览)
  - [1.2 问题总结](#12-问题总结)
- [二、Hazel 引擎渲染器架构参考](#二hazel-引擎渲染器架构参考)
  - [2.1 分层架构](#21-分层架构)
  - [2.2 关键设计差异对比](#22-关键设计差异对比)
  - [2.3 Hazel Renderer2D 设计亮点](#23-hazel-renderer2d-设计亮点)
  - [2.4 Hazel SceneRenderer 设计亮点](#24-hazel-scenerenderer-设计亮点)
  - [2.5 值得借鉴的设计](#25-值得借鉴的设计)
- [三、2D Sprite / Line 渲染方案分析](#三2d-sprite--line-渲染方案分析)
  - [3.1 方案对比](#31-方案对比)
  - [3.2 推荐方案：渐进式演进（B → C）](#32-推荐方案渐进式演进b--c)
  - [3.3 Unity 对应关系](#33-unity-对应关系)
- [四、Renderer2D 设计方案](#四renderer2d-设计方案)
  - [4.1 接口设计](#41-接口设计)
  - [4.2 内部数据结构](#42-内部数据结构)
  - [4.3 批处理机制](#43-批处理机制)
  - [4.4 与 Renderer3D 的协作](#44-与-renderer3d-的协作)
- [五、SceneRenderer 设计方案](#五scenerenderer-设计方案)
  - [5.1 接口设计](#51-接口设计)
  - [5.2 渲染流程](#52-渲染流程)
- [六、渐进式演进路线](#六渐进式演进路线)
  - [6.1 短期（不改架构）](#61-短期不改架构)
  - [6.2 中期（引入新模块）](#62-中期引入新模块)
  - [6.3 远期（架构升级）](#63-远期架构升级)
  - [6.4 演进路线图](#64-演进路线图)

---

## 一、当前 Renderer3D 架构分析

### 1.1 当前架构概览

```
SceneViewportPanel::OnUpdate
  → Framebuffer.Bind()
  → RenderCommand::Clear()
  → Scene::OnUpdate()
      → Renderer3D::BeginScene()     // 设置 Camera UBO + Light UBO
      → Renderer3D::DrawMesh() × N   // 遍历 SubMesh → 绑定 Shader → Apply 材质 → DrawIndexedRange
      → Renderer3D::EndScene()       // 当前为空实现
  → Framebuffer.Unbind()
```

当前 `Renderer3D` 的设计特征：

| 特征 | 当前状态 | 说明 |
|------|---------|------|
| **设计模式** | 全 `static` 方法 + 文件级 `static Renderer3DData s_Data` | 不是真正的"渲染器对象"，更像一个工具函数集合 |
| **职责** | 混合了：着色器管理、材质管理、默认纹理管理、UBO 管理、绘制调用 | 单一类承担过多职责 |
| **渲染流程** | `BeginScene → DrawMesh × N → EndScene`（EndScene 为空） | 即时绘制，无排序、无延迟提交 |
| **扩展性** | 只支持 3D Mesh 渲染 | 无法扩展 2D Sprite、Line、Particle 等 |
| **实例化** | 无 | 每个 SubMesh 一次 DrawCall |
| **全局状态** | 全局唯一，无法创建多个渲染器实例 | 无法支持多视口、多相机独立渲染 |

### 1.2 问题总结

| 问题 | 影响 | 紧迫程度 |
|------|------|---------|
| `DrawMesh` 即时绘制，无排序 | 频繁 GPU 状态切换，性能浪费 | ?? 高（R9 已规划） |
| `EndScene` 为空实现 | 无法在帧末统一处理（排序、后处理等） | ?? 高（R9 已规划） |
| 职责过多（ShaderLibrary / DefaultTextures / UBO 等） | 代码耦合，难以维护 | ?? 中 |
| 全 static 设计 | 无法支持多视口、多相机 | ?? 低（远期需求） |
| 只支持 3D Mesh | 无法渲染 2D Sprite、Line、Circle、Text 等 | ?? 中（按需引入） |

**核心结论**：`Renderer3D` 确实需要修改，但应该**渐进式重构**，由具体功能需求驱动，而非一步到位。

---

## 二、Hazel 引擎渲染器架构参考

> 参考项目：`E:\Projects\Hazel`（Hazel Engine by TheCherno）

### 2.1 分层架构

Hazel 的渲染器采用清晰的分层设计：

```
┌─────────────────────────────────────────────────────────┐
│                    SceneRenderer                         │
│  （场景渲染器，实例化对象，管理完整渲染管线）              │
│                                                         │
│  持有：Renderer2D × 2（世界空间 + 屏幕空间）             │
│  持有：DebugRenderer                                     │
│  持有：多种 RenderPass / ComputePass                     │
│  持有：多种 DrawList（Opaque / Transparent / Shadow 等）  │
├─────────────────────────────────────────────────────────┤
│                    Renderer2D                            │
│  （2D 批处理渲染器，实例化对象）                          │
│                                                         │
│  支持：Quad / Line / Circle / Text                       │
│  内部：独立的 VertexBuffer 批处理                         │
├─────────────────────────────────────────────────────────┤
│                    Renderer                              │
│  （底层 API 封装 + RenderCommandQueue）                   │
│                                                         │
│  职责：延迟执行渲染命令、资源管理                         │
└─────────────────────────────────────────────────────────┘
```

### 2.2 关键设计差异对比

| 维度 | Hazel | Luck3D 当前 |
|------|-------|------------|
| **SceneRenderer** | **实例化对象**（`RefCounted`），可创建多个 | 不存在 |
| **Renderer3D** | 不存在（3D 渲染由 SceneRenderer 直接管理） | 全 static，全局唯一 |
| **Renderer2D** | **独立的实例化 2D 渲染器**，支持 Quad/Line/Circle/Text | 不存在 |
| **DebugRenderer** | 独立的调试渲染器，队列化绘制命令 | 不存在（R10 GizmoRenderer 已规划） |
| **RenderPass** | 抽象基类，支持 Input/Output 绑定 | 不存在（R7 已规划） |
| **DrawList** | 多个 `std::map<MeshKey, DrawCommand>`，按类型分类 | 无，即时绘制 |
| **Submit 模式** | `SubmitMesh` 收集 → `FlushDrawList` 统一绘制 | `DrawMesh` 立即绘制 |
| **命令队列** | `RenderCommandBuffer` 延迟执行 | 无 |

### 2.3 Hazel Renderer2D 设计亮点

Hazel 的 `Renderer2D` 是一个**实例化对象**（`RefCounted`），核心设计：

**支持的图元类型**：

| 图元 | 方法 | 说明 |
|------|------|------|
| Quad | `DrawQuad` / `DrawRotatedQuad` / `DrawQuadBillboard` | 支持纹理、颜色、旋转、Billboard |
| Line | `DrawLine` | 支持 `onTop` 参数控制深度测试 |
| Circle | `DrawCircle` / `FillCircle` | 支持线框和填充 |
| Text | `DrawString` | MSDF 字体渲染 |
| AABB | `DrawAABB` | 包围盒线框 |
| Transform | `DrawTransform` | 坐标轴可视化 |

**内部数据结构**：

```
Renderer2D 内部为每种图元维护独立的批处理缓冲区：

┌──────────────────────────────────────────────────┐
│  QuadVertex Buffer    → QuadPass (RenderPass)    │
│  LineVertex Buffer    → LinePass (RenderPass)    │
│  CircleVertex Buffer  → CirclePipeline           │
│  TextVertex Buffer    → TextPass (RenderPass)    │
└──────────────────────────────────────────────────┘

每种图元有独立的：
- VertexBuffer（CPU 端写入，GPU 端上传）
- IndexBuffer
- Material / Shader
- 顶点计数器（用于批处理 flush）
```

**关键特性**：
- 每种图元独立批处理，互不干扰
- 支持 `onTop` 参数（Line/Circle），控制是否忽略深度测试
- 纹理槽自动管理（最多 32 个纹理槽）
- `Flush()` 时统一提交所有批次

### 2.4 Hazel SceneRenderer 设计亮点

Hazel 的 `SceneRenderer` 是整个渲染管线的核心：

**DrawList 分类**：

```cpp
// Hazel 使用多个 DrawList 分别管理不同类型的绘制命令
std::map<MeshKey, DrawCommand> m_DrawList;                    // 不透明物体
std::map<MeshKey, DrawCommand> m_TransparentDrawList;         // 透明物体
std::map<MeshKey, DrawCommand> m_SelectedMeshDrawList;        // 选中物体
std::map<MeshKey, DrawCommand> m_ShadowPassDrawList;          // 阴影 Pass

std::map<MeshKey, StaticDrawCommand> m_StaticMeshDrawList;    // 静态网格
std::map<MeshKey, StaticDrawCommand> m_StaticColliderDrawList; // 碰撞体调试
```

**渲染流程**：

```
BeginScene()
  → 设置 Camera / Light / Scene UBO
  
SubmitMesh() × N / SubmitStaticMesh() × N
  → 收集到对应的 DrawList（按 MeshKey 分组，自动合并实例）
  
EndScene()
  → FlushDrawList()
      → ClearPass()
      → ShadowMapPass()          // 阴影
      → SpotShadowMapPass()      // 聚光灯阴影
      → PreDepthPass()           // 深度预渲染
      → HZBCompute()             // 层次化深度
      → LightCullingPass()       // 光源剔除
      → SkyboxPass()             // 天空盒
      → GeometryPass()           // 几何渲染（不透明 + 透明）
      → GTAOCompute()            // 环境光遮蔽
      → SSRCompute()             // 屏幕空间反射
      → BloomCompute()           // 泛光
      → EdgeDetectionPass()      // 边缘检测（选中描边）
      → CompositePass()          // 最终合成
```

**持有的子渲染器**：

```cpp
Ref<Renderer2D> m_Renderer2D;              // 世界空间 2D 渲染
Ref<Renderer2D> m_Renderer2DScreenSpace;   // 屏幕空间 2D 渲染（UI）
Ref<DebugRenderer> m_DebugRenderer;        // 调试渲染器
```

### 2.5 值得借鉴的设计

| 设计 | Hazel 做法 | 对 Luck3D 的启示 |
|------|-----------|-----------------|
| **SceneRenderer 实例化** | `RefCounted` 对象，可创建多个实例 | 支持多视口（Scene View + Game View + Preview） |
| **Renderer2D 独立且实例化** | SceneRenderer 持有两个 Renderer2D 实例 | 世界空间和屏幕空间 UI 可以独立渲染 |
| **DebugRenderer 队列化** | 绘制命令存入 `std::vector<std::function<void(Ref<Renderer2D>)>>`，每帧统一 flush | 与 R10 GizmoRenderer 的设计思路一致 |
| **DrawList 分类** | 多个 `std::map<MeshKey, DrawCommand>` | 比单一列表更灵活，天然支持多 Pass |
| **Submit 而非 Draw** | `SubmitMesh` / `SubmitStaticMesh` 命名 | 语义更准确，强调"提交"而非"立即绘制" |
| **MeshKey 自动合并** | 相同 Mesh + Material + SubmeshIndex 自动合并为一个 DrawCommand | 天然支持实例化渲染 |

---

## 三、2D Sprite / Line 渲染方案分析

### 3.1 方案对比

| 方案 | 描述 | 优点 | 缺点 |
|------|------|------|------|
| **A：扩展 Renderer3D** | 在 Renderer3D 中添加 DrawSprite/DrawLine | 简单，无需新模块 | 职责膨胀，违反单一职责原则 |
| **B：独立 Renderer2D** | 创建独立的 Renderer2D 类 | 职责清晰，可独立批处理 | 需要协调渲染顺序 |
| **C：统一 SceneRenderer** | 创建 SceneRenderer 持有 Renderer3D + Renderer2D | 最灵活，支持多视口 | 改动最大 |

**推荐路径**：B → C（先独立创建 Renderer2D，后续统一到 SceneRenderer）

### 3.2 推荐方案：渐进式演进（B → C）

#### 阶段 1：当前 → R9 完成后

保持 `Renderer3D` 的 static 设计不变，仅做内部重构（DrawCommand 排序）。这与现有的 R9 规划完全一致。

#### 阶段 2：引入 Renderer2D（独立模块）

当需要 2D Sprite 或 Line 渲染时，创建独立的 `Renderer2D`。初期可以保持 static 设计（与当前架构一致），后续再改为实例化。

#### 阶段 3：R7 完成后 → SceneRenderer

当多 Pass 渲染框架完成后，将 `Renderer3D` 和 `Renderer2D` 统一到 `SceneRenderer` 中。

### 3.3 Unity 对应关系

| Unity 概念 | Luck3D 当前 | 建议的未来实现 |
|------------|------------|--------------|
| **MeshRenderer** | `Renderer3D::DrawMesh` | `SceneRenderer::SubmitMesh` |
| **SpriteRenderer** | 不存在 | `Renderer2D::DrawSprite` |
| **LineRenderer** | 不存在 | `Renderer2D::DrawLine` |
| **TrailRenderer** | 不存在 | `Renderer2D::DrawLine`（连续线段） |
| **ParticleSystem** | 不存在 | 未来独立的 `ParticleRenderer`（使用 Renderer2D 的批处理） |
| **Camera** | `EditorCamera` / `Camera` | 不变 |
| **SRP (Scriptable Render Pipeline)** | 不存在 | `RenderPipeline` + `RenderPass`（R7 规划） |

---

## 四、Renderer2D 设计方案

> 本节描述 Renderer2D 的设计方案。初期采用 static 设计（与当前 Renderer3D 一致），后续可改为实例化。

### 4.1 接口设计

```cpp
class Renderer2D
{
public:
    static void Init();
    static void Shutdown();
    
    static void BeginScene(const glm::mat4& viewProj);
    static void EndScene();  // flush 所有批次
    
    // ---- Sprite 渲染（类似 Unity SpriteRenderer）----
    
    /// 绘制带纹理的 Sprite
    static void DrawSprite(const glm::mat4& transform, const Ref<Texture2D>& texture, 
                           const glm::vec4& color = glm::vec4(1.0f), 
                           float tilingFactor = 1.0f);
    
    /// 绘制纯色 Quad
    static void DrawQuad(const glm::mat4& transform, const glm::vec4& color);
    
    // ---- Line 渲染（类似 Unity LineRenderer）----
    
    /// 绘制线段
    static void DrawLine(const glm::vec3& start, const glm::vec3& end, 
                         const glm::vec4& color = glm::vec4(1.0f),
                         bool onTop = false);
    
    /// 绘制 AABB 包围盒线框
    static void DrawAABB(const glm::vec3& min, const glm::vec3& max, 
                         const glm::mat4& transform,
                         const glm::vec4& color = glm::vec4(1.0f));
    
    // ---- Circle 渲染 ----
    
    /// 绘制圆环（线框）
    static void DrawCircle(const glm::mat4& transform, const glm::vec4& color, 
                           bool onTop = false);
    
    /// 绘制填充圆
    static void FillCircle(const glm::vec3& position, float radius, 
                           const glm::vec4& color, float thickness = 0.05f);
    
    // ---- 统计 ----
    
    struct Statistics
    {
        uint32_t DrawCalls = 0;
        uint32_t QuadCount = 0;
        uint32_t LineCount = 0;
    };
    
    static Statistics GetStats();
    static void ResetStats();
};
```

### 4.2 内部数据结构

```cpp
/// Renderer2D 内部数据
struct Renderer2DData
{
    static const uint32_t MaxQuads = 5000;
    static const uint32_t MaxVertices = MaxQuads * 4;
    static const uint32_t MaxIndices = MaxQuads * 6;
    static const uint32_t MaxTextureSlots = 32;
    
    static const uint32_t MaxLines = 2000;
    static const uint32_t MaxLineVertices = MaxLines * 2;
    
    // ---- Quad 批处理 ----
    
    struct QuadVertex
    {
        glm::vec3 Position;
        glm::vec4 Color;
        glm::vec2 TexCoord;
        float TexIndex;       // 纹理槽索引
        float TilingFactor;
    };
    
    Ref<VertexArray> QuadVertexArray;
    Ref<VertexBuffer> QuadVertexBuffer;
    Ref<Shader> QuadShader;
    
    uint32_t QuadIndexCount = 0;
    QuadVertex* QuadVertexBufferBase = nullptr;  // CPU 端缓冲区起始
    QuadVertex* QuadVertexBufferPtr = nullptr;   // 当前写入位置
    
    glm::vec4 QuadVertexPositions[4];  // 单位 Quad 的四个顶点
    
    // ---- Line 批处理 ----
    
    struct LineVertex
    {
        glm::vec3 Position;
        glm::vec4 Color;
    };
    
    Ref<VertexArray> LineVertexArray;
    Ref<VertexBuffer> LineVertexBuffer;
    Ref<Shader> LineShader;
    
    uint32_t LineVertexCount = 0;
    LineVertex* LineVertexBufferBase = nullptr;
    LineVertex* LineVertexBufferPtr = nullptr;
    
    // ---- 纹理槽 ----
    
    std::array<Ref<Texture2D>, MaxTextureSlots> TextureSlots;
    uint32_t TextureSlotIndex = 1;  // 0 = 白色纹理
    
    Ref<Texture2D> WhiteTexture;
    
    // ---- 统计 ----
    Renderer2D::Statistics Stats;
};
```

### 4.3 批处理机制

```
Renderer2D 的批处理流程：

BeginScene(viewProj)
  → 重置所有批次计数器
  → 上传 Camera UBO（共享 binding=0）

DrawSprite() / DrawQuad() × N
  → 写入 QuadVertex 到 CPU 缓冲区
  → 如果 Quad 批次满了 → 自动 Flush

DrawLine() × N
  → 写入 LineVertex 到 CPU 缓冲区
  → 如果 Line 批次满了 → 自动 Flush

EndScene()
  → Flush()
      → 上传 QuadVertexBuffer → DrawIndexed（一次 DrawCall 绘制所有 Quad）
      → 上传 LineVertexBuffer → DrawArrays（一次 DrawCall 绘制所有 Line）
```

**关键优势**：
- 多个 Sprite / Line 合并为一次 DrawCall
- 纹理槽自动管理，最多 32 个不同纹理在一个批次中
- 批次满时自动 Flush，对调用者透明

### 4.4 与 Renderer3D 的协作

```
Scene::OnUpdate()
  → 收集光源数据
  → Renderer3D::BeginScene(camera, lightData)   // 设置 Camera UBO (binding=0) + Light UBO (binding=1)
  → Renderer3D::DrawMesh() × N                  // 3D 物体
  → Renderer3D::EndScene()
  
  → Renderer2D::BeginScene(camera.GetViewProjectionMatrix())  // 共享 Camera UBO
  → Renderer2D::DrawSprite() × N                              // 2D Sprite
  → Renderer2D::DrawLine() × N                                // 线段
  → Renderer2D::EndScene()                                    // Flush 所有 2D 批次
```

**注意**：
- Renderer2D 与 Renderer3D 共享 Camera UBO（binding=0）
- 先渲染 3D 物体，再渲染 2D 物体（2D 物体通常在 3D 之上）
- 如果需要 2D 物体参与深度排序，可以在 Renderer2D 中启用深度测试

---

## 五、SceneRenderer 设计方案

> 本节描述远期的 SceneRenderer 统一场景渲染器设计。需要 R7 多 Pass 框架完成后才有实施价值。

### 5.1 接口设计

```cpp
/// 统一场景渲染器（实例化对象，支持多视口）
class SceneRenderer
{
public:
    SceneRenderer();
    ~SceneRenderer();
    
    void Init();
    void Shutdown();
    
    void SetViewportSize(uint32_t width, uint32_t height);
    
    // ---- 场景渲染 ----
    
    void BeginScene(const EditorCamera& camera, const SceneLightData& lightData);
    void BeginScene(const Camera& camera, const glm::mat4& transform, const SceneLightData& lightData);
    void EndScene();  // 执行完整的渲染管线
    
    // ---- 3D 渲染提交 ----
    
    void SubmitMesh(const glm::mat4& transform, Ref<Mesh>& mesh, 
                    const std::vector<Ref<Material>>& materials);
    
    // ---- 2D 渲染提交 ----
    
    void SubmitSprite(const glm::mat4& transform, const Ref<Texture2D>& texture, 
                      const glm::vec4& color = glm::vec4(1.0f));
    
    // ---- 获取子渲染器 ----
    
    Renderer2D& GetRenderer2D() { return m_Renderer2D; }
    DebugRenderer& GetDebugRenderer() { return m_DebugRenderer; }
    
    // ---- 获取渲染结果 ----
    
    Ref<Framebuffer> GetFinalFramebuffer() { return m_FinalFramebuffer; }
    
private:
    void FlushDrawList();
    
    // ---- 渲染 Pass ----
    void ShadowPass();
    void OpaquePass();
    void TransparentPass();
    void PostProcessPass();
    void DebugPass();
    
private:
    // 子渲染器
    Renderer2D m_Renderer2D;
    DebugRenderer m_DebugRenderer;
    
    // DrawList
    std::vector<DrawCommand> m_OpaqueCommands;
    std::vector<DrawCommand> m_TransparentCommands;
    std::vector<DrawCommand> m_ShadowCommands;
    
    // UBO
    Ref<UniformBuffer> m_CameraUBO;
    Ref<UniformBuffer> m_LightUBO;
    
    // Framebuffer
    Ref<Framebuffer> m_FinalFramebuffer;
    Ref<Framebuffer> m_ShadowMapFBO;
    
    // 统计
    Statistics m_Statistics;
    
    uint32_t m_ViewportWidth = 0;
    uint32_t m_ViewportHeight = 0;
};
```

### 5.2 渲染流程

```
SceneRenderer::BeginScene(camera, lightData)
  → 设置 Camera UBO + Light UBO
  → 清空所有 DrawList

SceneRenderer::SubmitMesh() × N
  → 根据材质的 RenderQueue 分类到 OpaqueCommands / TransparentCommands

SceneRenderer::EndScene()
  → FlushDrawList()
      → ShadowPass()         // 阴影 Pass（使用 m_ShadowCommands）
      → OpaquePass()         // 不透明 Pass（使用 m_OpaqueCommands，前向排序）
      → TransparentPass()    // 透明 Pass（使用 m_TransparentCommands，后向排序）
      → m_Renderer2D.Flush() // 2D 批处理 Flush
      → DebugPass()          // 调试渲染（Gizmo、线框等）
      → PostProcessPass()    // 后处理（Bloom、Tonemapping 等）
```

---

## 六、渐进式演进路线

### 6.1 短期（不改架构，仅内部优化）

| 优先级 | 建议 | 对应规划 | 说明 |
|--------|------|---------|------|
| ?? 最高 | 实现 R9 DrawCommand 排序 | `PhaseR9_DrawCommand_Sorting.md` | 仅修改 Renderer3D.cpp 内部，API 不变 |
| ?? 中 | 实现 R10 GizmoRenderer | `PhaseR10_Gizmo_Rendering.md` | 独立渲染路径，不影响主渲染 |

### 6.2 中期（引入新模块）

| 优先级 | 建议 | 说明 |
|--------|------|------|
| ?? 高 | 实现 R4/R5/R6 | 阴影、HDR、后处理 |
| ?? 中 | 创建 `Renderer2D` | 独立的 2D 批处理渲染器，支持 Sprite/Line/Circle |
| ?? 中 | 实现 R13 Per-Material 渲染状态 | 参见 `PhaseR13_RenderState_PerMaterial.md`，为透明物体、双面渲染、特殊效果提供基础 |
| ?? 中 | 将 ShaderLibrary 从 Renderer3D 中分离 | 移到 `Renderer` 或独立的 `ShaderManager` |

### 6.3 远期（架构升级）

| 优先级 | 建议 | 说明 |
|--------|------|------|
| ?? 中 | 实现 R7 多 Pass 渲染框架 | `RenderPass` 基类 + `RenderPipeline` |
| ?? 中 | 将 Renderer3D 从 static 改为实例化 | 支持多视口、多相机 |
| ?? 低 | 引入 `SceneRenderer` 统一管理 | 持有 Renderer3D + Renderer2D + DebugRenderer |

### 6.4 演进路线图

```
当前状态                    阶段 1                     阶段 2                     阶段 3
(Renderer3D 全 static)     (内部重构)                  (引入新模块)               (架构升级)
     │                       │                          │                          │
     ▼                       ▼                          ▼                          ▼
┌─────────┐           ┌──────────────┐          ┌──────────────┐          ┌──────────────┐
│当前状态  │           │ R9 DrawCmd   │          │ R4 阴影       │          │ R7 多Pass    │
│全 static │           │ 排序+延迟提交 │          │ R5 HDR        │          │ RenderPass   │
│即时绘制  │           │ EndScene 实现 │          │ R6 后处理     │          │ RenderPipeline│
│单一职责  │           │ API 不变     │          │              │          │              │
└─────────┘           └──────────────┘          └──────────────┘          └──────────────┘
                            │                         │                         │
                      ┌──────────────┐          ┌──────────────┐          ┌──────────────┐
                      │ R10 Gizmo    │          │ Renderer2D   │          │ SceneRenderer│
                      │ 独立渲染路径  │          │ 2D 批处理     │          │ 统一管理     │
                      │              │          │ Sprite/Line  │          │ 实例化对象   │
                      └──────────────┘          └──────────────┘          └──────────────┘
                            │                         │                         │
                      ┌──────────────┐          ┌──────────────┐          ┌──────────────┐
                      │ R11 模型导入  │          │ ShaderLib    │          │ 渲染器实例化 │
                      │ Assimp       │          │ 从 R3D 分离  │          │ 多视口支持   │
                      └──────────────┘          └──────────────┘          └──────────────┘
```

**核心原则**：

> **不要提前引入不需要的抽象。每次改造都应该由具体的功能需求驱动，而不是为了"架构完整性"。**

- 当前的 `Renderer3D` static 设计在 R9 阶段完全够用，不需要提前重构为实例化
- 当需要透明物体或双面渲染时，引入 R13 Per-Material 渲染状态（参见 `PhaseR13_RenderState_PerMaterial.md`）
- 当真正需要 2D 渲染时，再引入 `Renderer2D`
- 当多 Pass 框架成熟后，再引入 `SceneRenderer` 统一管理
- 每一步都应该是**最小改动**，确保系统始终可用