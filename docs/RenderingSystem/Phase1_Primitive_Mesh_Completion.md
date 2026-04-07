# Phase 1：补全基础图元网格 + 统一工厂入口

> **文档版本**：v1.0  
> **创建日期**：2026-04-07  
> **最后更新**：2026-04-07  
> **前置依赖**：无（当前 `MeshFactory` 已实现 `CreateCube()`）  
> **文档说明**：本文档详细描述如何补全所有已声明的原生图元网格（Plane、Sphere、Cylinder、Capsule），添加统一的 `CreatePrimitive` 工厂入口，并将 `MeshFilterComponent` 中的创建逻辑解耦。所有代码可直接对照实现。

---

## 目录

- [一、现状分析](#一现状分析)
  - [1.1 已实现的功能](#11-已实现的功能)
  - [1.2 现有问题清单](#12-现有问题清单)
- [二、改进目标](#二改进目标)
- [三、涉及的文件](#三涉及的文件)
- [四、PrimitiveType 枚举（无需修改）](#四primitivetype-枚举无需修改)
- [五、MeshFactory 接口改造](#五meshfactory-接口改造)
  - [5.1 新增方法声明](#51-新增方法声明)
  - [5.2 完整的 MeshFactory.h](#52-完整的-meshfactoryh)
- [六、CreatePrimitive 统一入口实现](#六createprimitive-统一入口实现)
  - [6.1 CreatePrimitive 实现](#61-createprimitive-实现)
  - [6.2 GetPrimitiveTypeName 实现](#62-getprimitivetypename-实现)
- [七、各图元创建算法详解](#七各图元创建算法详解)
  - [7.1 CreateCube（已实现，无需修改）](#71-createcube已实现无需修改)
  - [7.2 CreatePlane](#72-createplane)
  - [7.3 CreateSphere](#73-createsphere)
  - [7.4 CreateCylinder](#74-createcylinder)
  - [7.5 CreateCapsule](#75-createcapsule)
- [八、MeshFilterComponent 解耦](#八meshfiltercomponent-解耦)
  - [8.1 当前问题](#81-当前问题)
  - [8.2 改进方案](#82-改进方案)
- [九、编辑器菜单集成](#九编辑器菜单集成)
  - [9.1 当前状态](#91-当前状态)
  - [9.2 改进方案](#92-改进方案)
- [十、完整代码修改清单](#十完整代码修改清单)
  - [10.1 MeshFactory.h 完整代码](#101-meshfactoryh-完整代码)
  - [10.2 MeshFactory.cpp 完整代码](#102-meshfactorycpp-完整代码)
  - [10.3 MeshFilterComponent.h 修改](#103-meshfiltercomponenth-修改)
  - [10.4 SceneHierarchyPanel.cpp 修改](#104-scenehierarchypanelcpp-修改)
- [十一、文件变更总览](#十一文件变更总览)
- [十二、验证方法](#十二验证方法)
- [十三、设计决策记录](#十三设计决策记录)

---

## 一、现状分析

### 1.1 已实现的功能

| 功能 | 状态 | 说明 |
|------|------|------|
| `PrimitiveType` 枚举 | ? 已定义 | 包含 None、Cube、Plane、Sphere、Cylinder、Capsule |
| `MeshFactory::CreateCube()` | ? 已实现 | 24 顶点、36 索引、独立面法线、正确 UV |
| `MeshFactory::CreatePlane()` | ? 未实现 | 枚举中已声明但无对应方法 |
| `MeshFactory::CreateSphere()` | ? 未实现 | 枚举中已声明但无对应方法 |
| `MeshFactory::CreateCylinder()` | ? 未实现 | 枚举中已声明但无对应方法 |
| `MeshFactory::CreateCapsule()` | ? 未实现 | 枚举中已声明但无对应方法 |
| `CreatePrimitive` 统一入口 | ? 不存在 | 创建逻辑分散在 `MeshFilterComponent` 中 |

### 1.2 现有问题清单

| 编号 | 问题 | 影响 | 优先级 |
|------|------|------|--------|
| **M-01** | 枚举声明了 5 种图元但只实现了 Cube | 使用 Plane/Sphere 等类型时 Mesh 为空指针，导致崩溃 | ?? 高 |
| **M-02** | 没有统一的 `CreatePrimitive` 入口 | 每个使用方都需要自己写 switch 分支 | ?? 中 |
| **M-03** | `MeshFilterComponent` 构造函数中硬编码 switch | 新增图元类型时需要同步修改组件代码，违反开闭原则 | ?? 中 |
| **M-04** | 编辑器创建菜单只有 Cube | 用户无法通过 UI 创建其他图元 | ?? 中 |
| **M-05** | 场景反序列化时非 Cube 的 PrimitiveType 会导致空 Mesh | 保存了 Plane 类型的场景文件无法正确加载 | ?? 高 |

---

## 二、改进目标

1. **补全所有图元创建方法**：实现 `CreatePlane()`、`CreateSphere()`、`CreateCylinder()`、`CreateCapsule()`
2. **添加统一工厂入口**：`CreatePrimitive(PrimitiveType)` 方法，根据类型自动分发
3. **解耦 MeshFilterComponent**：组件构造函数不再维护 switch 分支，统一调用 `CreatePrimitive`
4. **补全编辑器菜单**：在 `SceneHierarchyPanel` 的创建菜单中添加所有图元类型
5. **确保序列化兼容**：所有 `PrimitiveType` 都能正确序列化和反序列化

---

## 三、涉及的文件

### 需要修改的文件

| 文件路径 | 说明 |
|---------|------|
| `Lucky/Source/Lucky/Renderer/MeshFactory.h` | 添加新的工厂方法声明 |
| `Lucky/Source/Lucky/Renderer/MeshFactory.cpp` | 实现所有图元创建方法 |
| `Lucky/Source/Lucky/Scene/Components/MeshFilterComponent.h` | 简化构造函数，使用 `CreatePrimitive` |
| `Luck3DApp/Source/Panels/SceneHierarchyPanel.cpp` | 添加 Plane/Sphere/Cylinder/Capsule 创建菜单项 |

### 不需要修改的文件

| 文件路径 | 原因 |
|---------|------|
| `Lucky/Source/Lucky/Renderer/Mesh.h` | Vertex 结构和 Mesh 类接口不变 |
| `Lucky/Source/Lucky/Renderer/Mesh.cpp` | Mesh 构造逻辑不变 |
| `Lucky/Source/Lucky/Scene/SceneSerializer.cpp` | 反序列化通过 `MeshFilterComponent(PrimitiveType)` 构造，自动受益于本次改动 |

---

## 四、PrimitiveType 枚举（无需修改）

当前枚举定义已经完整，无需修改：

```cpp
enum class PrimitiveType
{
    None = 0,
    
    Cube,       // = 1
    Plane,      // = 2
    Sphere,     // = 3
    Cylinder,   // = 4
    Capsule,    // = 5
};
```

> **注意**：枚举值的整数映射已被场景序列化使用（`PrimitiveType: 1` 表示 Cube），因此**不能修改已有枚举项的顺序或值**。新增枚举项只能追加在末尾。

---

## 五、MeshFactory 接口改造

### 5.1 新增方法声明

| 方法 | 参数 | 说明 |
|------|------|------|
| `CreatePrimitive(PrimitiveType)` | 图元类型 | 统一入口，根据类型分发到具体创建方法 |
| `CreatePlane(uint32_t)` | 细分次数（默认 1） | 创建 XZ 平面 |
| `CreateSphere(uint32_t, uint32_t)` | 经线段数（默认 32）、纬线段数（默认 16） | 创建 UV 球体 |
| `CreateCylinder(uint32_t)` | 圆周段数（默认 32） | 创建圆柱体 |
| `CreateCapsule(uint32_t, uint32_t)` | 圆周段数（默认 32）、半球纬线段数（默认 8） | 创建胶囊体 |
| `GetPrimitiveTypeName(PrimitiveType)` | 图元类型 | 返回图元类型名称字符串（private） |

### 5.2 完整的 MeshFactory.h

```cpp
#pragma once

#include "Mesh.h"

namespace Lucky
{
    /// <summary>
    /// 图元类型
    /// </summary>
    enum class PrimitiveType
    {
        None = 0,
        
        Cube,
        Plane,
        Sphere,
        Cylinder,
        Capsule,
    };
    
    /// <summary>
    /// 网格工厂：用于创建原生图元网格
    /// </summary>
    class MeshFactory
    {
    public:
        /// <summary>
        /// 根据图元类型创建网格（统一入口）
        /// </summary>
        /// <param name="type">图元类型</param>
        /// <returns>创建的网格，类型为 None 时返回 nullptr</returns>
        static Ref<Mesh> CreatePrimitive(PrimitiveType type);
        
        /// <summary>
        /// 创建立方体网格
        /// 中心在原点，边长为 1（范围 [-0.5, 0.5]）
        /// 24 个顶点（每面 4 个独立顶点，用于独立法线）
        /// 36 个索引（12 个三角形）
        /// </summary>
        /// <returns>立方体网格</returns>
        static Ref<Mesh> CreateCube();
        
        /// <summary>
        /// 创建平面网格
        /// XZ 平面，中心在原点，大小为 1×1（范围 [-0.5, 0.5]）
        /// 法线朝上 (0, 1, 0)
        /// </summary>
        /// <param name="subdivisions">细分次数，默认为 1（即 2 个三角形）
        /// subdivisions=1 时：4 个顶点，6 个索引
        /// subdivisions=10 时：121 个顶点，600 个索引（与 Unity 默认 Plane 一致）</param>
        /// <returns>平面网格</returns>
        static Ref<Mesh> CreatePlane(uint32_t subdivisions = 1);
        
        /// <summary>
        /// 创建球体网格（UV Sphere）
        /// 中心在原点，半径为 0.5（直径为 1）
        /// 使用经纬线（UV Sphere）算法
        /// </summary>
        /// <param name="segments">经线段数（水平方向），默认 32，最小 3</param>
        /// <param name="rings">纬线段数（垂直方向），默认 16，最小 2</param>
        /// <returns>球体网格</returns>
        static Ref<Mesh> CreateSphere(uint32_t segments = 32, uint32_t rings = 16);
        
        /// <summary>
        /// 创建圆柱体网格
        /// 中心在原点，高度为 1（范围 Y: [-0.5, 0.5]），半径为 0.5
        /// 包含顶面、底面和侧面
        /// </summary>
        /// <param name="segments">圆周段数，默认 32，最小 3</param>
        /// <returns>圆柱体网格</returns>
        static Ref<Mesh> CreateCylinder(uint32_t segments = 32);
        
        /// <summary>
        /// 创建胶囊体网格
        /// 中心在原点，总高度为 2（圆柱部分高度 1 + 上下半球各半径 0.5）
        /// 半径为 0.5
        /// </summary>
        /// <param name="segments">圆周段数，默认 32，最小 3</param>
        /// <param name="rings">每个半球的纬线段数，默认 8，最小 2</param>
        /// <returns>胶囊体网格</returns>
        static Ref<Mesh> CreateCapsule(uint32_t segments = 32, uint32_t rings = 8);
        
    private:
        /// <summary>
        /// 获取图元类型对应的名称字符串
        /// </summary>
        /// <param name="type">图元类型</param>
        /// <returns>名称字符串</returns>
        static const char* GetPrimitiveTypeName(PrimitiveType type);
    };
}
```

---

## 六、CreatePrimitive 统一入口实现

### 6.1 CreatePrimitive 实现

```cpp
Ref<Mesh> MeshFactory::CreatePrimitive(PrimitiveType type)
{
    Ref<Mesh> mesh = nullptr;
    
    switch (type)
    {
        case PrimitiveType::Cube:       mesh = CreateCube();        break;
        case PrimitiveType::Plane:      mesh = CreatePlane();       break;
        case PrimitiveType::Sphere:     mesh = CreateSphere();      break;
        case PrimitiveType::Cylinder:   mesh = CreateCylinder();    break;
        case PrimitiveType::Capsule:    mesh = CreateCapsule();     break;
        default:
            LF_CORE_WARN("MeshFactory::CreatePrimitive: Unknown PrimitiveType {0}", (int)type);
            return nullptr;
    }
    
    if (mesh)
    {
        mesh->SetName(GetPrimitiveTypeName(type));
    }
    
    return mesh;
}
```

### 6.2 GetPrimitiveTypeName 实现

```cpp
const char* MeshFactory::GetPrimitiveTypeName(PrimitiveType type)
{
    switch (type)
    {
        case PrimitiveType::Cube:       return "Cube";
        case PrimitiveType::Plane:      return "Plane";
        case PrimitiveType::Sphere:     return "Sphere";
        case PrimitiveType::Cylinder:   return "Cylinder";
        case PrimitiveType::Capsule:    return "Capsule";
        default:                        return "Unknown";
    }
}
```

---

## 七、各图元创建算法详解

### 7.1 CreateCube（已实现，无需修改）

当前实现已经正确，保持不变。

**规格**：
- 中心在原点，边长为 1
- 24 个顶点（6 面 × 4 顶点/面），每面独立法线
- 36 个索引（6 面 × 2 三角形/面 × 3 索引/三角形）
- UV 映射：每面独立 [0,1]×[0,1]
- 顶点颜色：白色 (1,1,1,1)

> **注意**：`CreateCube()` 内部不再调用 `mesh->SetName("Cube")`，名称统一由 `CreatePrimitive` 设置。但为了保持向后兼容（直接调用 `CreateCube()` 的代码），可以保留内部的 `SetName`，或者不设置让调用方自行处理。**推荐**：不在具体创建方法内设置名称，统一由 `CreatePrimitive` 设置。

---

### 7.2 CreatePlane

#### 7.2.1 几何规格

| 属性 | 值 |
|------|-----|
| 朝向 | XZ 平面，法线朝上 Y+ |
| 中心 | 原点 (0, 0, 0) |
| 大小 | 1×1（X: [-0.5, 0.5]，Z: [-0.5, 0.5]） |
| Y 坐标 | 0 |
| 默认细分 | subdivisions = 1（最简单的平面） |

#### 7.2.2 顶点和索引计算

```
设 subdivisions = N（N ≥ 1）

顶点数 = (N + 1) × (N + 1)
三角形数 = N × N × 2
索引数 = N × N × 6

示例：
  N=1:  4 顶点，2 三角形，6 索引
  N=10: 121 顶点，200 三角形，600 索引（Unity 默认 Plane）
```

#### 7.2.3 算法伪代码

```
对于每个网格点 (i, j)，其中 i ∈ [0, N], j ∈ [0, N]:
    x = -0.5 + i / N          // X 坐标：从 -0.5 到 0.5
    y = 0                      // Y 坐标：固定为 0
    z = -0.5 + j / N          // Z 坐标：从 -0.5 到 0.5
    
    normal = (0, 1, 0)         // 法线朝上
    
    u = i / N                  // UV.x：从 0 到 1
    v = j / N                  // UV.y：从 0 到 1
    
    color = (1, 1, 1, 1)      // 白色

对于每个网格单元 (i, j)，其中 i ∈ [0, N-1], j ∈ [0, N-1]:
    topLeft     = i * (N + 1) + j
    topRight    = i * (N + 1) + (j + 1)
    bottomLeft  = (i + 1) * (N + 1) + j
    bottomRight = (i + 1) * (N + 1) + (j + 1)
    
    // 三角形 1（逆时针，从上方看）
    indices: topLeft, bottomLeft, topRight
    
    // 三角形 2
    indices: topRight, bottomLeft, bottomRight
```

#### 7.2.4 完整实现代码

```cpp
Ref<Mesh> MeshFactory::CreatePlane(uint32_t subdivisions)
{
    // 参数校验
    if (subdivisions < 1)
        subdivisions = 1;
    
    uint32_t vertexCount = (subdivisions + 1) * (subdivisions + 1);
    uint32_t indexCount = subdivisions * subdivisions * 6;
    
    std::vector<Vertex> vertices;
    vertices.reserve(vertexCount);
    
    std::vector<uint32_t> indices;
    indices.reserve(indexCount);
    
    // 生成顶点
    for (uint32_t i = 0; i <= subdivisions; ++i)
    {
        for (uint32_t j = 0; j <= subdivisions; ++j)
        {
            float x = -0.5f + static_cast<float>(i) / static_cast<float>(subdivisions);
            float z = -0.5f + static_cast<float>(j) / static_cast<float>(subdivisions);
            
            float u = static_cast<float>(i) / static_cast<float>(subdivisions);
            float v = static_cast<float>(j) / static_cast<float>(subdivisions);
            
            vertices.push_back({
                { x, 0.0f, z },             // Position
                { 1.0f, 1.0f, 1.0f, 1.0f }, // Color（白色）
                { 0.0f, 1.0f, 0.0f },       // Normal（朝上）
                { u, v }                     // TexCoord
            });
        }
    }
    
    // 生成索引
    for (uint32_t i = 0; i < subdivisions; ++i)
    {
        for (uint32_t j = 0; j < subdivisions; ++j)
        {
            uint32_t topLeft     = i * (subdivisions + 1) + j;
            uint32_t topRight    = i * (subdivisions + 1) + (j + 1);
            uint32_t bottomLeft  = (i + 1) * (subdivisions + 1) + j;
            uint32_t bottomRight = (i + 1) * (subdivisions + 1) + (j + 1);
            
            // 三角形 1
            indices.push_back(topLeft);
            indices.push_back(bottomLeft);
            indices.push_back(topRight);
            
            // 三角形 2
            indices.push_back(topRight);
            indices.push_back(bottomLeft);
            indices.push_back(bottomRight);
        }
    }
    
    return CreateRef<Mesh>(vertices, indices);
}
```

#### 7.2.5 参数选择说明

| 方案 | subdivisions 默认值 | 优点 | 缺点 |
|------|---------------------|------|------|
| **方案 A：subdivisions = 1（推荐）** | 1 | 最简单，4 顶点 2 三角形，性能最优 | 无法做顶点级变形 |
| 方案 B：subdivisions = 10 | 10 | 与 Unity 默认 Plane 一致，支持顶点变形 | 121 顶点对简单平面来说过多 |

**推荐方案 A**：默认 subdivisions = 1。原因：
1. 当前引擎没有顶点变形需求
2. 用户可以通过参数自行指定更高细分
3. 性能优先

---

### 7.3 CreateSphere

#### 7.3.1 几何规格

| 属性 | 值 |
|------|-----|
| 类型 | UV Sphere（经纬线球体） |
| 中心 | 原点 (0, 0, 0) |
| 半径 | 0.5（直径为 1） |
| 默认 segments | 32（经线段数，水平方向） |
| 默认 rings | 16（纬线段数，垂直方向） |

#### 7.3.2 算法选择

| 方案 | 算法 | 优点 | 缺点 |
|------|------|------|------|
| **方案 A：UV Sphere（推荐）** | 经纬线法 | 实现简单，UV 映射自然，与 Unity/Blender 一致 | 极点处三角形退化（面积趋近于 0） |
| 方案 B：Icosphere | 正二十面体细分 | 三角形分布均匀 | 实现复杂，UV 映射困难 |
| 方案 C：Cube Sphere | 立方体投影到球面 | 三角形较均匀，UV 映射可控 | 实现中等复杂度 |

**推荐方案 A（UV Sphere）**：
1. 与 Unity、Blender 的默认球体一致
2. UV 映射天然对应球面坐标，纹理贴图效果好
3. 实现最简单，极点退化在视觉上可接受

#### 7.3.3 顶点和索引计算

```
设 segments = S（S ≥ 3），rings = R（R ≥ 2）

顶点数 = (S + 1) × (R + 1)
  说明：每一纬线层有 S+1 个顶点（首尾重复以闭合 UV），共 R+1 层（从北极到南极）

索引数 = S × R × 6
  说明：每个网格单元 2 个三角形 × 3 索引

示例（默认参数 S=32, R=16）：
  顶点数 = 33 × 17 = 561
  索引数 = 32 × 16 × 6 = 3072
```

#### 7.3.4 算法伪代码

```
半径 radius = 0.5

对于每一纬线层 ring ∈ [0, R]:
    phi = π × ring / R                    // 从北极(0)到南极(π)
    y = radius × cos(phi)                  // Y 坐标
    ringRadius = radius × sin(phi)         // 当前纬线圈的半径
    
    对于每一经线段 seg ∈ [0, S]:
        theta = 2π × seg / S              // 从 0 到 2π
        x = ringRadius × cos(theta)        // X 坐标
        z = ringRadius × sin(theta)        // Z 坐标
        
        normal = normalize(x, y, z)        // 法线 = 归一化位置（球心在原点）
        
        u = seg / S                        // UV.x：从 0 到 1
        v = ring / R                       // UV.y：从 0（北极）到 1（南极）
        
        color = (1, 1, 1, 1)

对于每个网格单元 (ring, seg)，其中 ring ∈ [0, R-1], seg ∈ [0, S-1]:
    current     = ring * (S + 1) + seg
    next        = current + 1
    below       = (ring + 1) * (S + 1) + seg
    belowNext   = below + 1
    
    // 三角形 1
    indices: current, below, next
    
    // 三角形 2
    indices: next, below, belowNext
```

#### 7.3.5 完整实现代码

```cpp
Ref<Mesh> MeshFactory::CreateSphere(uint32_t segments, uint32_t rings)
{
    // 参数校验
    if (segments < 3) segments = 3;
    if (rings < 2) rings = 2;
    
    const float radius = 0.5f;
    const float PI = glm::pi<float>();
    
    uint32_t vertexCount = (segments + 1) * (rings + 1);
    uint32_t indexCount = segments * rings * 6;
    
    std::vector<Vertex> vertices;
    vertices.reserve(vertexCount);
    
    std::vector<uint32_t> indices;
    indices.reserve(indexCount);
    
    // 生成顶点
    for (uint32_t ring = 0; ring <= rings; ++ring)
    {
        float phi = PI * static_cast<float>(ring) / static_cast<float>(rings);  // [0, π]
        float y = radius * cos(phi);
        float ringRadius = radius * sin(phi);
        
        for (uint32_t seg = 0; seg <= segments; ++seg)
        {
            float theta = 2.0f * PI * static_cast<float>(seg) / static_cast<float>(segments);  // [0, 2π]
            float x = ringRadius * cos(theta);
            float z = ringRadius * sin(theta);
            
            // 法线 = 归一化的位置向量（球心在原点）
            glm::vec3 normal = glm::normalize(glm::vec3(x, y, z));
            
            float u = static_cast<float>(seg) / static_cast<float>(segments);
            float v = static_cast<float>(ring) / static_cast<float>(rings);
            
            vertices.push_back({
                { x, y, z },                 // Position
                { 1.0f, 1.0f, 1.0f, 1.0f }, // Color
                normal,                      // Normal
                { u, v }                     // TexCoord
            });
        }
    }
    
    // 生成索引
    for (uint32_t ring = 0; ring < rings; ++ring)
    {
        for (uint32_t seg = 0; seg < segments; ++seg)
        {
            uint32_t current   = ring * (segments + 1) + seg;
            uint32_t next      = current + 1;
            uint32_t below     = (ring + 1) * (segments + 1) + seg;
            uint32_t belowNext = below + 1;
            
            // 三角形 1
            indices.push_back(current);
            indices.push_back(below);
            indices.push_back(next);
            
            // 三角形 2
            indices.push_back(next);
            indices.push_back(below);
            indices.push_back(belowNext);
        }
    }
    
    return CreateRef<Mesh>(vertices, indices);
}
```

#### 7.3.6 关于极点处理的说明

上述实现中，北极和南极各有 `segments + 1` 个重叠顶点（位置相同但 UV 不同）。这是 UV Sphere 的标准做法：

- **优点**：UV 映射连续，纹理不会在极点处撕裂
- **缺点**：极点处有退化三角形（面积趋近于 0）

这与 Unity 和 Blender 的 UV Sphere 行为一致，不需要特殊处理。

---

### 7.4 CreateCylinder

#### 7.4.1 几何规格

| 属性 | 值 |
|------|-----|
| 中心 | 原点 (0, 0, 0) |
| 高度 | 1（Y: [-0.5, 0.5]） |
| 半径 | 0.5（直径为 1） |
| 默认 segments | 32 |
| 组成部分 | 侧面 + 顶面 + 底面 |

#### 7.4.2 三部分结构

```
圆柱体由三部分组成：

1. 侧面（Side）：
   - 顶点数 = (segments + 1) × 2（上下两圈，首尾重复以闭合 UV）
   - 索引数 = segments × 6
   - 法线：水平向外 (cos(θ), 0, sin(θ))

2. 顶面（Top Cap）：
   - 顶点数 = segments + 1（圆周顶点）+ 1（中心点）= segments + 2
   - 索引数 = segments × 3
   - 法线：朝上 (0, 1, 0)

3. 底面（Bottom Cap）：
   - 顶点数 = segments + 2（同顶面）
   - 索引数 = segments × 3
   - 法线：朝下 (0, -1, 0)

总计：
  顶点数 = (segments + 1) × 2 + (segments + 2) × 2
  索引数 = segments × 6 + segments × 3 × 2 = segments × 12
```

#### 7.4.3 完整实现代码

```cpp
Ref<Mesh> MeshFactory::CreateCylinder(uint32_t segments)
{
    // 参数校验
    if (segments < 3) segments = 3;
    
    const float radius = 0.5f;
    const float halfHeight = 0.5f;
    const float PI = glm::pi<float>();
    
    std::vector<Vertex> vertices;
    std::vector<uint32_t> indices;
    
    // ========== 1. 侧面 ==========
    uint32_t sideBaseIndex = 0;
    
    for (uint32_t seg = 0; seg <= segments; ++seg)
    {
        float theta = 2.0f * PI * static_cast<float>(seg) / static_cast<float>(segments);
        float cosTheta = cos(theta);
        float sinTheta = sin(theta);
        
        float x = radius * cosTheta;
        float z = radius * sinTheta;
        
        glm::vec3 normal = { cosTheta, 0.0f, sinTheta };  // 水平向外
        
        float u = static_cast<float>(seg) / static_cast<float>(segments);
        
        // 上圈顶点
        vertices.push_back({
            { x, halfHeight, z },
            { 1.0f, 1.0f, 1.0f, 1.0f },
            normal,
            { u, 1.0f }
        });
        
        // 下圈顶点
        vertices.push_back({
            { x, -halfHeight, z },
            { 1.0f, 1.0f, 1.0f, 1.0f },
            normal,
            { u, 0.0f }
        });
    }
    
    // 侧面索引
    for (uint32_t seg = 0; seg < segments; ++seg)
    {
        uint32_t topLeft     = sideBaseIndex + seg * 2;
        uint32_t bottomLeft  = topLeft + 1;
        uint32_t topRight    = topLeft + 2;
        uint32_t bottomRight = topLeft + 3;
        
        // 三角形 1
        indices.push_back(topLeft);
        indices.push_back(bottomLeft);
        indices.push_back(topRight);
        
        // 三角形 2
        indices.push_back(topRight);
        indices.push_back(bottomLeft);
        indices.push_back(bottomRight);
    }
    
    // ========== 2. 顶面 ==========
    uint32_t topCapBaseIndex = static_cast<uint32_t>(vertices.size());
    
    // 中心点
    vertices.push_back({
        { 0.0f, halfHeight, 0.0f },
        { 1.0f, 1.0f, 1.0f, 1.0f },
        { 0.0f, 1.0f, 0.0f },       // 法线朝上
        { 0.5f, 0.5f }              // UV 中心
    });
    
    // 圆周顶点
    for (uint32_t seg = 0; seg <= segments; ++seg)
    {
        float theta = 2.0f * PI * static_cast<float>(seg) / static_cast<float>(segments);
        float cosTheta = cos(theta);
        float sinTheta = sin(theta);
        
        vertices.push_back({
            { radius * cosTheta, halfHeight, radius * sinTheta },
            { 1.0f, 1.0f, 1.0f, 1.0f },
            { 0.0f, 1.0f, 0.0f },                                   // 法线朝上
            { cosTheta * 0.5f + 0.5f, sinTheta * 0.5f + 0.5f }     // 圆形 UV 映射
        });
    }
    
    // 顶面索引（扇形三角形）
    for (uint32_t seg = 0; seg < segments; ++seg)
    {
        indices.push_back(topCapBaseIndex);          // 中心点
        indices.push_back(topCapBaseIndex + 1 + seg);
        indices.push_back(topCapBaseIndex + 2 + seg);
    }
    
    // ========== 3. 底面 ==========
    uint32_t bottomCapBaseIndex = static_cast<uint32_t>(vertices.size());
    
    // 中心点
    vertices.push_back({
        { 0.0f, -halfHeight, 0.0f },
        { 1.0f, 1.0f, 1.0f, 1.0f },
        { 0.0f, -1.0f, 0.0f },      // 法线朝下
        { 0.5f, 0.5f }              // UV 中心
    });
    
    // 圆周顶点
    for (uint32_t seg = 0; seg <= segments; ++seg)
    {
        float theta = 2.0f * PI * static_cast<float>(seg) / static_cast<float>(segments);
        float cosTheta = cos(theta);
        float sinTheta = sin(theta);
        
        vertices.push_back({
            { radius * cosTheta, -halfHeight, radius * sinTheta },
            { 1.0f, 1.0f, 1.0f, 1.0f },
            { 0.0f, -1.0f, 0.0f },                                  // 法线朝下
            { cosTheta * 0.5f + 0.5f, sinTheta * 0.5f + 0.5f }     // 圆形 UV 映射
        });
    }
    
    // 底面索引（扇形三角形，注意绕序与顶面相反）
    for (uint32_t seg = 0; seg < segments; ++seg)
    {
        indices.push_back(bottomCapBaseIndex);           // 中心点
        indices.push_back(bottomCapBaseIndex + 2 + seg); // 注意：与顶面相反
        indices.push_back(bottomCapBaseIndex + 1 + seg);
    }
    
    return CreateRef<Mesh>(vertices, indices);
}
```

#### 7.4.4 UV 映射说明

| 部分 | UV 映射方式 |
|------|------------|
| 侧面 | U = θ/2π（水平展开），V = 0（底部）到 1（顶部） |
| 顶面/底面 | 圆形投影映射：U = cos(θ)×0.5+0.5，V = sin(θ)×0.5+0.5 |

---

### 7.5 CreateCapsule

#### 7.5.1 几何规格

| 属性 | 值 |
|------|-----|
| 中心 | 原点 (0, 0, 0) |
| 总高度 | 2.0（圆柱部分高度 1.0 + 上下半球各 0.5） |
| 半径 | 0.5 |
| 默认 segments | 32 |
| 默认 rings | 8（每个半球的纬线段数） |

#### 7.5.2 结构说明

```
胶囊体由三部分组成：

1. 上半球（Top Hemisphere）：
   - 从北极到赤道，rings 层
   - 赤道 Y = halfHeight（圆柱顶部）
   - 北极 Y = halfHeight + radius

2. 圆柱中段（Cylinder Body）：
   - 上圈 Y = halfHeight
   - 下圈 Y = -halfHeight
   - 只有 1 层（2 圈顶点）

3. 下半球（Bottom Hemisphere）：
   - 从赤道到南极，rings 层
   - 赤道 Y = -halfHeight（圆柱底部）
   - 南极 Y = -(halfHeight + radius)

其中 halfHeight = 0.5（圆柱部分半高）
     radius = 0.5
     总高度 = 2 × (halfHeight + radius) = 2.0
```

#### 7.5.3 完整实现代码

```cpp
Ref<Mesh> MeshFactory::CreateCapsule(uint32_t segments, uint32_t rings)
{
    // 参数校验
    if (segments < 3) segments = 3;
    if (rings < 2) rings = 2;
    
    const float radius = 0.5f;
    const float halfHeight = 0.5f;  // 圆柱部分半高
    const float PI = glm::pi<float>();
    
    std::vector<Vertex> vertices;
    std::vector<uint32_t> indices;
    
    // ========== 1. 上半球 ==========
    // 从北极（ring=0）到赤道（ring=rings）
    for (uint32_t ring = 0; ring <= rings; ++ring)
    {
        // phi 从 0（北极）到 π/2（赤道）
        float phi = (PI / 2.0f) * static_cast<float>(ring) / static_cast<float>(rings);
        float y = radius * cos(phi) + halfHeight;  // 偏移到圆柱顶部之上
        float ringRadius = radius * sin(phi);
        
        for (uint32_t seg = 0; seg <= segments; ++seg)
        {
            float theta = 2.0f * PI * static_cast<float>(seg) / static_cast<float>(segments);
            float x = ringRadius * cos(theta);
            float z = ringRadius * sin(theta);
            
            // 法线 = 归一化的球面方向（相对于半球中心）
            glm::vec3 spherePos = { x, radius * cos(phi), z };
            glm::vec3 normal = glm::normalize(spherePos);
            
            float u = static_cast<float>(seg) / static_cast<float>(segments);
            // V 映射：北极 = 1.0，赤道 = 0.75（上半球占 UV 的上 1/4）
            float v = 1.0f - (static_cast<float>(ring) / static_cast<float>(rings)) * 0.25f;
            
            vertices.push_back({
                { x, y, z },
                { 1.0f, 1.0f, 1.0f, 1.0f },
                normal,
                { u, v }
            });
        }
    }
    
    // ========== 2. 下半球 ==========
    // 从赤道（ring=0）到南极（ring=rings）
    // 注意：赤道层与上半球的赤道层共享位置但需要独立顶点（UV 不同）
    for (uint32_t ring = 0; ring <= rings; ++ring)
    {
        // phi 从 π/2（赤道）到 π（南极）
        float phi = (PI / 2.0f) + (PI / 2.0f) * static_cast<float>(ring) / static_cast<float>(rings);
        float y = radius * cos(phi) - halfHeight;  // 偏移到圆柱底部之下
        float ringRadius = radius * sin(phi);
        
        for (uint32_t seg = 0; seg <= segments; ++seg)
        {
            float theta = 2.0f * PI * static_cast<float>(seg) / static_cast<float>(segments);
            float x = ringRadius * cos(theta);
            float z = ringRadius * sin(theta);
            
            glm::vec3 spherePos = { x, radius * cos(phi), z };
            glm::vec3 normal = glm::normalize(spherePos);
            
            float u = static_cast<float>(seg) / static_cast<float>(segments);
            // V 映射：赤道 = 0.25，南极 = 0.0（下半球占 UV 的下 1/4）
            float v = 0.25f - (static_cast<float>(ring) / static_cast<float>(rings)) * 0.25f;
            
            vertices.push_back({
                { x, y, z },
                { 1.0f, 1.0f, 1.0f, 1.0f },
                normal,
                { u, v }
            });
        }
    }
    
    // ========== 3. 生成索引 ==========
    // 上半球索引
    uint32_t topHemiOffset = 0;
    for (uint32_t ring = 0; ring < rings; ++ring)
    {
        for (uint32_t seg = 0; seg < segments; ++seg)
        {
            uint32_t current   = topHemiOffset + ring * (segments + 1) + seg;
            uint32_t next      = current + 1;
            uint32_t below     = current + (segments + 1);
            uint32_t belowNext = below + 1;
            
            indices.push_back(current);
            indices.push_back(below);
            indices.push_back(next);
            
            indices.push_back(next);
            indices.push_back(below);
            indices.push_back(belowNext);
        }
    }
    
    // 中间连接带（上半球赤道层 → 下半球赤道层）
    uint32_t topEquatorOffset = topHemiOffset + rings * (segments + 1);
    uint32_t bottomHemiOffset = (rings + 1) * (segments + 1);
    uint32_t bottomEquatorOffset = bottomHemiOffset;
    
    for (uint32_t seg = 0; seg < segments; ++seg)
    {
        uint32_t topCurrent    = topEquatorOffset + seg;
        uint32_t topNext       = topCurrent + 1;
        uint32_t bottomCurrent = bottomEquatorOffset + seg;
        uint32_t bottomNext    = bottomCurrent + 1;
        
        indices.push_back(topCurrent);
        indices.push_back(bottomCurrent);
        indices.push_back(topNext);
        
        indices.push_back(topNext);
        indices.push_back(bottomCurrent);
        indices.push_back(bottomNext);
    }
    
    // 下半球索引
    for (uint32_t ring = 0; ring < rings; ++ring)
    {
        for (uint32_t seg = 0; seg < segments; ++seg)
        {
            uint32_t current   = bottomHemiOffset + ring * (segments + 1) + seg;
            uint32_t next      = current + 1;
            uint32_t below     = current + (segments + 1);
            uint32_t belowNext = below + 1;
            
            indices.push_back(current);
            indices.push_back(below);
            indices.push_back(next);
            
            indices.push_back(next);
            indices.push_back(below);
            indices.push_back(belowNext);
        }
    }
    
    return CreateRef<Mesh>(vertices, indices);
}
```

#### 7.5.4 UV 映射说明

胶囊体的 UV 映射采用纵向分段方式：

```
V = 1.0  ─── 北极
V = 0.75 ─── 上半球赤道（圆柱顶部）
V = 0.25 ─── 下半球赤道（圆柱底部）
V = 0.0  ─── 南极

U = 0 到 1 ─── 水平环绕
```

> **注意**：圆柱中段的 V 范围为 [0.25, 0.75]，占据了 UV 空间的中间一半，这样纹理在圆柱部分不会被过度拉伸。

---

## 八、MeshFilterComponent 解耦

### 8.1 当前问题

当前 `MeshFilterComponent` 构造函数中硬编码了 switch 分支：

```cpp
MeshFilterComponent(PrimitiveType primitive)
    : Primitive(primitive)
{
    switch (primitive)
    {
        case PrimitiveType::Cube:
        {
            Mesh = MeshFactory::CreateCube();
            Mesh->SetName("Cube");
            break;
        }
        default:
            break;
    }
}
```

**问题**：
1. 每新增一种图元类型，都需要在此处添加 case 分支
2. 名称设置逻辑分散（组件中设置名称，而不是工厂统一设置）
3. 非 Cube 类型会导致 `Mesh` 为空指针

### 8.2 改进方案

使用 `MeshFactory::CreatePrimitive` 统一入口替代 switch：

```cpp
#pragma once

#include "Lucky/Renderer/Mesh.h"
#include "Lucky/Renderer/MeshFactory.h"

namespace Lucky
{
    using MeshRef = Ref<Mesh>;
    
    struct MeshFilterComponent
    {
        MeshRef Mesh;
        PrimitiveType Primitive = PrimitiveType::None;  // Temp
        
        MeshFilterComponent() = default;
        MeshFilterComponent(const MeshFilterComponent& other) = default;
        MeshFilterComponent(const MeshRef& mesh)
            : Mesh(mesh) {}
        
        MeshFilterComponent(PrimitiveType primitive)
            : Primitive(primitive)
        {
            Mesh = MeshFactory::CreatePrimitive(primitive);
        }
    };
}
```

**改进点**：
1. 一行代码替代整个 switch 块
2. 名称由 `CreatePrimitive` 内部统一设置
3. 新增图元类型时 `MeshFilterComponent` 无需任何修改
4. `PrimitiveType::None` 时 `CreatePrimitive` 返回 `nullptr`，行为与之前一致

---

## 九、编辑器菜单集成

### 9.1 当前状态

`SceneHierarchyPanel::DrawEntityCreateMenu` 中只有 Cube 的创建菜单项。

### 9.2 改进方案

#### 方案 A：逐项添加菜单项（推荐）

为每种图元类型添加独立的菜单项，与 Unity 的 `GameObject > 3D Object` 菜单一致：

```cpp
void SceneHierarchyPanel::DrawEntityCreateMenu(Entity parent)
{
    if (!ImGui::BeginMenu("Create"))
    {
        return;
    }
    
    Entity newEntity;
    
    // 创建空物体
    if (ImGui::MenuItem("Create Empty"))
    {
        std::string uniqueName = GenerateUniqueName("Entity", parent);
        newEntity = m_Scene->CreateEntity(uniqueName);
    }
    
    // ---- 3D 图元 ----
    ImGui::Separator();
    
    // 创建 Cube
    if (ImGui::MenuItem("Cube"))
    {
        std::string uniqueName = GenerateUniqueName("Cube", parent);
        newEntity = m_Scene->CreateEntity(uniqueName);
        newEntity.AddComponent<MeshFilterComponent>(PrimitiveType::Cube);
        MeshRendererComponent& meshRenderer = newEntity.AddComponent<MeshRendererComponent>();
        meshRenderer.SetMaterial(0, Renderer3D::GetDefaultMaterial());
    }
    // 创建 Plane
    if (ImGui::MenuItem("Plane"))
    {
        std::string uniqueName = GenerateUniqueName("Plane", parent);
        newEntity = m_Scene->CreateEntity(uniqueName);
        newEntity.AddComponent<MeshFilterComponent>(PrimitiveType::Plane);
        MeshRendererComponent& meshRenderer = newEntity.AddComponent<MeshRendererComponent>();
        meshRenderer.SetMaterial(0, Renderer3D::GetDefaultMaterial());
    }
    // 创建 Sphere
    if (ImGui::MenuItem("Sphere"))
    {
        std::string uniqueName = GenerateUniqueName("Sphere", parent);
        newEntity = m_Scene->CreateEntity(uniqueName);
        newEntity.AddComponent<MeshFilterComponent>(PrimitiveType::Sphere);
        MeshRendererComponent& meshRenderer = newEntity.AddComponent<MeshRendererComponent>();
        meshRenderer.SetMaterial(0, Renderer3D::GetDefaultMaterial());
    }
    // 创建 Cylinder
    if (ImGui::MenuItem("Cylinder"))
    {
        std::string uniqueName = GenerateUniqueName("Cylinder", parent);
        newEntity = m_Scene->CreateEntity(uniqueName);
        newEntity.AddComponent<MeshFilterComponent>(PrimitiveType::Cylinder);
        MeshRendererComponent& meshRenderer = newEntity.AddComponent<MeshRendererComponent>();
        meshRenderer.SetMaterial(0, Renderer3D::GetDefaultMaterial());
    }
    // 创建 Capsule
    if (ImGui::MenuItem("Capsule"))
    {
        std::string uniqueName = GenerateUniqueName("Capsule", parent);
        newEntity = m_Scene->CreateEntity(uniqueName);
        newEntity.AddComponent<MeshFilterComponent>(PrimitiveType::Capsule);
        MeshRendererComponent& meshRenderer = newEntity.AddComponent<MeshRendererComponent>();
        meshRenderer.SetMaterial(0, Renderer3D::GetDefaultMaterial());
    }
    
    ImGui::Separator();
    
    // 创建 Directional Light
    if (ImGui::MenuItem("Directional Light"))
    {
        std::string uniqueName = GenerateUniqueName("Directional Light", parent);
        newEntity = m_Scene->CreateEntity(uniqueName);
        newEntity.AddComponent<DirectionalLightComponent>();
        TransformComponent& transform = newEntity.GetComponent<TransformComponent>();
        transform.SetRotationEuler(glm::vec3(glm::radians(50.0f), glm::radians(-32.0f), 0.0f));
    }
    
    if (newEntity)
    {
        if (parent)
        {
            newEntity.SetParentUUID(parent.GetUUID());
            parent.GetChildren().push_back(newEntity.GetUUID());
        }
        SelectionManager::Deselect();
        SelectionManager::Select(newEntity.GetUUID());
    }
    
    ImGui::EndMenu();
}
```

#### 方案 B：使用辅助函数减少重复代码

提取一个 `CreatePrimitiveEntity` 辅助方法：

```cpp
// 在 SceneHierarchyPanel 类中添加 private 方法
Entity SceneHierarchyPanel::CreatePrimitiveEntity(const std::string& baseName, PrimitiveType type, Entity parent)
{
    std::string uniqueName = GenerateUniqueName(baseName, parent);
    Entity entity = m_Scene->CreateEntity(uniqueName);
    entity.AddComponent<MeshFilterComponent>(type);
    MeshRendererComponent& meshRenderer = entity.AddComponent<MeshRendererComponent>();
    meshRenderer.SetMaterial(0, Renderer3D::GetDefaultMaterial());
    return entity;
}
```

然后菜单代码简化为：

```cpp
if (ImGui::MenuItem("Cube"))
    newEntity = CreatePrimitiveEntity("Cube", PrimitiveType::Cube, parent);
if (ImGui::MenuItem("Plane"))
    newEntity = CreatePrimitiveEntity("Plane", PrimitiveType::Plane, parent);
if (ImGui::MenuItem("Sphere"))
    newEntity = CreatePrimitiveEntity("Sphere", PrimitiveType::Sphere, parent);
if (ImGui::MenuItem("Cylinder"))
    newEntity = CreatePrimitiveEntity("Cylinder", PrimitiveType::Cylinder, parent);
if (ImGui::MenuItem("Capsule"))
    newEntity = CreatePrimitiveEntity("Capsule", PrimitiveType::Capsule, parent);
```

**推荐方案 B**：减少代码重复，新增图元类型时只需添加一行。

---

## 十、完整代码修改清单

### 10.1 MeshFactory.h 完整代码

见 [第五节 5.2](#52-完整的-meshfactoryh)。

### 10.2 MeshFactory.cpp 完整代码

```cpp
#include "lcpch.h"
#include "MeshFactory.h"

#include <glm/gtc/constants.hpp>

namespace Lucky
{
    // ==================== 统一入口 ====================
    
    Ref<Mesh> MeshFactory::CreatePrimitive(PrimitiveType type)
    {
        Ref<Mesh> mesh = nullptr;
        
        switch (type)
        {
            case PrimitiveType::Cube:       mesh = CreateCube();        break;
            case PrimitiveType::Plane:      mesh = CreatePlane();       break;
            case PrimitiveType::Sphere:     mesh = CreateSphere();      break;
            case PrimitiveType::Cylinder:   mesh = CreateCylinder();    break;
            case PrimitiveType::Capsule:    mesh = CreateCapsule();     break;
            default:
                LF_CORE_WARN("MeshFactory::CreatePrimitive: Unknown PrimitiveType {0}", (int)type);
                return nullptr;
        }
        
        if (mesh)
        {
            mesh->SetName(GetPrimitiveTypeName(type));
        }
        
        return mesh;
    }
    
    const char* MeshFactory::GetPrimitiveTypeName(PrimitiveType type)
    {
        switch (type)
        {
            case PrimitiveType::Cube:       return "Cube";
            case PrimitiveType::Plane:      return "Plane";
            case PrimitiveType::Sphere:     return "Sphere";
            case PrimitiveType::Cylinder:   return "Cylinder";
            case PrimitiveType::Capsule:    return "Capsule";
            default:                        return "Unknown";
        }
    }
    
    // ==================== CreateCube ====================
    
    Ref<Mesh> MeshFactory::CreateCube()
    {
        // ... 保持现有实现不变 ...
        // （24 顶点、36 索引的完整 Cube 数据）
    }
    
    // ==================== CreatePlane ====================
    
    // 见第七节 7.2.4 的完整实现代码
    
    // ==================== CreateSphere ====================
    
    // 见第七节 7.3.5 的完整实现代码
    
    // ==================== CreateCylinder ====================
    
    // 见第七节 7.4.3 的完整实现代码
    
    // ==================== CreateCapsule ====================
    
    // 见第七节 7.5.3 的完整实现代码
}
```

> **注意**：需要在文件顶部添加 `#include <glm/gtc/constants.hpp>` 以使用 `glm::pi<float>()`。或者也可以直接定义 `const float PI = 3.14159265358979323846f;`。

### 10.3 MeshFilterComponent.h 修改

见 [第八节 8.2](#82-改进方案)。

### 10.4 SceneHierarchyPanel.cpp 修改

见 [第九节 9.2](#92-改进方案)。

---

## 十一、文件变更总览

| 文件 | 操作 | 改动说明 |
|------|------|---------|
| `Lucky/Source/Lucky/Renderer/MeshFactory.h` | 修改 | 添加 5 个新方法声明 + 1 个 private 方法 |
| `Lucky/Source/Lucky/Renderer/MeshFactory.cpp` | 修改 | 添加 `CreatePrimitive`、`GetPrimitiveTypeName`、`CreatePlane`、`CreateSphere`、`CreateCylinder`、`CreateCapsule` 实现 |
| `Lucky/Source/Lucky/Scene/Components/MeshFilterComponent.h` | 修改 | 简化构造函数，移除 switch 块 |
| `Luck3DApp/Source/Panels/SceneHierarchyPanel.cpp` | 修改 | 添加 Plane/Sphere/Cylinder/Capsule 菜单项 |

---

## 十二、验证方法

### 12.1 基本渲染验证

1. 在编辑器中通过 Create 菜单分别创建 Cube、Plane、Sphere、Cylinder、Capsule
2. 确认每种图元都能正确渲染，无黑面、无穿透
3. 确认法线方向正确（光照效果正常）

### 12.2 UV 映射验证

1. 为每种图元应用一个棋盘格纹理
2. 确认纹理映射无明显拉伸或撕裂
3. 特别检查球体极点处和胶囊体连接处

### 12.3 序列化验证

1. 创建包含所有图元类型的场景
2. 保存场景文件
3. 重新加载场景文件
4. 确认所有图元都能正确重建

### 12.4 边界情况验证

1. `CreatePlane(0)` → 应自动修正为 `subdivisions = 1`
2. `CreateSphere(2, 1)` → 应自动修正为 `segments = 3, rings = 2`
3. `MeshFilterComponent(PrimitiveType::None)` → `Mesh` 应为 `nullptr`，不崩溃

---

## 十三、设计决策记录

| 决策 | 选择 | 原因 |
|------|------|------|
| 球体算法 | UV Sphere | 与 Unity/Blender 一致，UV 映射自然，实现简单 |
| Plane 默认细分 | 1 | 性能优先，无顶点变形需求 |
| Sphere 默认参数 | segments=32, rings=16 | 与 Unity 一致，视觉效果好 |
| Cylinder 默认 segments | 32 | 圆滑度足够，性能可接受 |
| Capsule 默认参数 | segments=32, rings=8 | 半球精度足够，与 Unity 接近 |
| 名称设置位置 | `CreatePrimitive` 统一设置 | 避免分散在各处，单一职责 |
| 编辑器菜单 | 辅助函数 + 逐项菜单 | 减少重复代码，易于扩展 |
