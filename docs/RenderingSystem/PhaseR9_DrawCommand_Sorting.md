# Phase R9：渲染排序 + 延迟提交（DrawCommand）

> **文档版本**：v1.1  
> **创建日期**：2026-04-15  
> **更新日期**：2026-04-16  
> **优先级**：?? P1（最高优先级）  
> **预计工作量**：3-4 天  
> **前置依赖**：无（可独立实施）  
> **文档说明**：本文档详细描述如何将当前的"立即绘制"模式改造为"收集 + 排序 + 批量执行"模式，解决透明物体渲染错误和 Shader 状态频繁切换问题。改动仅限 `Renderer3D.cpp` 内部，外部 API 完全不变。所有代码可直接对照实现。

---

## 目录

- [一、现状分析](#一现状分析)
- [二、改进目标](#二改进目标)
- [三、涉及的文件清单](#三涉及的文件清单)
- [四、方案设计](#四方案设计)
  - [4.1 DrawCommand 结构](#41-drawcommand-结构)
  - [4.2 排序键设计](#42-排序键设计)
  - [4.3 改造后的渲染流程](#43-改造后的渲染流程)
- [五、详细实现](#五详细实现)
  - [5.1 DrawCommand 定义](#51-drawcommand-定义)
  - [5.2 Renderer3DData 修改](#52-renderer3ddata-修改)
  - [5.3 BeginScene 修改](#53-beginscene-修改)
  - [5.4 DrawMesh 修改](#54-drawmesh-修改)
  - [5.5 EndScene 实现](#55-endscene-实现)
  - [5.6 排序实现](#56-排序实现)
  - [5.7 批量绘制实现](#57-批量绘制实现)
- [六、透明物体支持（预留）](#六透明物体支持预留)
- [七、渲染队列演进计划（RenderQueue）](#七渲染队列演进计划renderqueue)
  - [7.1 与 Unity 渲染队列的对比](#71-与-unity-渲染队列的对比)
  - [7.2 演进阶段一：Material 添加 RenderQueue 字段](#72-演进阶段一material-添加-renderqueue-字段)
  - [7.3 演进阶段二：SortKey 重新设计](#73-演进阶段二sortkey-重新设计)
  - [7.4 演进阶段三：EndScene 分区绘制](#74-演进阶段三endscene-分区绘制)
  - [7.5 演进阶段四：Inspector UI 暴露 RenderQueue](#75-演进阶段四inspector-ui-暴露-renderqueue)
  - [7.6 演进路线总结](#76-演进路线总结)
- [八、验证方法](#八验证方法)
- [九、为未来扩展打下的基础](#九为未来扩展打下的基础)
- [十、设计决策记录](#十设计决策记录)

---

## 一、现状分析

> 基于 2026-04-15 的实际代码状态。

### 当前渲染流程

```cpp
// Scene::OnUpdate 中
Renderer3D::BeginScene(camera, sceneLightData);
{
    auto meshGroup = m_Registry.group<TransformComponent>(entt::get<MeshFilterComponent, MeshRendererComponent>);
    for (auto entity : meshGroup)
    {
        auto [transform, meshFilter, meshRenderer] = meshGroup.get<...>(entity);
        Renderer3D::DrawMesh(transform.GetTransform(), meshFilter.Mesh, meshRenderer.Materials);
    }
}
Renderer3D::EndScene();  // 当前为空实现
```

### 当前 DrawMesh 实现（立即绘制）

```cpp
void Renderer3D::DrawMesh(const glm::mat4& transform, Ref<Mesh>& mesh, const std::vector<Ref<Material>>& materials)
{
    // 准备顶点数据
    s_Data.MeshVertexBufferData.clear();
    for (const Vertex& vertex : mesh->GetVertices()) { ... }
    mesh->SetVertexBufferData(...);
    
    // 立即绘制每个 SubMesh
    for (const SubMesh& sm : mesh->GetSubMeshes())
    {
        Ref<Material> material = materials[sm.MaterialIndex];
        material->GetShader()->Bind();                          // 绑定 Shader
        material->GetShader()->SetMat4("u_ObjectToWorldMatrix", transform);
        material->Apply();                                      // 上传材质参数
        RenderCommand::DrawIndexedRange(mesh->GetVertexArray(), sm.IndexOffset, sm.IndexCount);
    }
}
```

### 问题

| 编号 | 问题 | 影响 |
|------|------|------|
| R9-01 | 物体按 entt 注册顺序绘制，没有任何排序 | 透明物体渲染错误 |
| R9-02 | 相同 Shader/材质的物体没有聚合 | 频繁切换 Shader 状态，性能浪费 |
| R9-03 | `EndScene()` 为空实现 | 无法在渲染结束时执行排序或后处理 |
| R9-04 | 每次 `DrawMesh` 都重新拷贝顶点数据 | 不必要的 CPU 开销 |

### 问题详解

```
当前 DrawMesh 流程（立即绘制）：
  物体 A（Shader=Standard, Material=M1）→ 绑定 Standard → Apply M1 → Draw
  物体 B（Shader=Custom,   Material=M2）→ 绑定 Custom   → Apply M2 → Draw
  物体 C（Shader=Standard, Material=M1）→ 绑定 Standard → Apply M1 → Draw  ← 重复绑定！
  物体 D（Shader=Standard, Material=M3）→ 绑定 Standard → Apply M3 → Draw

问题：Shader Standard 被绑定了 3 次，其中 2 次是不必要的
```

---

## 二、改进目标

1. **延迟提交**：`DrawMesh` 不再立即绘制，而是收集 DrawCommand
2. **排序绘制**：`EndScene` 中对 DrawCommand 排序后批量执行
3. **减少状态切换**：相同 Shader/Material 的物体聚合绘制
4. **外部 API 不变**：`BeginScene`/`DrawMesh`/`EndScene` 签名完全不变
5. **为后续扩展打基础**：Shadow Pass、后处理、透明排序等

---

## 三、涉及的文件清单

| 文件路径 | 操作 | 说明 |
|---------|------|------|
| `Lucky/Source/Lucky/Renderer/Renderer3D.h` | 不变 | 外部 API 完全不变 |
| `Lucky/Source/Lucky/Renderer/Renderer3D.cpp` | **修改** | 核心改造：DrawCommand 收集 + 排序 + 批量绘制 |
| `Lucky/Source/Lucky/Scene/Scene.cpp` | 不变 | 调用方式不变 |

> **注意**：这是一个纯内部重构，仅修改 `Renderer3D.cpp` 一个文件。

---

## 四、方案设计

### 4.1 DrawCommand 结构

每个 DrawCommand 描述一次 DrawCall 所需的全部信息：

```cpp
struct DrawCommand
{
    glm::mat4 Transform;            // 模型变换矩阵
    Ref<Mesh> MeshData;             // 网格引用
    const SubMesh* SubMeshPtr;      // SubMesh 指针
    Ref<Material> MaterialData;     // 材质
    uint64_t SortKey;               // 排序键（用于不透明物体排序）
    float DistanceToCamera;         // 到相机的距离（用于透明物体排序）
};
```

### 4.2 排序键设计

```
SortKey（64 位）：
┌──────────┬──────────────────┬──────────────────┐
│ 16 bit   │ 24 bit           │ 24 bit           │
│ ShaderID │ MaterialHash     │ MeshHash         │
└──────────┴──────────────────┴──────────────────┘

不透明物体：按 SortKey 升序排序
  → 相同 Shader 的物体聚合在一起
  → 相同 Shader 内，相同 Material 的物体聚合
  → 减少 GPU 状态切换

透明物体（预留）：按 DistanceToCamera 降序排序
  → 从远到近绘制，确保混合正确
```

### 4.3 改造后的渲染流程

```
BeginScene()
  → 设置 Camera/Light UBO（不变）
  → 清空 DrawCommand 列表

DrawMesh() × N
  → 不立即绘制
  → 为每个 SubMesh 生成 DrawCommand 并加入列表
  → 计算 SortKey

EndScene()
  → 对 DrawCommand 列表按 SortKey 排序
  → 遍历排序后的列表，执行实际绘制
  → 跟踪当前绑定的 Shader/Material，避免重复绑定
  → 清空 DrawCommand 列表
```

---

## 五、详细实现

### 5.1 DrawCommand 定义

在 `Renderer3D.cpp` 文件顶部（`Renderer3DData` 之前）定义：

```cpp
/// <summary>
/// 绘制命令：描述一次 DrawCall 所需的全部信息
/// </summary>
struct DrawCommand
{
    glm::mat4 Transform;            // 模型变换矩阵
    Ref<Mesh> MeshData;             // 网格引用
    const SubMesh* SubMeshPtr;      // SubMesh 指针（指向 Mesh 内部数据，生命周期由 Mesh 保证）
    Ref<Material> MaterialData;     // 材质引用
    uint64_t SortKey = 0;           // 排序键
    float DistanceToCamera = 0.0f;  // 到相机的距离
};
```

### 5.2 Renderer3DData 修改

```cpp
struct Renderer3DData
{
    // ... 现有成员不变 ...
    
    // ---- DrawCommand 系统（新增） ----
    std::vector<DrawCommand> OpaqueCommands;        // 不透明物体绘制命令列表
    // std::vector<DrawCommand> TransparentCommands; // 透明物体绘制命令列表（预留）
    
    glm::vec3 CameraPosition;                       // 缓存相机位置（用于计算距离）
};
```

### 5.3 BeginScene 修改

```cpp
void Renderer3D::BeginScene(const EditorCamera& camera, const SceneLightData& lightData)
{
    // Camera UBO（不变）
    s_Data.CameraBuffer.ViewProjectionMatrix = camera.GetViewProjectionMatrix();
    s_Data.CameraBuffer.Position = camera.GetPosition();
    s_Data.CameraUniformBuffer->SetData(&s_Data.CameraBuffer, sizeof(Renderer3DData::CameraUBOData));
    
    // Light UBO（不变）
    // ... 现有光照 UBO 设置代码不变 ...
    
    // 清空绘制命令列表（新增）
    s_Data.OpaqueCommands.clear();
    // s_Data.TransparentCommands.clear();  // 预留
    
    // 缓存相机位置（新增）
    s_Data.CameraPosition = camera.GetPosition();
}
```

### 5.4 DrawMesh 修改

```cpp
void Renderer3D::DrawMesh(const glm::mat4& transform, Ref<Mesh>& mesh, const std::vector<Ref<Material>>& materials)
{
    // 准备顶点数据（暂时保留，后续优化可移除）
    s_Data.MeshVertexBufferData.clear();
    for (const Vertex& vertex : mesh->GetVertices())
    {
        s_Data.MeshVertexBufferData.push_back(vertex);
    }
    uint32_t dataSize = sizeof(Vertex) * static_cast<uint32_t>(s_Data.MeshVertexBufferData.size());
    mesh->SetVertexBufferData(s_Data.MeshVertexBufferData.data(), dataSize);
    
    // 计算物体到相机的距离
    glm::vec3 objPos = glm::vec3(transform[3]);
    float distToCamera = glm::length(s_Data.CameraPosition - objPos);
    
    // 为每个 SubMesh 生成 DrawCommand（新增）
    for (const SubMesh& sm : mesh->GetSubMeshes())
    {
        // 获取该 SubMesh 对应的材质
        Ref<Material> material = nullptr;
        if (sm.MaterialIndex < materials.size())
        {
            material = materials[sm.MaterialIndex];
        }
        
        // 材质不存在时使用内部错误材质
        if (!material || !material->GetShader())
        {
            material = s_Data.InternalErrorMaterial;
        }
        
        // 计算排序键
        uint64_t shaderID = static_cast<uint64_t>(material->GetShader()->GetRendererID());
        uint64_t sortKey = (shaderID & 0xFFFF) << 48;  // 高 16 位：Shader ID
        
        // 构建 DrawCommand
        DrawCommand cmd;
        cmd.Transform = transform;
        cmd.MeshData = mesh;
        cmd.SubMeshPtr = &sm;
        cmd.MaterialData = material;
        cmd.SortKey = sortKey;
        cmd.DistanceToCamera = distToCamera;
        
        // 加入不透明命令列表（当前所有物体都视为不透明）
        s_Data.OpaqueCommands.push_back(cmd);
    }
}
```

### 5.5 EndScene 实现

```cpp
void Renderer3D::EndScene()
{
    // ---- 排序不透明物体 ----
    std::sort(s_Data.OpaqueCommands.begin(), s_Data.OpaqueCommands.end(),
        [](const DrawCommand& a, const DrawCommand& b)
        {
            return a.SortKey < b.SortKey;   // 按 SortKey 升序（聚合相同 Shader）
        }
    );
    
    // ---- 批量绘制不透明物体 ----
    uint32_t lastShaderID = 0;  // 跟踪上一次绑定的 Shader
    
    for (const DrawCommand& cmd : s_Data.OpaqueCommands)
    {
        // 绑定 Shader（仅在 Shader 变化时绑定）
        uint32_t currentShaderID = cmd.MaterialData->GetShader()->GetRendererID();
        if (currentShaderID != lastShaderID)
        {
            cmd.MaterialData->GetShader()->Bind();
            lastShaderID = currentShaderID;
        }
        
        // 设置引擎内部 uniform
        cmd.MaterialData->GetShader()->SetMat4("u_ObjectToWorldMatrix", cmd.Transform);
        
        // 应用材质属性
        cmd.MaterialData->Apply();
        
        // 绘制
        RenderCommand::DrawIndexedRange(
            cmd.MeshData->GetVertexArray(),
            cmd.SubMeshPtr->IndexOffset,
            cmd.SubMeshPtr->IndexCount
        );
        
        s_Data.Stats.DrawCalls++;
        s_Data.Stats.TriangleCount += cmd.SubMeshPtr->IndexCount / 3;
    }
    
    // ---- 清空命令列表 ----
    s_Data.OpaqueCommands.clear();
}
```

### 5.6 排序实现

当前阶段仅实现不透明物体的 Shader 聚合排序。排序策略：

```
不透明物体排序规则：
  1. 首先按 ShaderID 排序（减少 Shader 切换）
  2. 相同 Shader 内按 MaterialHash 排序（减少 Material Apply 次数）
  
排序后的绘制顺序示例：
  物体 A（Shader=Standard, Material=M1）→ 绑定 Standard → Apply M1 → Draw
  物体 C（Shader=Standard, Material=M1）→ （Shader 不变）→ Apply M1 → Draw  ← 可进一步优化
  物体 D（Shader=Standard, Material=M3）→ （Shader 不变）→ Apply M3 → Draw
  物体 B（Shader=Custom,   Material=M2）→ 绑定 Custom   → Apply M2 → Draw

Shader 绑定次数：从 3 次减少到 2 次
```

### 5.7 批量绘制实现

批量绘制的核心是**跟踪当前 GPU 状态**，避免重复绑定：

```cpp
// 状态跟踪变量
uint32_t lastShaderID = 0;

for (const DrawCommand& cmd : s_Data.OpaqueCommands)
{
    uint32_t currentShaderID = cmd.MaterialData->GetShader()->GetRendererID();
    
    // 仅在 Shader 变化时绑定
    if (currentShaderID != lastShaderID)
    {
        cmd.MaterialData->GetShader()->Bind();
        lastShaderID = currentShaderID;
    }
    
    // 设置 per-object uniform（每个物体都不同，必须设置）
    cmd.MaterialData->GetShader()->SetMat4("u_ObjectToWorldMatrix", cmd.Transform);
    
    // 应用材质属性（后续可优化：相同材质跳过 Apply）
    cmd.MaterialData->Apply();
    
    // 绘制
    RenderCommand::DrawIndexedRange(cmd.MeshData->GetVertexArray(), 
                                     cmd.SubMeshPtr->IndexOffset, 
                                     cmd.SubMeshPtr->IndexCount);
}
```

---

## 六、透明物体支持（预留）

当前阶段所有物体都视为不透明。后续添加透明物体支持时：

1. 在 `Material` 中添加 `RenderMode`（Opaque / Transparent）
2. `DrawMesh` 中根据 `RenderMode` 分别加入 `OpaqueCommands` 或 `TransparentCommands`
3. `EndScene` 中：
   - 先绘制不透明物体（按 SortKey 升序）
   - 再绘制透明物体（按 DistanceToCamera 降序，从远到近）
   - 透明物体绘制时禁用深度写入、启用混合

```cpp
// EndScene 中的透明物体绘制（预留）
// 排序：从远到近
std::sort(s_Data.TransparentCommands.begin(), s_Data.TransparentCommands.end(),
    [](const DrawCommand& a, const DrawCommand& b)
    {
        return a.DistanceToCamera > b.DistanceToCamera;
    }
);

// 设置渲染状态
RenderCommand::EnableDepthWrite(false);
RenderCommand::EnableBlending(true);

for (const DrawCommand& cmd : s_Data.TransparentCommands)
{
    // ... 绘制逻辑同不透明物体 ...
}

// 恢复渲染状态
RenderCommand::EnableDepthWrite(true);
RenderCommand::EnableBlending(false);
```

---

## 七、渲染队列演进计划（RenderQueue）

> **说明**：当前 R9 阶段仅实现 ShaderID 聚合排序（所有物体视为不透明），这是正确的起步策略。本章节描述如何在后续阶段平滑演进到 Unity 风格的渲染队列机制，使用户可以通过修改材质的渲染队列值来控制渲染顺序。

### 7.1 与 Unity 渲染队列的对比

#### Unity 的渲染队列机制

Unity 的排序分为两层：

```
┌─────────────────────────────────────────────────────────────────┐
│                    Unity 渲染排序（两层）                         │
├─────────────────────────────────────────────────────────────────┤
│                                                                 │
│  第一层：RenderQueue（渲染队列值）                                │
│    ┌──────────────────────────────────────────────────────────┐ │
│    │ Background  = 1000   // 天空盒等                         │ │
│    │ Geometry    = 2000   // 默认不透明物体                    │ │
│    │ AlphaTest   = 2450   // Alpha 测试物体                   │ │
│    │ Transparent = 3000   // 透明物体                         │ │
│    │ Overlay     = 4000   // UI 叠加层                        │ │
│    │                                                          │ │
│    │ 用户可以设置任意整数值（如 2100），插入到任意位置           │ │
│    └──────────────────────────────────────────────────────────┘ │
│                                                                 │
│  第二层：队列内部排序                                            │
│    ┌──────────────────────────────────────────────────────────┐ │
│    │ Queue < 2500（不透明区）：按 Shader/Material 聚合排序     │ │
│    │                          + 从前到后（减少 Overdraw）      │ │
│    │                                                          │ │
│    │ Queue >= 2500（透明区）：从后到前（保证混合正确）          │ │
│    └──────────────────────────────────────────────────────────┘ │
│                                                                 │
└─────────────────────────────────────────────────────────────────┘
```

#### 当前 R9 设计 vs Unity

| 维度 | Unity | 当前 R9 设计 |
|------|-------|-------------|
| **排序层级** | 两层（Queue + 队列内排序） | 单层（仅 Shader 聚合） |
| **用户可控性** | ? 材质可设置 RenderQueue 值 | ? 用户无法控制排序 |
| **透明/不透明分离** | ? 以 Queue=2500 为分界线 | ? 全部视为不透明 |
| **自定义插入点** | ? 任意整数值 | ? 无 |
| **排序键最高位** | RenderQueue 值 | ShaderID |

> **结论**：当前 R9 的 DrawCommand + SortKey 架构是正确的基础设施，只需要小幅扩展即可达到 Unity 风格的行为。

---

### 7.2 演进阶段一：Material 添加 RenderQueue 字段

> **触发时机**：当需要透明物体支持或用户需要控制渲染顺序时实施。

在 `Material` 类中新增渲染队列字段：

```cpp
// Material.h 中新增
class Material
{
public:
    // ---- 渲染队列预定义值（与 Unity 一致） ----
    enum RenderQueue : int32_t
    {
        Background  = 1000,     // 天空盒等最先绘制的物体
        Geometry    = 2000,     // 默认不透明物体
        AlphaTest   = 2450,     // Alpha 测试物体
        Transparent = 3000,     // 透明物体
        Overlay     = 4000,     // UI 叠加层等最后绘制的物体
    };
    
    /// <summary>
    /// 设置渲染队列值（值越小越先绘制）
    /// </summary>
    void SetRenderQueue(int32_t queue) { m_RenderQueue = queue; }
    
    /// <summary>
    /// 获取渲染队列值
    /// </summary>
    int32_t GetRenderQueue() const { return m_RenderQueue; }
    
    /// <summary>
    /// 是否为透明物体（Queue >= 2500）
    /// </summary>
    bool IsTransparent() const { return m_RenderQueue >= 2500; }

private:
    int32_t m_RenderQueue = RenderQueue::Geometry;  // 默认：不透明几何体
};
```

**改动量**：Material.h 新增 1 个枚举 + 3 个方法 + 1 个成员变量。

---

### 7.3 演进阶段二：SortKey 重新设计

将 SortKey 的最高位从 ShaderID 改为 RenderQueue，使渲染队列成为排序的第一优先级：

```
改进后的 SortKey（64 位）：
┌──────────────┬──────────┬──────────────────┬──────────────────┐
│ 16 bit       │ 16 bit   │ 16 bit           │ 16 bit           │
│ RenderQueue  │ ShaderID │ MaterialHash     │ DistanceKey      │
└──────────────┴──────────┴──────────────────┴──────────────────┘

不透明物体（Queue < 2500）：
  → 按 RenderQueue 升序 → ShaderID 聚合 → 从前到后

透明物体（Queue >= 2500）：
  → 按 RenderQueue 升序 → 从后到前（DistanceKey 取反）
```

```cpp
/// <summary>
/// 构建排序键（渲染队列优先）
/// </summary>
uint64_t BuildSortKey(int32_t renderQueue, uint32_t shaderID, float distToCamera, bool isTransparent)
{
    uint64_t key = 0;
    
    // 最高 16 位：RenderQueue（决定大的渲染顺序）
    key |= (static_cast<uint64_t>(renderQueue) & 0xFFFF) << 48;
    
    // 次高 16 位：ShaderID（聚合相同 Shader）
    key |= (static_cast<uint64_t>(shaderID) & 0xFFFF) << 32;
    
    // 低 32 位：距离键
    if (isTransparent)
    {
        // 透明物体：距离越远，键值越小（从后到前绘制）
        uint32_t distKey = static_cast<uint32_t>(distToCamera * 1000.0f);
        key |= static_cast<uint64_t>(0xFFFFFFFF - distKey);  // 取反
    }
    else
    {
        // 不透明物体：距离越近，键值越小（从前到后，减少 Overdraw）
        uint32_t distKey = static_cast<uint32_t>(distToCamera * 1000.0f);
        key |= static_cast<uint64_t>(distKey);
    }
    
    return key;
}
```

**改动量**：修改 `DrawMesh` 中的 SortKey 计算逻辑（约 5 行代码）。

---

### 7.4 演进阶段三：EndScene 分区绘制

根据 RenderQueue 值自动分区，不透明和透明物体使用不同的渲染状态：

```cpp
void Renderer3D::EndScene()
{
    // 统一排序（RenderQueue 在最高位，自然分区）
    std::sort(s_Data.OpaqueCommands.begin(), s_Data.OpaqueCommands.end(),
        [](const DrawCommand& a, const DrawCommand& b) { return a.SortKey < b.SortKey; }
    );
    
    uint32_t lastShaderID = 0;
    bool inTransparentRegion = false;
    
    for (const DrawCommand& cmd : s_Data.OpaqueCommands)
    {
        bool isTransparent = cmd.MaterialData->IsTransparent();
        
        // 进入透明区域时切换渲染状态
        if (isTransparent && !inTransparentRegion)
        {
            RenderCommand::EnableDepthWrite(false);
            RenderCommand::EnableBlending(true);
            inTransparentRegion = true;
            lastShaderID = 0;  // 重置 Shader 跟踪
        }
        
        // 绑定 Shader（仅在变化时）
        uint32_t currentShaderID = cmd.MaterialData->GetShader()->GetRendererID();
        if (currentShaderID != lastShaderID)
        {
            cmd.MaterialData->GetShader()->Bind();
            lastShaderID = currentShaderID;
        }
        
        cmd.MaterialData->GetShader()->SetMat4("u_ObjectToWorldMatrix", cmd.Transform);
        cmd.MaterialData->Apply();
        RenderCommand::DrawIndexedRange(cmd.MeshData->GetVertexArray(),
                                         cmd.SubMeshPtr->IndexOffset,
                                         cmd.SubMeshPtr->IndexCount);
    }
    
    // 恢复渲染状态
    if (inTransparentRegion)
    {
        RenderCommand::EnableDepthWrite(true);
        RenderCommand::EnableBlending(false);
    }
    
    s_Data.OpaqueCommands.clear();
}
```

**改动量**：修改 `EndScene` 中的绘制循环（约 10 行新增代码）。

---

### 7.5 演进阶段四：Inspector UI 暴露 RenderQueue

在材质 Inspector 面板中添加 RenderQueue 编辑控件，使用户可以像 Unity 一样直接修改渲染队列值：

```cpp
// InspectorPanel.cpp 中材质编辑器部分
int renderQueue = material->GetRenderQueue();
if (ImGui::DragInt("Render Queue", &renderQueue, 1, 0, 5000))
{
    material->SetRenderQueue(renderQueue);
}

// 快捷预设按钮
if (ImGui::Button("Background"))  material->SetRenderQueue(1000);
ImGui::SameLine();
if (ImGui::Button("Geometry"))    material->SetRenderQueue(2000);
ImGui::SameLine();
if (ImGui::Button("AlphaTest"))   material->SetRenderQueue(2450);
ImGui::SameLine();
if (ImGui::Button("Transparent")) material->SetRenderQueue(3000);
ImGui::SameLine();
if (ImGui::Button("Overlay"))     material->SetRenderQueue(4000);
```

**改动量**：InspectorPanel.cpp 新增约 15 行 UI 代码。

---

### 7.6 演进路线总结

```
┌─────────────────┐     ┌─────────────────┐     ┌─────────────────┐     ┌─────────────────┐
│  R9 当前设计     │     │  阶段一          │     │  阶段二+三       │     │  阶段四          │
│                 │     │                 │     │                 │     │                 │
│ ShaderID 聚合   │ ──→ │ Material 加     │ ──→ │ SortKey 最高位  │ ──→ │ Inspector UI   │
│ 排序            │     │ RenderQueue     │     │ 改为 Queue +   │     │ 暴露 Queue 编辑│
│ 仅不透明        │     │ 字段            │     │ 透明分区绘制    │     │ + 预设按钮      │
│                 │     │                 │     │                 │     │                 │
│ 改动：0         │     │ 改动：Material.h│     │ 改动：          │     │ 改动：          │
│ （当前阶段）     │     │ 加 1 个字段     │     │ Renderer3D.cpp │     │ InspectorPanel │
│                 │     │ 5 分钟          │     │ 约 15 行        │     │ 约 15 行        │
└─────────────────┘     └─────────────────┘     └─────────────────┘     └─────────────────┘
```

| 阶段 | 改动量 | 触发时机 | 达成效果 |
|------|--------|---------|----------|
| **R9 当前** | 基础实现 | 立即 | ShaderID 聚合排序，减少状态切换 |
| **阶段一** | Material.h 加 1 个字段 | 需要透明物体时 | 材质具备渲染队列属性 |
| **阶段二+三** | Renderer3D.cpp 约 15 行 | 同阶段一 | 完整的不透明/透明分区绘制，与 Unity 行为一致 |
| **阶段四** | InspectorPanel.cpp 约 15 行 | 同阶段一 | 用户可在编辑器中直接修改渲染队列值 |

> **核心原则**：R9 先按当前设计实现（当前场景全是不透明物体，RenderQueue 没有实际用途），等到需要透明物体支持时，再花很小的代价加上 RenderQueue，即可获得与 Unity 完全一致的渲染队列行为。

---

## 八、验证方法

### 8.1 基础功能验证

1. 创建多个 Cube，确认渲染结果与改造前完全一致
2. 创建不同材质的物体，确认材质正确应用
3. 保存/加载场景，确认序列化不受影响

### 8.2 排序验证

1. 创建多个使用不同 Shader 的物体
2. 在 `EndScene` 中添加日志，输出排序后的 DrawCommand 顺序
3. 确认相同 Shader 的物体被聚合在一起

### 8.3 性能验证

1. 创建 50+ 个物体（混合使用 Standard 和 InternalError Shader）
2. 对比改造前后的 Shader 绑定次数
3. 确认 DrawCall 数量不变，但 Shader 切换次数减少

### 8.4 统计数据验证

1. 确认 `Renderer3D::GetStats()` 返回正确的 DrawCall 和三角形数量
2. 确认统计数据与改造前一致

---

## 九、为未来扩展打下的基础

| 后续功能 | 如何利用 DrawCommand |
|---------|---------------------|
| **阴影映射（R4）** | Shadow Pass 复用 DrawCommand 列表，用深度 Shader 重新绘制一遍不透明物体 |
| **HDR + Tonemapping（R5）** | 在 `EndScene` 的绘制完成后，添加 Tonemapping Pass |
| **后处理（R6）** | 在 `EndScene` 末尾执行后处理链 |
| **多 Pass 渲染（R7）** | DrawCommand 列表可以被多个 Pass 复用 |
| **渲染队列（R9+）** | SortKey 最高位改为 RenderQueue，支持 Unity 风格的渲染队列控制 |
| **透明物体** | 基于 RenderQueue >= 2500 自动分区，透明物体从后到前绘制 |
| **实例化渲染** | 相同 Mesh + Material 的 DrawCommand 可以合并为 Instanced Draw |

---

## 十、设计决策记录

| 决策 | 选择 | 原因 |
|------|------|------|
| 改动范围 | 仅 `Renderer3D.cpp` | 最小化风险，外部 API 不变 |
| 排序算法 | `std::sort` | 简单高效，DrawCommand 数量通常 < 1000 |
| 排序键 | 64 位整数（ShaderID 优先） | Shader 切换是最昂贵的状态变更 |
| 透明物体 | 预留接口，暂不实现 | 当前场景无透明物体需求 |
| 渲染队列 | 预留演进方案，暂不实现 | 当前场景全为不透明物体，RenderQueue 无实际用途；架构已支持平滑演进 |
| 顶点数据拷贝 | 暂时保留 | 后续优化可直接使用 Mesh 内部数据 |
| 状态跟踪 | 仅跟踪 ShaderID | 简单有效，后续可扩展跟踪 MaterialID |
| DrawCommand 存储 | `std::vector` | 简单，支持随机访问和排序 |
