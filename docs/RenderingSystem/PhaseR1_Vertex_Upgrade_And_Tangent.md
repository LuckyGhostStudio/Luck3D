# Phase R1：Vertex 结构升级 + Tangent 空间

> **文档版本**：v1.0  
> **创建日期**：2026-04-07  
> **优先级**：?? P0（最高，PBR 的前置条件）  
> **预计工作量**：2-3 天  
> **前置依赖**：无  
> **文档说明**：本文档详细描述 Vertex 结构体的升级方案，添加 Tangent 属性以支持法线贴图（Normal Mapping），这是后续 PBR 渲染的前置条件。所有代码可直接对照实现。

---

## 目录

- [一、现状分析](#一现状分析)
- [二、改进目标](#二改进目标)
- [三、涉及的文件清单](#三涉及的文件清单)
- [四、Vertex 结构体修改](#四vertex-结构体修改)
  - [4.1 当前结构](#41-当前结构)
  - [4.2 目标结构](#42-目标结构)
  - [4.3 设计决策](#43-设计决策)
- [五、Mesh.h 修改](#五meshh-修改)
  - [5.1 Vertex 结构体](#51-vertex-结构体)
  - [5.2 Mesh 类新增方法](#52-mesh-类新增方法)
- [六、Mesh.cpp 修改](#六meshcpp-修改)
  - [6.1 顶点缓冲区布局更新](#61-顶点缓冲区布局更新)
  - [6.2 RecalculateTangents 实现](#62-recalculatetangents-实现)
- [七、MeshFactory.cpp 修改](#七meshfactorycpp-修改)
  - [7.1 CreateCube Tangent 数据](#71-createcube-tangent-数据)
  - [7.2 CreatePlane Tangent 数据](#72-createplane-tangent-数据)
  - [7.3 CreateSphere Tangent 数据](#73-createsphere-tangent-数据)
  - [7.4 CreateCylinder Tangent 数据](#74-createcylinder-tangent-数据)
  - [7.5 CreateCapsule Tangent 数据](#75-createcapsule-tangent-数据)
- [八、Shader 修改](#八shader-修改)
  - [8.1 Standard.vert 修改](#81-standardvert-修改)
  - [8.2 Standard.frag 修改（最小改动）](#82-standardfrag-修改最小改动)
  - [8.3 InternalError.vert 修改](#83-internalerrorvert-修改)
- [九、Renderer3D.cpp 修改](#九renderer3dcpp-修改)
- [十、MeshTangentCalculator 工具类](#十meshtangentcalculator-工具类)
  - [10.1 头文件](#101-头文件)
  - [10.2 实现文件](#102-实现文件)
- [十一、验证方法](#十一验证方法)
- [十二、设计决策记录](#十二设计决策记录)

---

## 一、现状分析

### 当前 Vertex 结构

```cpp
// Mesh.h - 当前定义
struct Vertex
{
    glm::vec3 Position; // 位置   (12 bytes)
    glm::vec4 Color;    // 颜色   (16 bytes)
    glm::vec3 Normal;   // 法线   (12 bytes)
    glm::vec2 TexCoord; // 纹理坐标 (8 bytes)
};
// 总计 48 bytes/顶点
```

### 当前 Shader 顶点属性布局

```glsl
// Standard.vert
layout(location = 0) in vec3 a_Position;    // 位置
layout(location = 1) in vec4 a_Color;       // 颜色
layout(location = 2) in vec3 a_Normal;      // 法线
layout(location = 3) in vec2 a_TexCoord;    // 纹理坐标
```

### 当前 Mesh.cpp 中的 BufferLayout

```cpp
m_VertexBuffer->SetLayout({
    { ShaderDataType::Float3, "a_Position" },
    { ShaderDataType::Float4, "a_Color" },
    { ShaderDataType::Float3, "a_Normal" },
    { ShaderDataType::Float2, "a_TexCoord" },
});
```

### 问题

| 编号 | 问题 | 影响 |
|------|------|------|
| R1-01 | 缺少 Tangent 属性 | 无法支持法线贴图，PBR 渲染无法实现 |
| R1-02 | 注释中有旧的 Vertex 定义未清理 | 代码混乱 |
| R1-03 | Color 字段对原生网格冗余（全部白色） | 每顶点浪费 16 bytes |

---

## 二、改进目标

1. **添加 `glm::vec4 Tangent` 字段**：xyz = 切线方向，w = handedness（±1）
2. **更新所有 Shader 的顶点属性布局**：添加 `a_Tangent` 输入
3. **更新所有图元创建方法**：填充正确的 Tangent 数据
4. **实现通用 Tangent 计算工具**：为模型导入准备
5. **清理注释中的旧 Vertex 定义**

---

## 三、涉及的文件清单

| 文件路径 | 操作 | 说明 |
|---------|------|------|
| `Lucky/Source/Lucky/Renderer/Mesh.h` | 修改 | Vertex 添加 Tangent，Mesh 添加 RecalculateTangents() |
| `Lucky/Source/Lucky/Renderer/Mesh.cpp` | 修改 | 更新两个构造函数中的 SetLayout |
| `Lucky/Source/Lucky/Renderer/MeshFactory.cpp` | 修改 | 所有图元添加 Tangent 数据 |
| `Lucky/Source/Lucky/Renderer/MeshTangentCalculator.h` | **新建** | Tangent 计算工具类声明 |
| `Lucky/Source/Lucky/Renderer/MeshTangentCalculator.cpp` | **新建** | Tangent 计算实现 |
| `Luck3DApp/Assets/Shaders/Standard.vert` | 修改 | 添加 a_Tangent 输入，计算 TBN 矩阵 |
| `Luck3DApp/Assets/Shaders/Standard.frag` | 修改 | 接收 TBN 矩阵（暂不使用，为 Phase R2 准备） |
| `Luck3DApp/Assets/Shaders/InternalError.vert` | 修改 | 添加 a_Tangent 输入（忽略不使用） |

---

## 四、Vertex 结构体修改

### 4.1 当前结构

```cpp
struct Vertex
{
    glm::vec3 Position; // 12 bytes
    glm::vec4 Color;    // 16 bytes
    glm::vec3 Normal;   // 12 bytes
    glm::vec2 TexCoord; // 8 bytes
};
// 48 bytes/顶点
```

### 4.2 目标结构

```cpp
struct Vertex
{
    glm::vec3 Position;     // 位置       (12 bytes)
    glm::vec4 Color;        // 颜色       (16 bytes)
    glm::vec3 Normal;       // 法线       (12 bytes)
    glm::vec2 TexCoord;     // 纹理坐标   (8 bytes)
    glm::vec4 Tangent;      // 切线+手性  (16 bytes)  xyz=tangent, w=handedness(±1)
};
// 64 bytes/顶点
```

### 4.3 设计决策

| 决策 | 选择 | 原因 |
|------|------|------|
| Tangent 格式 | `vec4`（xyz + w=handedness） | glTF 2.0 标准，Unity/Unreal 一致 |
| Color 字段 | 保留 | 避免破坏性修改，保留顶点着色能力 |
| 字段顺序 | 追加在末尾 | 最小化对现有代码的影响 |
| Bitangent | 不存储，Shader 中计算 | `B = cross(N, T.xyz) * T.w`，节省 12 bytes |

> **为什么 Tangent 追加在末尾而不是调整字段顺序？**
> 
> 如果调整字段顺序（如 Position → Normal → Tangent → TexCoord → Color），会改变所有 location 映射，需要修改更多 Shader。追加在末尾只需要在所有 Shader 中添加一个新的 `layout(location = 4) in vec4 a_Tangent`，现有 location 0-3 不变。

---

## 五、Mesh.h 修改

### 5.1 Vertex 结构体

```cpp
struct Vertex
{
    glm::vec3 Position;     // 位置       (12 bytes)
    glm::vec4 Color;        // 颜色       (16 bytes)
    glm::vec3 Normal;       // 法线       (12 bytes)
    glm::vec2 TexCoord;     // 纹理坐标   (8 bytes)
    glm::vec4 Tangent;      // 切线+手性  (16 bytes)  xyz=tangent方向, w=handedness(±1)
};
// 总计 64 bytes/顶点
```

同时**删除**注释中的旧 Vertex 定义：

```cpp
// 删除以下注释代码块：
// struct Vertex
// {
//     glm::vec3 Position;
//     glm::vec3 Normal;
//     glm::vec3 Tangent;
//     glm::vec3 Binormal;
//     glm::vec2 Texcoord;
// };
```

### 5.2 Mesh 类新增方法

在 `Mesh` 类的 public 区域添加：

```cpp
/// <summary>
/// 重新计算所有顶点的切线空间（Tangent + Handedness）
/// 基于顶点位置、法线和纹理坐标计算
/// 适用于导入的模型或手动创建的网格
/// </summary>
void RecalculateTangents();
```

---

## 六、Mesh.cpp 修改

### 6.1 顶点缓冲区布局更新

两个构造函数中的 `SetLayout` 都需要更新：

```cpp
// 修改前：
m_VertexBuffer->SetLayout({
    { ShaderDataType::Float3, "a_Position" },
    { ShaderDataType::Float4, "a_Color" },
    { ShaderDataType::Float3, "a_Normal" },
    { ShaderDataType::Float2, "a_TexCoord" },
});

// 修改后：
m_VertexBuffer->SetLayout({
    { ShaderDataType::Float3, "a_Position" },   // location 0
    { ShaderDataType::Float4, "a_Color" },      // location 1
    { ShaderDataType::Float3, "a_Normal" },     // location 2
    { ShaderDataType::Float2, "a_TexCoord" },   // location 3
    { ShaderDataType::Float4, "a_Tangent" },    // location 4  ← 新增
});
```

### 6.2 RecalculateTangents 实现

```cpp
#include "MeshTangentCalculator.h"

void Mesh::RecalculateTangents()
{
    if (m_Vertices.empty() || m_VertexIndices.empty())
        return;
    
    MeshTangentCalculator::Calculate(m_Vertices, m_VertexIndices);
    
    // 更新 GPU 缓冲区
    uint32_t dataSize = m_VertexCount * sizeof(Vertex);
    m_VertexBuffer->SetData(m_Vertices.data(), dataSize);
}
```

---

## 七、MeshFactory.cpp 修改

所有图元创建方法需要在顶点数据中添加 `Tangent` 字段。

### 7.1 CreateCube Tangent 数据

Cube 每个面的 Tangent 是固定的，取决于面的朝向和 UV 映射方向。

根据当前 CreateCube 的 UV 布局，每个面的 Tangent 如下：

```cpp
// 右面 (X+)：Normal=(1,0,0), UV 的 U 方向沿 -Z
// Tangent = (0, 0, -1, 1.0)
{ {0.5f, -0.5f,  0.5f}, {1,1,1,1}, { 1,0,0}, {1,0}, {0,0,-1, 1.0f} },
{ {0.5f,  0.5f,  0.5f}, {1,1,1,1}, { 1,0,0}, {1,1}, {0,0,-1, 1.0f} },
{ {0.5f,  0.5f, -0.5f}, {1,1,1,1}, { 1,0,0}, {0,1}, {0,0,-1, 1.0f} },
{ {0.5f, -0.5f, -0.5f}, {1,1,1,1}, { 1,0,0}, {0,0}, {0,0,-1, 1.0f} },

// 左面 (X-)：Normal=(-1,0,0), UV 的 U 方向沿 +Z
// Tangent = (0, 0, 1, 1.0)
{ {-0.5f, -0.5f, -0.5f}, {1,1,1,1}, {-1,0,0}, {1,0}, {0,0,1, 1.0f} },
{ {-0.5f,  0.5f, -0.5f}, {1,1,1,1}, {-1,0,0}, {1,1}, {0,0,1, 1.0f} },
{ {-0.5f,  0.5f,  0.5f}, {1,1,1,1}, {-1,0,0}, {0,1}, {0,0,1, 1.0f} },
{ {-0.5f, -0.5f,  0.5f}, {1,1,1,1}, {-1,0,0}, {0,0}, {0,0,1, 1.0f} },

// 上面 (Y+)：Normal=(0,1,0), UV 的 U 方向沿 +X
// Tangent = (1, 0, 0, 1.0)
{ {-0.5f, 0.5f,  0.5f}, {1,1,1,1}, {0,1,0}, {0,0}, {1,0,0, 1.0f} },
{ { 0.5f, 0.5f,  0.5f}, {1,1,1,1}, {0,1,0}, {1,0}, {1,0,0, 1.0f} },
{ { 0.5f, 0.5f, -0.5f}, {1,1,1,1}, {0,1,0}, {1,1}, {1,0,0, 1.0f} },
{ {-0.5f, 0.5f, -0.5f}, {1,1,1,1}, {0,1,0}, {0,1}, {1,0,0, 1.0f} },

// 下面 (Y-)：Normal=(0,-1,0), UV 的 U 方向沿 +X
// Tangent = (1, 0, 0, 1.0)
{ {-0.5f, -0.5f, -0.5f}, {1,1,1,1}, {0,-1,0}, {0,0}, {1,0,0, 1.0f} },
{ { 0.5f, -0.5f, -0.5f}, {1,1,1,1}, {0,-1,0}, {1,0}, {1,0,0, 1.0f} },
{ { 0.5f, -0.5f,  0.5f}, {1,1,1,1}, {0,-1,0}, {1,1}, {1,0,0, 1.0f} },
{ {-0.5f, -0.5f,  0.5f}, {1,1,1,1}, {0,-1,0}, {0,1}, {1,0,0, 1.0f} },

// 前面 (Z+)：Normal=(0,0,1), UV 的 U 方向沿 +X
// Tangent = (1, 0, 0, 1.0)
{ {-0.5f, -0.5f, 0.5f}, {1,1,1,1}, {0,0,1}, {0,0}, {1,0,0, 1.0f} },
{ { 0.5f, -0.5f, 0.5f}, {1,1,1,1}, {0,0,1}, {1,0}, {1,0,0, 1.0f} },
{ { 0.5f,  0.5f, 0.5f}, {1,1,1,1}, {0,0,1}, {1,1}, {1,0,0, 1.0f} },
{ {-0.5f,  0.5f, 0.5f}, {1,1,1,1}, {0,0,1}, {0,1}, {1,0,0, 1.0f} },

// 后面 (Z-)：Normal=(0,0,-1), UV 的 U 方向沿 -X
// Tangent = (-1, 0, 0, 1.0)
{ { 0.5f, -0.5f, -0.5f}, {1,1,1,1}, {0,0,-1}, {0,0}, {-1,0,0, 1.0f} },
{ {-0.5f, -0.5f, -0.5f}, {1,1,1,1}, {0,0,-1}, {1,0}, {-1,0,0, 1.0f} },
{ {-0.5f,  0.5f, -0.5f}, {1,1,1,1}, {0,0,-1}, {1,1}, {-1,0,0, 1.0f} },
{ { 0.5f,  0.5f, -0.5f}, {1,1,1,1}, {0,0,-1}, {0,1}, {-1,0,0, 1.0f} },
```

> **注意**：上述 Tangent 方向需要根据实际 CreateCube 中的顶点顺序和 UV 坐标验证。实现时建议先用通用 `MeshTangentCalculator::Calculate` 验证，再硬编码解析值。

### 7.2 CreatePlane Tangent 数据

Plane 的所有顶点 Tangent 相同（U 方向沿 X 轴）：

```cpp
vertices.push_back({
    { x, 0.0f, z },                 // Position
    { 1.0f, 1.0f, 1.0f, 1.0f },    // Color
    { 0.0f, 1.0f, 0.0f },          // Normal（朝上）
    { u, v },                       // TexCoord
    { 1.0f, 0.0f, 0.0f, 1.0f }     // Tangent（U 方向沿 X 轴，handedness=1）
});
```

### 7.3 CreateSphere Tangent 数据

球体的 Tangent 沿经线方向（?P/?θ 的归一化）：

```cpp
// 在生成顶点的循环中：
float theta = 2.0f * PI * static_cast<float>(seg) / static_cast<float>(segments);

// Tangent 方向 = ?P/?θ 的归一化
float tangentX = -sin(theta);
float tangentZ = cos(theta);
glm::vec3 tangentDir = glm::normalize(glm::vec3(tangentX, 0.0f, tangentZ));

vertices.push_back({
    { x, y, z },
    { 1.0f, 1.0f, 1.0f, 1.0f },
    normal,
    { u, v },
    { tangentDir.x, tangentDir.y, tangentDir.z, 1.0f }  // Tangent
});
```

> **极点处理**：在北极（ring=0）和南极（ring=rings），`ringRadius = 0`，所有经线段的顶点位置重合。此时 Tangent 方向仍然有效（由 theta 决定），不需要特殊处理。

### 7.4 CreateCylinder Tangent 数据

- **侧面**：与 Sphere 类似，Tangent 沿经线方向

```cpp
glm::vec3 tangentDir = glm::normalize(glm::vec3(-sinTheta, 0.0f, cosTheta));
// 上圈和下圈顶点使用相同的 Tangent
```

- **顶面**：Tangent 沿 X 轴方向

```cpp
{ 1.0f, 0.0f, 0.0f, 1.0f }  // 顶面 Tangent
```

- **底面**：Tangent 沿 X 轴方向

```cpp
{ 1.0f, 0.0f, 0.0f, 1.0f }  // 底面 Tangent
```

### 7.5 CreateCapsule Tangent 数据

与 Sphere + Cylinder 的组合类似：

- **上半球/下半球**：与 Sphere 相同的 Tangent 计算
- **圆柱中段连接带**：与 Cylinder 侧面相同

```cpp
// 对于所有部分，Tangent 方向都是：
float tangentX = -sin(theta);
float tangentZ = cos(theta);
glm::vec3 tangentDir = glm::normalize(glm::vec3(tangentX, 0.0f, tangentZ));
```

---

## 八、Shader 修改

### 8.1 Standard.vert 修改

```glsl
#version 450 core

layout(location = 0) in vec3 a_Position;    // 位置
layout(location = 1) in vec4 a_Color;       // 颜色
layout(location = 2) in vec3 a_Normal;      // 法线
layout(location = 3) in vec2 a_TexCoord;    // 纹理坐标
layout(location = 4) in vec4 a_Tangent;     // 切线 + 手性  ← 新增

// 相机 Uniform 缓冲区
layout(std140, binding = 0) uniform Camera
{
    mat4 ViewProjectionMatrix;
    vec3 Position;
} u_Camera;

// 光照 Uniform 缓冲区
layout(std140, binding = 1) uniform Light
{
    float Intensity;
    vec3 Direction;
    vec3 Color;
} u_Light;

// 模型矩阵
uniform mat4 u_ObjectToWorldMatrix;

// 顶点着色器输出数据
struct VertexOutput
{
    vec4 Color;
    vec3 Normal;
    vec2 TexCoord;
    vec3 WorldPos;
    mat3 TBN;       // ← 新增：切线空间矩阵
};

layout(location = 0) out VertexOutput v_Output;

void main()
{
    v_Output.Color = a_Color;
    v_Output.TexCoord = a_TexCoord;

    // 法线矩阵
    mat3 normalMatrix = mat3(transpose(inverse(u_ObjectToWorldMatrix)));

    // 变换法向量到世界空间
    vec3 N = normalize(normalMatrix * a_Normal);
    v_Output.Normal = N;

    // 计算 TBN 矩阵  ← 新增
    vec3 T = normalize(normalMatrix * a_Tangent.xyz);
    // Gram-Schmidt 正交化：确保 T ⊥ N
    T = normalize(T - dot(T, N) * N);
    // Bitangent = cross(N, T) * handedness
    vec3 B = cross(N, T) * a_Tangent.w;
    v_Output.TBN = mat3(T, B, N);

    // 计算世界空间位置
    vec4 worldPos = u_ObjectToWorldMatrix * vec4(a_Position, 1.0);
    v_Output.WorldPos = worldPos.xyz;

    gl_Position = u_Camera.ViewProjectionMatrix * worldPos;
}
```

### 8.2 Standard.frag 修改（最小改动）

本 Phase 中 Fragment Shader 只需要接收 TBN 矩阵，但**暂不使用**（法线贴图在 Phase R2 中启用）：

```glsl
#version 450 core

layout(location = 0) out vec4 o_Color;

// ... UBO 定义不变 ...

// 顶点着色器输出数据
struct VertexOutput
{
    vec4 Color;
    vec3 Normal;
    vec2 TexCoord;
    vec3 WorldPos;
    mat3 TBN;       // ← 新增（本 Phase 暂不使用）
};

layout(location = 0) in VertexOutput v_Input;

// ... 材质 Uniform 不变 ...

void main()
{
    // 归一化法向量（仍然使用插值法线，不使用 TBN）
    vec3 N = normalize(v_Input.Normal);

    // ... 其余光照计算不变 ...
}
```

### 8.3 InternalError.vert 修改

只需添加 `a_Tangent` 输入声明，不使用：

```glsl
layout(location = 0) in vec3 a_Position;
layout(location = 1) in vec4 a_Color;
layout(location = 2) in vec3 a_Normal;
layout(location = 3) in vec2 a_TexCoord;
layout(location = 4) in vec4 a_Tangent;     // ← 新增（不使用）

// ... 其余不变 ...
```

---

## 九、Renderer3D.cpp 修改

`DrawMesh` 中的顶点数据复制逻辑**无需修改**，因为：

1. `MeshVertexBufferData` 的类型是 `std::vector<Vertex>`
2. `sizeof(Vertex)` 会自动从 48 变为 64
3. `SetVertexBufferData` 使用 `sizeof(Vertex) * count` 计算大小

唯一需要确认的是 `MeshVertexBufferData` 的初始容量是否足够，但由于使用 `push_back`，会自动扩容。

---

## 十、MeshTangentCalculator 工具类

### 10.1 头文件

```cpp
// Lucky/Source/Lucky/Renderer/MeshTangentCalculator.h
#pragma once

#include "Mesh.h"

namespace Lucky
{
    /// <summary>
    /// 网格切线空间计算工具
    /// 为网格的每个顶点计算 Tangent 向量和 Handedness
    /// </summary>
    class MeshTangentCalculator
    {
    public:
        /// <summary>
        /// 为网格计算切线空间（Tangent + Handedness）
        /// 基于 Lengyel 的切线空间计算算法
        /// 
        /// 算法步骤：
        /// 1. 遍历每个三角形，计算面切线和面副切线
        /// 2. 将面切线/副切线累积到三角形的三个顶点（面积加权）
        /// 3. 对每个顶点进行 Gram-Schmidt 正交化
        /// 4. 计算 handedness（±1）
        /// </summary>
        /// <param name="vertices">顶点数组（会被修改，填充 Tangent 字段）</param>
        /// <param name="indices">索引数组</param>
        static void Calculate(std::vector<Vertex>& vertices, const std::vector<uint32_t>& indices);
    };
}
```

### 10.2 实现文件

```cpp
// Lucky/Source/Lucky/Renderer/MeshTangentCalculator.cpp
#include "lcpch.h"
#include "MeshTangentCalculator.h"

namespace Lucky
{
    void MeshTangentCalculator::Calculate(std::vector<Vertex>& vertices, const std::vector<uint32_t>& indices)
    {
        if (vertices.empty() || indices.empty())
            return;
        
        // 临时存储每个顶点的累积 Tangent 和 Bitangent
        std::vector<glm::vec3> tangents(vertices.size(), glm::vec3(0.0f));
        std::vector<glm::vec3> bitangents(vertices.size(), glm::vec3(0.0f));
        
        // 步骤 1：遍历每个三角形，计算面切线和面副切线
        for (size_t i = 0; i + 2 < indices.size(); i += 3)
        {
            uint32_t i0 = indices[i];
            uint32_t i1 = indices[i + 1];
            uint32_t i2 = indices[i + 2];
            
            // 边界检查
            if (i0 >= vertices.size() || i1 >= vertices.size() || i2 >= vertices.size())
                continue;
            
            const glm::vec3& p0 = vertices[i0].Position;
            const glm::vec3& p1 = vertices[i1].Position;
            const glm::vec3& p2 = vertices[i2].Position;
            
            const glm::vec2& uv0 = vertices[i0].TexCoord;
            const glm::vec2& uv1 = vertices[i1].TexCoord;
            const glm::vec2& uv2 = vertices[i2].TexCoord;
            
            glm::vec3 edge1 = p1 - p0;
            glm::vec3 edge2 = p2 - p0;
            
            float du1 = uv1.x - uv0.x;
            float dv1 = uv1.y - uv0.y;
            float du2 = uv2.x - uv0.x;
            float dv2 = uv2.y - uv0.y;
            
            float det = du1 * dv2 - du2 * dv1;
            
            // 避免除以零（退化三角形或退化 UV）
            if (std::abs(det) < 1e-8f)
                continue;
            
            float f = 1.0f / det;
            
            glm::vec3 tangent = f * (dv2 * edge1 - dv1 * edge2);
            glm::vec3 bitangent = f * (-du2 * edge1 + du1 * edge2);
            
            // 步骤 2：累积到三个顶点（面积加权，tangent 长度与三角形面积成正比）
            tangents[i0] += tangent;
            tangents[i1] += tangent;
            tangents[i2] += tangent;
            
            bitangents[i0] += bitangent;
            bitangents[i1] += bitangent;
            bitangents[i2] += bitangent;
        }
        
        // 步骤 3 & 4：正交化并计算 handedness
        for (size_t i = 0; i < vertices.size(); ++i)
        {
            const glm::vec3& n = vertices[i].Normal;
            const glm::vec3& t = tangents[i];
            const glm::vec3& b = bitangents[i];
            
            // Gram-Schmidt 正交化：T' = normalize(T - N * dot(N, T))
            glm::vec3 tangentOrtho = t - n * glm::dot(n, t);
            
            float len = glm::length(tangentOrtho);
            if (len < 1e-8f)
            {
                // 退化情况：生成一个任意的切线（与法线垂直）
                tangentOrtho = (std::abs(n.x) < 0.9f) 
                    ? glm::normalize(glm::cross(n, glm::vec3(1.0f, 0.0f, 0.0f)))
                    : glm::normalize(glm::cross(n, glm::vec3(0.0f, 1.0f, 0.0f)));
                vertices[i].Tangent = glm::vec4(tangentOrtho, 1.0f);
                continue;
            }
            
            tangentOrtho = glm::normalize(tangentOrtho);
            
            // Handedness：判断 Bitangent 方向是否与 cross(N, T) 同向
            float handedness = (glm::dot(glm::cross(n, tangentOrtho), b) < 0.0f) ? -1.0f : 1.0f;
            
            vertices[i].Tangent = glm::vec4(tangentOrtho, handedness);
        }
    }
}
```

---

## 十一、验证方法

### 11.1 基本渲染验证

1. 修改 Vertex 结构后，编译通过
2. 运行引擎，确认所有现有图元（Cube 等）仍然正确渲染
3. 确认光照效果不变（Tangent 字段不影响现有 Blinn-Phong 光照）

### 11.2 Tangent 方向可视化

在 Fragment Shader 中临时输出 Tangent 方向作为颜色：

```glsl
// 临时调试代码
o_Color = vec4(v_Input.TBN[0] * 0.5 + 0.5, 1.0);  // Tangent 可视化（红绿蓝）
```

验证：
- Cube 的每个面应显示不同颜色（Tangent 方向不同）
- Sphere 的 Tangent 方向应沿经线方向平滑变化
- Plane 应显示均匀的红色（Tangent 沿 X 轴 = (1,0,0)）

### 11.3 通用计算验证

1. 创建一个 Cube，手动将所有 Tangent 清零
2. 调用 `RecalculateTangents()`
3. 对比计算结果与解析值，应非常接近

---

## 十二、设计决策记录

| 决策 | 选择 | 原因 |
|------|------|------|
| Tangent 存储方式 | vec4（xyz + w=handedness） | glTF 2.0 标准，Unity/Unreal 一致 |
| Color 字段 | 保留 | 避免破坏性修改，保留顶点着色能力 |
| Tangent 字段位置 | 追加在 Vertex 末尾 | 最小化 Shader location 变更 |
| 原生图元 Tangent | 解析计算（硬编码） | 精确，无数值误差 |
| 导入模型 Tangent | 通用计算（MeshTangentCalculator） | 通用性好 |
| Bitangent | 不存储，Shader 中计算 | 节省 12 bytes/顶点 |
| Tangent 计算工具 | 独立类 MeshTangentCalculator | 单一职责，可被 Mesh 和模型导入器复用 |
