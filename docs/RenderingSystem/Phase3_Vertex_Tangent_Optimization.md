# Phase 3：Vertex 结构优化 + Tangent 空间计算

> **文档版本**：v1.0  
> **创建日期**：2026-04-07  
> **最后更新**：2026-04-07  
> **前置依赖**：Phase 1（补全基础图元网格）、Phase 2（工厂优化）  
> **文档说明**：本文档描述 Vertex 结构体的优化方案，包括添加 Tangent/Bitangent 属性以支持法线贴图（Normal Mapping），以及对现有 Color 字段的处理策略。由于本 Phase 涉及面较广（Vertex 结构变更会影响所有网格创建、渲染、Shader），属于**大范围重构**，需要谨慎评估。所有代码可直接对照实现。

---

## 目录

- [一、现状分析](#一现状分析)
  - [1.1 当前 Vertex 结构](#11-当前-vertex-结构)
  - [1.2 当前 Shader 顶点属性布局](#12-当前-shader-顶点属性布局)
  - [1.3 待优化问题](#13-待优化问题)
- [二、改进目标](#二改进目标)
- [三、涉及的文件](#三涉及的文件)
- [四、Vertex 结构优化](#四vertex-结构优化)
  - [4.1 设计决策：Color 字段的处理](#41-设计决策color-字段的处理)
  - [4.2 设计决策：Tangent 存储方式](#42-设计决策tangent-存储方式)
  - [4.3 方案 A：添加 Tangent + Bitangent（6 floats）](#43-方案-a添加-tangent--bitangent6-floats)
  - [4.4 方案 B：添加 Tangent + Handedness（4 floats）（推荐）](#44-方案-b添加-tangent--handedness4-floats推荐)
  - [4.5 方案 C：仅添加 Tangent（3 floats）](#45-方案-c仅添加-tangent3-floats)
  - [4.6 方案对比与推荐](#46-方案对比与推荐)
- [五、Color 字段处理策略](#五color-字段处理策略)
  - [5.1 方案 A：保留 Color 字段（推荐当前阶段）](#51-方案-a保留-color-字段推荐当前阶段)
  - [5.2 方案 B：移除 Color 字段](#52-方案-b移除-color-字段)
  - [5.3 方案 C：Color 改为可选（多种 Vertex 布局）](#53-方案-c-color-改为可选多种-vertex-布局)
  - [5.4 方案对比与推荐](#54-方案对比与推荐)
- [六、Tangent 空间计算算法](#六tangent-空间计算算法)
  - [6.1 算法原理](#61-算法原理)
  - [6.2 通用 Tangent 计算函数](#62-通用-tangent-计算函数)
  - [6.3 原生图元的 Tangent 解析计算](#63-原生图元的-tangent-解析计算)
  - [6.4 方案选择：解析计算 vs 通用计算](#64-方案选择解析计算-vs-通用计算)
- [七、Shader 修改](#七shader-修改)
  - [7.1 顶点属性布局更新](#71-顶点属性布局更新)
  - [7.2 Vertex Shader 修改](#72-vertex-shader-修改)
  - [7.3 Fragment Shader 法线贴图采样](#73-fragment-shader-法线贴图采样)
- [八、完整代码实现](#八完整代码实现)
  - [8.1 Mesh.h 修改](#81-meshh-修改)
  - [8.2 Mesh.cpp 修改](#82-meshcpp-修改)
  - [8.3 MeshFactory.cpp 修改](#83-meshfactorycpp-修改)
  - [8.4 Renderer3D.cpp 修改](#84-renderer3dcpp-修改)
  - [8.5 MeshTangentCalculator 工具类](#85-meshtangentcalculator-工具类)
- [九、对模型导入的影响](#九对模型导入的影响)
- [十、文件变更总览](#十文件变更总览)
- [十一、验证方法](#十一验证方法)
- [十二、设计决策记录](#十二设计决策记录)
- [十三、风险评估与回退方案](#十三风险评估与回退方案)

---

## 一、现状分析

### 1.1 当前 Vertex 结构

```cpp
struct Vertex
{
    glm::vec3 Position; // 位置   (12 bytes)
    glm::vec4 Color;    // 颜色   (16 bytes)
    glm::vec3 Normal;   // 法线   (12 bytes)
    glm::vec2 TexCoord; // 纹理坐标 (8 bytes)
};
// 总计 48 bytes/顶点
```

### 1.2 当前 Shader 顶点属性布局

```cpp
m_VertexBuffer->SetLayout({
    { ShaderDataType::Float3, "a_Position" },   // 位置
    { ShaderDataType::Float4, "a_Color" },      // 颜色
    { ShaderDataType::Float3, "a_Normal" },     // 法线
    { ShaderDataType::Float2, "a_TexCoord" },   // 纹理坐标
});
```

### 1.3 待优化问题

| 编号 | 问题 | 影响 | 优先级 |
|------|------|------|--------|
| **V-01** | 缺少 Tangent/Bitangent 属性 | 无法支持法线贴图（Normal Mapping） | ?? 中 |
| **V-02** | Color 字段对原生网格冗余 | 每顶点浪费 16 bytes，原生网格全部硬编码白色 | ?? 低 |
| **V-03** | 注释中有一个替代 Vertex 定义被注释掉 | 代码中存在未清理的设计痕迹 | ?? 低 |

---

## 二、改进目标

1. **添加 Tangent 属性**：支持法线贴图所需的切线空间
2. **实现 Tangent 计算**：为原生图元提供解析计算，为导入模型提供通用计算
3. **评估 Color 字段**：决定保留、移除或改为可选
4. **更新 Shader**：顶点属性布局和法线贴图采样
5. **保持向后兼容**：确保现有场景和模型不受影响

---

## 三、涉及的文件

### 需要修改的文件

| 文件路径 | 说明 |
|---------|------|
| `Lucky/Source/Lucky/Renderer/Mesh.h` | 修改 `Vertex` 结构体 |
| `Lucky/Source/Lucky/Renderer/Mesh.cpp` | 更新顶点缓冲区布局 |
| `Lucky/Source/Lucky/Renderer/MeshFactory.cpp` | 所有图元创建方法添加 Tangent 数据 |
| `Lucky/Source/Lucky/Renderer/Renderer3D.cpp` | 更新 `MeshVertexBufferData` 相关逻辑 |
| `Luck3DApp/Assets/Shaders/Standard.glsl` | 更新顶点属性和法线贴图逻辑 |

### 可能需要新建的文件

| 文件路径 | 说明 |
|---------|------|
| `Lucky/Source/Lucky/Renderer/MeshTangentCalculator.h` | Tangent 计算工具类（可选） |
| `Lucky/Source/Lucky/Renderer/MeshTangentCalculator.cpp` | Tangent 计算实现（可选） |

---

## 四、Vertex 结构优化

### 4.1 设计决策：Color 字段的处理

详见 [第五节](#五color-字段处理策略)。

### 4.2 设计决策：Tangent 存储方式

法线贴图需要 TBN 矩阵（Tangent-Bitangent-Normal），其中 Normal 已有，需要添加 Tangent 和 Bitangent。

### 4.3 方案 A：添加 Tangent + Bitangent（6 floats）

```cpp
struct Vertex
{
    glm::vec3 Position;     // 12 bytes
    glm::vec4 Color;        // 16 bytes
    glm::vec3 Normal;       // 12 bytes
    glm::vec2 TexCoord;     // 8 bytes
    glm::vec3 Tangent;      // 12 bytes  ← 新增
    glm::vec3 Bitangent;    // 12 bytes  ← 新增
};
// 总计 72 bytes/顶点
```

**优点**：
- 最直观，Shader 中直接使用
- 不需要在 Shader 中计算 Bitangent

**缺点**：
- 每顶点增加 24 bytes（从 48 → 72，增加 50%）
- Bitangent 可以从 Normal × Tangent 计算得出，存储冗余

### 4.4 方案 B：添加 Tangent + Handedness（4 floats）（推荐）

```cpp
struct Vertex
{
    glm::vec3 Position;     // 12 bytes
    glm::vec4 Color;        // 16 bytes
    glm::vec3 Normal;       // 12 bytes
    glm::vec2 TexCoord;     // 8 bytes
    glm::vec4 Tangent;      // 16 bytes  ← 新增（xyz = tangent, w = handedness ±1）
};
// 总计 64 bytes/顶点
```

Bitangent 在 Shader 中计算：
```glsl
vec3 bitangent = cross(normal, tangent.xyz) * tangent.w;
```

**优点**：
- 业界标准做法（glTF 2.0 规范、Unity、Unreal 都使用此方式）
- 每顶点只增加 16 bytes（从 48 → 64，增加 33%）
- Handedness（w 分量）正确处理镜像 UV 的情况

**缺点**：
- Shader 中需要一次叉积计算（开销极小）

### 4.5 方案 C：仅添加 Tangent（3 floats）

```cpp
struct Vertex
{
    glm::vec3 Position;     // 12 bytes
    glm::vec4 Color;        // 16 bytes
    glm::vec3 Normal;       // 12 bytes
    glm::vec2 TexCoord;     // 8 bytes
    glm::vec3 Tangent;      // 12 bytes  ← 新增
};
// 总计 60 bytes/顶点
```

Bitangent 在 Shader 中计算，假设 handedness 始终为 +1：
```glsl
vec3 bitangent = cross(normal, tangent);
```

**优点**：
- 最节省空间
- 实现最简单

**缺点**：
- 无法处理镜像 UV（handedness = -1 的情况）
- 导入模型时可能出现法线贴图翻转

### 4.6 方案对比与推荐

| 维度 | 方案 A（T+B） | 方案 B（T4） | 方案 C（T3） |
|------|--------------|-------------|-------------|
| 每顶点大小 | 72 bytes | 64 bytes | 60 bytes |
| 相比当前增加 | +50% | +33% | +25% |
| 镜像 UV 支持 | ? | ? | ? |
| 业界标准 | 部分引擎 | glTF/Unity/Unreal | 无 |
| Shader 复杂度 | 最低 | 低（一次叉积） | 低（一次叉积） |
| 存储冗余 | 有（Bitangent 可计算） | 无 | 无 |

**推荐方案 B（Tangent + Handedness）**：

理由：
1. **业界标准**：glTF 2.0 规范明确定义 Tangent 为 `vec4`（xyz = tangent, w = handedness）
2. **空间效率**：比方案 A 节省 8 bytes/顶点
3. **正确性**：handedness 确保镜像 UV 的法线贴图正确
4. **未来兼容**：导入 glTF 模型时可以直接使用其 Tangent 数据

---

## 五、Color 字段处理策略

### 5.1 方案 A：保留 Color 字段（推荐当前阶段）

保持 `glm::vec4 Color` 不变，所有原生图元继续使用白色 `(1,1,1,1)`。

**优点**：
- 零改动风险
- 未来可用于顶点着色（Vertex Color）功能
- 某些调试场景下有用（如按面着色）

**缺点**：
- 每顶点 16 bytes 的"浪费"

**适用场景**：
- 当前阶段，优先保证稳定性
- 未来如果引入顶点着色功能，Color 字段直接可用

### 5.2 方案 B：移除 Color 字段

从 `Vertex` 中移除 `Color`，颜色完全由材质系统控制。

```cpp
struct Vertex
{
    glm::vec3 Position;     // 12 bytes
    glm::vec3 Normal;       // 12 bytes
    glm::vec2 TexCoord;     // 8 bytes
    glm::vec4 Tangent;      // 16 bytes
};
// 总计 48 bytes/顶点（与当前相同！）
```

**优点**：
- 添加 Tangent 后总大小不变（48 bytes）
- 更符合现代渲染管线（颜色由材质决定）

**缺点**：
- **破坏性修改**：所有 Shader 的 `a_Color` 属性需要移除
- 所有 `MeshFactory` 中的顶点数据需要修改
- 模型导入器（如果有）需要适配
- 失去顶点着色能力

**影响范围**：

| 文件 | 需要修改的内容 |
|------|--------------|
| `Mesh.h` | 移除 `Vertex::Color` |
| `Mesh.cpp` | 移除 `a_Color` 布局 |
| `MeshFactory.cpp` | 所有顶点数据移除 Color 分量 |
| `Renderer3D.cpp` | `MeshVertexBufferData` 相关逻辑 |
| `Standard.glsl` | 移除 `a_Color` 输入 |
| `InternalError.glsl` | 移除 `a_Color` 输入 |
| 模型导入器（未来） | 不再导入顶点颜色 |

### 5.3 方案 C：Color 改为可选（多种 Vertex 布局）

定义多种 Vertex 类型，根据需要选择：

```cpp
struct Vertex3D       // 标准 3D 顶点（无 Color）
{
    glm::vec3 Position;
    glm::vec3 Normal;
    glm::vec2 TexCoord;
    glm::vec4 Tangent;
};

struct Vertex3DColor  // 带顶点颜色的 3D 顶点
{
    glm::vec3 Position;
    glm::vec4 Color;
    glm::vec3 Normal;
    glm::vec2 TexCoord;
    glm::vec4 Tangent;
};
```

**优点**：
- 最灵活，按需选择
- 不浪费空间

**缺点**：
- **极大增加复杂度**：Mesh 类需要支持多种顶点格式
- 渲染器需要根据顶点格式选择不同的 Shader 变体
- 模板化或运行时多态，代码复杂度翻倍
- 远超当前引擎的架构能力

### 5.4 方案对比与推荐

| 维度 | 方案 A（保留） | 方案 B（移除） | 方案 C（可选） |
|------|--------------|--------------|--------------|
| 改动范围 | 无 | 大 | 极大 |
| 风险 | 无 | 中 | 高 |
| 空间效率 | 低 | 高 | 最高 |
| 实现复杂度 | 无 | 中 | 极高 |
| 未来灵活性 | 中 | 低（失去顶点颜色） | 最高 |

**推荐方案 A（保留 Color）**（当前阶段）：

理由：
1. 添加 Tangent 是本 Phase 的核心目标，不应同时进行 Color 移除这种破坏性修改
2. 保留 Color 为未来顶点着色功能预留空间
3. 16 bytes/顶点的"浪费"在当前场景规模下可以接受
4. 如果未来确定不需要顶点颜色，可以在单独的重构中移除

**如果选择方案 B（移除 Color）**，建议：
- 与 Tangent 添加同时进行，这样 Vertex 大小保持 48 bytes 不变
- 需要同步修改所有 Shader
- 需要确认模型导入器不依赖顶点颜色

---

## 六、Tangent 空间计算算法

### 6.1 算法原理

Tangent 空间（TBN 矩阵）用于将法线贴图中的切线空间法线转换到世界空间。

对于一个三角形，其三个顶点为 P0, P1, P2，对应的 UV 为 (u0,v0), (u1,v1), (u2,v2)：

```
Edge1 = P1 - P0
Edge2 = P2 - P0

ΔU1 = u1 - u0,  ΔV1 = v1 - v0
ΔU2 = u2 - u0,  ΔV2 = v2 - v0

f = 1.0 / (ΔU1 × ΔV2 - ΔU2 × ΔV1)

Tangent   = f × (ΔV2 × Edge1 - ΔV1 × Edge2)
Bitangent = f × (-ΔU2 × Edge1 + ΔU1 × Edge2)

Handedness = sign(dot(cross(Normal, Tangent), Bitangent))
```

### 6.2 通用 Tangent 计算函数

适用于任意网格（导入的模型、自定义网格等）：

```cpp
// MeshTangentCalculator.h

#pragma once

#include "Mesh.h"

namespace Lucky
{
    /// <summary>
    /// 网格切线空间计算工具
    /// </summary>
    class MeshTangentCalculator
    {
    public:
        /// <summary>
        /// 为网格计算切线空间（Tangent + Handedness）
        /// 基于 MikkTSpace 算法的简化版本
        /// </summary>
        /// <param name="vertices">顶点数组（会被修改，填充 Tangent 字段）</param>
        /// <param name="indices">索引数组</param>
        static void Calculate(std::vector<Vertex>& vertices, const std::vector<uint32_t>& indices);
    };
}
```

```cpp
// MeshTangentCalculator.cpp

#include "lcpch.h"
#include "MeshTangentCalculator.h"

namespace Lucky
{
    void MeshTangentCalculator::Calculate(std::vector<Vertex>& vertices, const std::vector<uint32_t>& indices)
    {
        // 临时存储每个顶点的累积 Tangent 和 Bitangent
        std::vector<glm::vec3> tangents(vertices.size(), glm::vec3(0.0f));
        std::vector<glm::vec3> bitangents(vertices.size(), glm::vec3(0.0f));
        
        // 遍历每个三角形
        for (size_t i = 0; i < indices.size(); i += 3)
        {
            uint32_t i0 = indices[i];
            uint32_t i1 = indices[i + 1];
            uint32_t i2 = indices[i + 2];
            
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
            
            float f = du1 * dv2 - du2 * dv1;
            
            // 避免除以零（退化三角形或退化 UV）
            if (std::abs(f) < 1e-8f)
                continue;
            
            f = 1.0f / f;
            
            glm::vec3 tangent = f * (dv2 * edge1 - dv1 * edge2);
            glm::vec3 bitangent = f * (-du2 * edge1 + du1 * edge2);
            
            // 累积到三个顶点（面积加权，因为 tangent 长度与三角形面积成正比）
            tangents[i0] += tangent;
            tangents[i1] += tangent;
            tangents[i2] += tangent;
            
            bitangents[i0] += bitangent;
            bitangents[i1] += bitangent;
            bitangents[i2] += bitangent;
        }
        
        // 正交化并计算 handedness
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
                // 退化情况：生成一个任意的切线
                tangentOrtho = (std::abs(n.x) < 0.9f) 
                    ? glm::normalize(glm::cross(n, glm::vec3(1, 0, 0)))
                    : glm::normalize(glm::cross(n, glm::vec3(0, 1, 0)));
                vertices[i].Tangent = glm::vec4(tangentOrtho, 1.0f);
                continue;
            }
            
            tangentOrtho = glm::normalize(tangentOrtho);
            
            // Handedness：判断 Bitangent 方向
            float handedness = (glm::dot(glm::cross(n, tangentOrtho), b) < 0.0f) ? -1.0f : 1.0f;
            
            vertices[i].Tangent = glm::vec4(tangentOrtho, handedness);
        }
    }
}
```

### 6.3 原生图元的 Tangent 解析计算

对于原生图元，Tangent 可以直接从几何关系解析计算，无需通用算法：

#### 6.3.1 Cube

每个面的 Tangent 是固定的，取决于面的朝向和 UV 映射方向：

| 面 | Normal | Tangent (xyz) | Handedness (w) |
|----|--------|---------------|----------------|
| 右面 (X+) | (1, 0, 0) | (0, 0, -1) | 1.0 |
| 左面 (X-) | (-1, 0, 0) | (0, 0, 1) | 1.0 |
| 上面 (Y+) | (0, 1, 0) | (1, 0, 0) | 1.0 |
| 下面 (Y-) | (0, -1, 0) | (1, 0, 0) | 1.0 |
| 前面 (Z+) | (0, 0, 1) | (1, 0, 0) | 1.0 |
| 后面 (Z-) | (0, 0, -1) | (-1, 0, 0) | 1.0 |

> **注意**：具体的 Tangent 方向取决于 UV 映射的 U 方向。上表基于当前 `CreateCube` 的 UV 布局。实现时需要根据实际 UV 坐标验证。

#### 6.3.2 Plane

```
Normal  = (0, 1, 0)
Tangent = (1, 0, 0, 1.0)  // U 方向沿 X 轴
```

所有顶点的 Tangent 相同。

#### 6.3.3 Sphere

```
对于球面上的点 (x, y, z)，其球面坐标为 (θ, φ)：
  θ = atan2(z, x)     // 经度
  φ = acos(y/radius)  // 纬度

Tangent 方向 = ?P/?θ 的归一化 = normalize(-sin(θ), 0, cos(θ))
Handedness = 1.0（标准 UV 映射下）
```

```cpp
// 在 CreateSphere 中，对于每个顶点：
float tangentX = -sin(theta);
float tangentZ = cos(theta);
glm::vec3 tangentDir = glm::normalize(glm::vec3(tangentX, 0.0f, tangentZ));
glm::vec4 tangent = glm::vec4(tangentDir, 1.0f);
```

> **极点处理**：在北极和南极，Tangent 方向退化（ringRadius = 0）。可以使用任意水平方向作为 Tangent，或使用通用计算算法处理。

#### 6.3.4 Cylinder

- **侧面**：与 Sphere 类似，Tangent 沿经线方向
  ```
  Tangent = normalize(-sin(θ), 0, cos(θ))
  ```
- **顶面/底面**：与 Plane 类似
  ```
  顶面 Tangent = (1, 0, 0, 1.0)  // 需要根据 UV 映射验证
  底面 Tangent = (1, 0, 0, 1.0)
  ```

#### 6.3.5 Capsule

与 Sphere + Cylinder 的组合类似，各部分分别计算。

### 6.4 方案选择：解析计算 vs 通用计算

| 方案 | 适用场景 | 优点 | 缺点 |
|------|---------|------|------|
| **解析计算** | 原生图元 | 精确，无数值误差 | 每种图元需要单独推导 |
| **通用计算** | 导入模型、自定义网格 | 通用，一次实现处处可用 | 有数值误差，退化三角形需要特殊处理 |

**推荐**：两者都实现。

1. **原生图元**使用解析计算（在 `MeshFactory` 的各 `Create` 方法中直接填充 Tangent）
2. **导入模型**使用通用计算（`MeshTangentCalculator::Calculate`）
3. 提供 `Mesh::RecalculateTangents()` 方法，内部调用通用计算

---

## 七、Shader 修改

### 7.1 顶点属性布局更新

```cpp
// Mesh.cpp 中的布局更新
m_VertexBuffer->SetLayout({
    { ShaderDataType::Float3, "a_Position" },   // 位置
    { ShaderDataType::Float4, "a_Color" },      // 颜色
    { ShaderDataType::Float3, "a_Normal" },     // 法线
    { ShaderDataType::Float2, "a_TexCoord" },   // 纹理坐标
    { ShaderDataType::Float4, "a_Tangent" },    // 切线 + 手性  ← 新增
});
```

### 7.2 Vertex Shader 修改

```glsl
// Standard.glsl - Vertex Shader

layout(location = 0) in vec3 a_Position;
layout(location = 1) in vec4 a_Color;
layout(location = 2) in vec3 a_Normal;
layout(location = 3) in vec2 a_TexCoord;
layout(location = 4) in vec4 a_Tangent;     // 新增

out VS_OUT {
    vec3 FragPos;
    vec3 Normal;
    vec2 TexCoord;
    vec4 Color;
    mat3 TBN;       // 新增：切线空间矩阵
} vs_out;

uniform mat4 u_ObjectToWorldMatrix;

void main()
{
    vec4 worldPos = u_ObjectToWorldMatrix * vec4(a_Position, 1.0);
    vs_out.FragPos = worldPos.xyz;
    
    mat3 normalMatrix = transpose(inverse(mat3(u_ObjectToWorldMatrix)));
    vs_out.Normal = normalize(normalMatrix * a_Normal);
    
    vs_out.TexCoord = a_TexCoord;
    vs_out.Color = a_Color;
    
    // 计算 TBN 矩阵
    vec3 T = normalize(normalMatrix * a_Tangent.xyz);
    vec3 N = vs_out.Normal;
    // 重新正交化（确保 T ⊥ N）
    T = normalize(T - dot(T, N) * N);
    vec3 B = cross(N, T) * a_Tangent.w;  // handedness
    vs_out.TBN = mat3(T, B, N);
    
    gl_Position = /* ViewProjection */ * worldPos;
}
```

### 7.3 Fragment Shader 法线贴图采样

```glsl
// Standard.glsl - Fragment Shader

in VS_OUT {
    vec3 FragPos;
    vec3 Normal;
    vec2 TexCoord;
    vec4 Color;
    mat3 TBN;
} fs_in;

uniform sampler2D u_NormalMap;      // 法线贴图
uniform int u_UseNormalMap;         // 是否使用法线贴图（0 或 1）

void main()
{
    vec3 normal;
    
    if (u_UseNormalMap == 1)
    {
        // 从法线贴图采样并转换到世界空间
        vec3 normalMapSample = texture(u_NormalMap, fs_in.TexCoord).rgb;
        normalMapSample = normalMapSample * 2.0 - 1.0;  // [0,1] → [-1,1]
        normal = normalize(fs_in.TBN * normalMapSample);
    }
    else
    {
        normal = normalize(fs_in.Normal);
    }
    
    // ... 使用 normal 进行光照计算 ...
}
```

> **注意**：`u_UseNormalMap` 和 `u_NormalMap` 需要在材质系统中注册为可编辑属性。这与材质系统的 Shader 内省功能配合使用。

---

## 八、完整代码实现

### 8.1 Mesh.h 修改

```cpp
struct Vertex
{
    glm::vec3 Position;     // 位置       (12 bytes)
    glm::vec4 Color;        // 颜色       (16 bytes)
    glm::vec3 Normal;       // 法线       (12 bytes)
    glm::vec2 TexCoord;     // 纹理坐标   (8 bytes)
    glm::vec4 Tangent;      // 切线+手性  (16 bytes)  xyz=tangent, w=handedness(±1)
};
// 总计 64 bytes/顶点
```

移除注释中的旧 Vertex 定义：

```cpp
// 删除以下注释代码：
// struct Vertex
// {
//     glm::vec3 Position;
//     glm::vec3 Normal;
//     glm::vec3 Tangent;
//     glm::vec3 Binormal;
//     glm::vec2 Texcoord;
// };
```

在 `Mesh` 类中添加 Tangent 重算方法：

```cpp
class Mesh
{
public:
    // ... 现有方法 ...
    
    /// <summary>
    /// 重新计算所有顶点的切线空间（Tangent + Handedness）
    /// 基于顶点位置、法线和纹理坐标计算
    /// 适用于导入的模型或手动创建的网格
    /// </summary>
    void RecalculateTangents();
};
```

### 8.2 Mesh.cpp 修改

更新顶点缓冲区布局（两个构造函数都需要修改）：

```cpp
m_VertexBuffer->SetLayout({
    { ShaderDataType::Float3, "a_Position" },   // 位置
    { ShaderDataType::Float4, "a_Color" },      // 颜色
    { ShaderDataType::Float3, "a_Normal" },     // 法线
    { ShaderDataType::Float2, "a_TexCoord" },   // 纹理坐标
    { ShaderDataType::Float4, "a_Tangent" },    // 切线 + 手性
});
```

添加 `RecalculateTangents` 实现：

```cpp
void Mesh::RecalculateTangents()
{
    MeshTangentCalculator::Calculate(m_Vertices, m_VertexIndices);
    
    // 更新 GPU 缓冲区
    uint32_t dataSize = m_VertexCount * sizeof(Vertex);
    m_VertexBuffer->SetData(m_Vertices.data(), dataSize);
}
```

### 8.3 MeshFactory.cpp 修改

所有图元创建方法需要在顶点数据中添加 `Tangent` 字段。

以 `CreateCube` 为例：

```cpp
// 修改前（每个顶点 4 个字段）：
{{0.5f, -0.5f, 0.5f}, {1.0f, 1.0f, 1.0f, 1.0f}, {1.0f, 0.0f, 0.0f}, {1.0f, 0.0f}},

// 修改后（每个顶点 5 个字段，添加 Tangent）：
{{0.5f, -0.5f, 0.5f}, {1.0f, 1.0f, 1.0f, 1.0f}, {1.0f, 0.0f, 0.0f}, {1.0f, 0.0f}, {0.0f, 0.0f, -1.0f, 1.0f}},
```

以 `CreatePlane` 为例：

```cpp
vertices.push_back({
    { x, 0.0f, z },                 // Position
    { 1.0f, 1.0f, 1.0f, 1.0f },    // Color
    { 0.0f, 1.0f, 0.0f },          // Normal
    { u, v },                       // TexCoord
    { 1.0f, 0.0f, 0.0f, 1.0f }     // Tangent（U 方向沿 X 轴）
});
```

以 `CreateSphere` 为例：

```cpp
// 在生成顶点的循环中：
float tangentX = -sin(theta);
float tangentZ = cos(theta);
glm::vec3 tangentDir = glm::normalize(glm::vec3(tangentX, 0.0f, tangentZ));

vertices.push_back({
    { x, y, z },
    { 1.0f, 1.0f, 1.0f, 1.0f },
    normal,
    { u, v },
    { tangentDir.x, tangentDir.y, tangentDir.z, 1.0f }
});
```

### 8.4 Renderer3D.cpp 修改

`DrawMesh` 中的顶点数据复制逻辑无需修改，因为 `Vertex` 结构体的变更会自动反映在 `sizeof(Vertex)` 中。

但需要确认 `MeshVertexBufferData` 的类型仍然是 `std::vector<Vertex>`，这样新的 Tangent 字段会自动包含在内。

### 8.5 MeshTangentCalculator 工具类

见 [第六节 6.2](#62-通用-tangent-计算函数) 的完整实现。

---

## 九、对模型导入的影响

当未来实现模型导入（如 Assimp/glTF）时：

| 情况 | 处理方式 |
|------|---------|
| 模型文件包含 Tangent 数据 | 直接使用，填充到 `Vertex::Tangent` |
| 模型文件不包含 Tangent 数据 | 导入后调用 `Mesh::RecalculateTangents()` |
| glTF 格式的 Tangent（vec4） | 直接映射，格式完全一致 |
| OBJ 格式（无 Tangent） | 导入后自动计算 |

这也是选择方案 B（Tangent vec4 + Handedness）的重要原因：与 glTF 2.0 规范完全兼容。

---

## 十、文件变更总览

| 文件 | 操作 | 改动说明 |
|------|------|---------|
| `Lucky/Source/Lucky/Renderer/Mesh.h` | 修改 | `Vertex` 添加 `Tangent` 字段，`Mesh` 添加 `RecalculateTangents()` |
| `Lucky/Source/Lucky/Renderer/Mesh.cpp` | 修改 | 更新顶点缓冲区布局，实现 `RecalculateTangents()` |
| `Lucky/Source/Lucky/Renderer/MeshFactory.cpp` | 修改 | 所有图元创建方法添加 Tangent 数据 |
| `Lucky/Source/Lucky/Renderer/MeshTangentCalculator.h` | 新建 | Tangent 计算工具类声明 |
| `Lucky/Source/Lucky/Renderer/MeshTangentCalculator.cpp` | 新建 | Tangent 计算实现 |
| `Luck3DApp/Assets/Shaders/Standard.glsl` | 修改 | 添加 `a_Tangent` 输入，TBN 矩阵计算，法线贴图采样 |
| `Luck3DApp/Assets/Shaders/InternalError.glsl` | 修改 | 添加 `a_Tangent` 输入（可以忽略不使用） |

---

## 十一、验证方法

### 11.1 基本渲染验证

1. 修改 Vertex 结构后，确认所有现有图元仍然正确渲染
2. 确认光照效果不变（Tangent 字段不影响现有光照计算）

### 11.2 Tangent 方向验证

1. 在 Fragment Shader 中临时输出 Tangent 方向作为颜色：
   ```glsl
   FragColor = vec4(fs_in.TBN[0] * 0.5 + 0.5, 1.0);  // Tangent 可视化
   ```
2. 确认 Cube 的每个面显示不同颜色（Tangent 方向不同）
3. 确认 Sphere 的 Tangent 方向沿经线方向平滑变化

### 11.3 法线贴图验证

1. 创建一个带法线贴图的材质
2. 应用到 Cube 和 Sphere 上
3. 确认法线贴图效果正确（凹凸感、光照响应）
4. 旋转光源，确认法线贴图在各角度下表现正确

### 11.4 通用 Tangent 计算验证

1. 创建一个 Cube，手动清除其 Tangent 数据
2. 调用 `RecalculateTangents()`
3. 确认计算结果与解析计算一致（或非常接近）

---

## 十二、设计决策记录

| 决策 | 选择 | 原因 |
|------|------|------|
| Tangent 存储方式 | 方案 B（vec4, w=handedness） | 业界标准（glTF 2.0），空间效率好，支持镜像 UV |
| Color 字段 | 方案 A（保留） | 避免破坏性修改，为顶点着色预留空间 |
| 原生图元 Tangent | 解析计算 | 精确，无数值误差 |
| 导入模型 Tangent | 通用计算（MeshTangentCalculator） | 通用性好，一次实现处处可用 |
| Tangent 计算工具 | 独立类（MeshTangentCalculator） | 单一职责，可被 Mesh 和模型导入器复用 |

---

## 十三、风险评估与回退方案

### 13.1 风险评估

| 风险 | 概率 | 影响 | 缓解措施 |
|------|------|------|---------|
| Vertex 结构变更导致现有场景渲染异常 | 中 | 高 | 同步更新所有 Shader 的顶点属性布局 |
| Tangent 计算错误导致法线贴图翻转 | 中 | 中 | 使用 Tangent 可视化调试，与参考引擎对比 |
| 性能下降（顶点大小增加 33%） | 低 | 低 | 当前场景规模小，影响可忽略 |
| 模型导入器不兼容新 Vertex 格式 | 低 | 中 | 模型导入器尚未实现，可以直接适配新格式 |

### 13.2 回退方案

如果 Phase 3 实施过程中遇到严重问题：

1. **最小回退**：保留 Tangent 字段但填充默认值 `(1,0,0,1)`，不启用法线贴图功能
2. **完全回退**：恢复原始 Vertex 结构（48 bytes），移除所有 Tangent 相关代码

### 13.3 实施建议

由于本 Phase 涉及面较广，建议分步实施：

| 步骤 | 内容 | 验证点 |
|------|------|--------|
| 1 | 修改 `Vertex` 结构，添加 `Tangent` 字段 | 编译通过 |
| 2 | 更新 `Mesh.cpp` 顶点缓冲区布局 | 编译通过 |
| 3 | 更新所有 Shader 的顶点属性 | 现有渲染不受影响 |
| 4 | 更新 `MeshFactory` 所有图元的 Tangent 数据 | 图元正确渲染 |
| 5 | 实现 `MeshTangentCalculator` | 单元测试通过 |
| 6 | 添加 `Mesh::RecalculateTangents()` | 功能测试通过 |
| 7 | 更新 Shader 添加法线贴图支持 | 法线贴图效果正确 |
