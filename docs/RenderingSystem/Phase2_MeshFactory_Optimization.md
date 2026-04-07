# Phase 2：MeshFactory 工厂优化 — 参数化创建 + 网格缓存

> **文档版本**：v1.0  
> **创建日期**：2026-04-07  
> **最后更新**：2026-04-07  
> **前置依赖**：Phase 1（补全基础图元网格 + 统一工厂入口）  
> **文档说明**：本文档描述 MeshFactory 的进一步优化，包括参数化图元创建、默认网格缓存机制，以减少重复创建开销并提供更灵活的图元定制能力。所有代码可直接对照实现。

---

## 目录

- [一、现状分析（Phase 1 完成后）](#一现状分析phase-1-完成后)
  - [1.1 已实现的功能](#11-已实现的功能)
  - [1.2 待优化问题](#12-待优化问题)
- [二、改进目标](#二改进目标)
- [三、涉及的文件](#三涉及的文件)
- [四、参数化图元创建](#四参数化图元创建)
  - [4.1 设计决策：参数结构体 vs 直接参数](#41-设计决策参数结构体-vs-直接参数)
  - [4.2 方案 A：PrimitiveParams 参数结构体（推荐）](#42-方案-aprimitiveParams-参数结构体推荐)
  - [4.3 方案 B：保持独立参数 + CreatePrimitive 使用默认参数](#43-方案-b保持独立参数--createprimitive-使用默认参数)
  - [4.4 方案对比与推荐](#44-方案对比与推荐)
- [五、网格缓存机制](#五网格缓存机制)
  - [5.1 缓存策略设计](#51-缓存策略设计)
  - [5.2 方案 A：仅缓存默认参数网格（推荐）](#52-方案-a仅缓存默认参数网格推荐)
  - [5.3 方案 B：基于参数哈希的完整缓存](#53-方案-b基于参数哈希的完整缓存)
  - [5.4 方案对比与推荐](#54-方案对比与推荐)
  - [5.5 缓存生命周期管理](#55-缓存生命周期管理)
  - [5.6 Copy-on-Write 安全性分析](#56-copy-on-write-安全性分析)
- [六、完整代码实现](#六完整代码实现)
  - [6.1 MeshFactory.h 修改](#61-meshfactoryh-修改)
  - [6.2 MeshFactory.cpp 修改](#62-meshfactorycpp-修改)
- [七、文件变更总览](#七文件变更总览)
- [八、验证方法](#八验证方法)
- [九、设计决策记录](#九设计决策记录)

---

## 一、现状分析（Phase 1 完成后）

### 1.1 已实现的功能

| 功能 | 状态 |
|------|------|
| `CreatePrimitive(PrimitiveType)` 统一入口 | ? |
| `CreateCube()` | ? |
| `CreatePlane(subdivisions)` | ? |
| `CreateSphere(segments, rings)` | ? |
| `CreateCylinder(segments)` | ? |
| `CreateCapsule(segments, rings)` | ? |
| `MeshFilterComponent` 使用 `CreatePrimitive` | ? |
| 编辑器菜单支持所有图元 | ? |

### 1.2 待优化问题

| 编号 | 问题 | 影响 | 优先级 |
|------|------|------|--------|
| **O-01** | `CreatePrimitive` 只能使用默认参数 | 无法通过统一入口创建自定义参数的图元 | ?? 中 |
| **O-02** | 每次创建都重新生成顶点数据和 GPU 缓冲区 | 场景中 10 个默认 Cube 会创建 10 份相同的 GPU 数据 | ?? 中 |
| **O-03** | 无法在运行时查询图元的创建参数 | Inspector 中无法显示/编辑图元参数 | ?? 低 |

---

## 二、改进目标

1. **参数化创建**：`CreatePrimitive` 支持传入自定义参数
2. **默认网格缓存**：使用默认参数创建的图元自动缓存，避免重复创建
3. **缓存清理**：提供 `ClearCache` 方法，在引擎关闭或场景切换时释放缓存

---

## 三、涉及的文件

| 文件路径 | 操作 | 说明 |
|---------|------|------|
| `Lucky/Source/Lucky/Renderer/MeshFactory.h` | 修改 | 添加缓存相关声明 |
| `Lucky/Source/Lucky/Renderer/MeshFactory.cpp` | 修改 | 实现缓存逻辑 |
| `Lucky/Source/Lucky/Renderer/Renderer3D.cpp` | 修改 | 在 `Shutdown` 中调用 `ClearCache` |

---

## 四、参数化图元创建

### 4.1 设计决策：参数结构体 vs 直接参数

当前各创建方法已经有独立的参数（如 `CreateSphere(segments, rings)`），问题是 `CreatePrimitive(PrimitiveType)` 统一入口无法传递这些参数。

### 4.2 方案 A：PrimitiveParams 参数结构体（推荐）

定义一个通用的参数结构体，包含所有图元类型可能用到的参数：

```cpp
/// <summary>
/// 图元创建参数
/// </summary>
struct PrimitiveParams
{
    // Plane
    uint32_t Subdivisions = 1;
    
    // Sphere / Capsule
    uint32_t Segments = 32;
    uint32_t Rings = 16;
    
    // Capsule 特有
    uint32_t CapsuleRings = 8;
    
    // 通用
    float Radius = 0.5f;
    float Height = 1.0f;
    
    /// <summary>
    /// 返回默认参数
    /// </summary>
    static PrimitiveParams Default() { return {}; }
};
```

统一入口改为：

```cpp
static Ref<Mesh> CreatePrimitive(PrimitiveType type, const PrimitiveParams& params = PrimitiveParams::Default());
```

**优点**：
- 统一入口可以传递任意参数
- 新增参数不需要修改函数签名
- 默认参数使得无参调用仍然有效

**缺点**：
- 参数结构体中有些字段对某些图元类型无意义（如 Cube 不需要 Segments）
- 增加了一层间接性

### 4.3 方案 B：保持独立参数 + CreatePrimitive 使用默认参数

保持当前设计不变，`CreatePrimitive` 始终使用默认参数，需要自定义参数时直接调用具体方法：

```cpp
// 默认参数
auto mesh = MeshFactory::CreatePrimitive(PrimitiveType::Sphere);

// 自定义参数
auto mesh = MeshFactory::CreateSphere(64, 32);
```

**优点**：
- 最简单，无需额外结构体
- 各方法的参数语义清晰
- 不引入不必要的复杂度

**缺点**：
- `CreatePrimitive` 无法传递自定义参数
- 如果需要根据运行时数据创建图元，需要自己写 switch

### 4.4 方案对比与推荐

| 维度 | 方案 A（参数结构体） | 方案 B（保持独立参数） |
|------|---------------------|----------------------|
| 复杂度 | 中 | 低 |
| 灵活性 | 高 | 中 |
| 当前需求匹配度 | 超出当前需求 | 恰好满足 |
| 未来扩展性 | 好 | 一般 |
| 实现工作量 | 中 | 无 |

**推荐方案 B**（当前阶段）：

理由：
1. 当前引擎中，原生图元只在两个地方创建：`MeshFilterComponent` 构造函数和编辑器菜单，都使用默认参数
2. 场景序列化只保存 `PrimitiveType`，不保存创建参数，因此自定义参数的图元无法持久化
3. 等到引入 Inspector 中的图元参数编辑功能时，再升级为方案 A

**如果选择方案 A**，需要同步修改：
- `MeshFilterComponent` 添加 `PrimitiveParams` 成员
- 场景序列化添加参数的序列化/反序列化
- Inspector 添加参数编辑 UI

---

## 五、网格缓存机制

### 5.1 缓存策略设计

核心思路：使用默认参数创建的原生图元是完全相同的，可以共享同一个 `Mesh` 对象，避免重复创建 GPU 缓冲区。

### 5.2 方案 A：仅缓存默认参数网格（推荐）

只缓存通过 `CreatePrimitive(PrimitiveType)` 创建的默认参数网格。直接调用 `CreateSphere(64, 32)` 等自定义参数方法时不走缓存。

#### 5.2.1 数据结构

```cpp
class MeshFactory
{
    // ... public 方法 ...
    
private:
    /// <summary>
    /// 默认参数图元网格缓存
    /// Key: PrimitiveType, Value: 缓存的 Mesh
    /// </summary>
    static std::unordered_map<PrimitiveType, Ref<Mesh>> s_PrimitiveCache;
};
```

> **注意**：`PrimitiveType` 是 `enum class`，需要提供哈希函数才能用作 `unordered_map` 的 Key。

#### 5.2.2 哈希函数

```cpp
// 在 MeshFactory.cpp 中定义
struct PrimitiveTypeHash
{
    std::size_t operator()(PrimitiveType type) const
    {
        return std::hash<int>()(static_cast<int>(type));
    }
};

static std::unordered_map<PrimitiveType, Ref<Mesh>, PrimitiveTypeHash> s_PrimitiveCache;
```

#### 5.2.3 缓存逻辑

```cpp
Ref<Mesh> MeshFactory::CreatePrimitive(PrimitiveType type)
{
    if (type == PrimitiveType::None)
        return nullptr;
    
    // 查找缓存
    auto it = s_PrimitiveCache.find(type);
    if (it != s_PrimitiveCache.end())
    {
        return it->second;
    }
    
    // 缓存未命中，创建新网格
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
        s_PrimitiveCache[type] = mesh;  // 存入缓存
    }
    
    return mesh;
}
```

#### 5.2.4 缓存清理

```cpp
void MeshFactory::ClearCache()
{
    s_PrimitiveCache.clear();
}
```

**优点**：
- 实现极其简单
- 缓存 Key 就是 `PrimitiveType` 枚举值，无需复杂的哈希计算
- 最多只缓存 5 个网格（Cube/Plane/Sphere/Cylinder/Capsule），内存开销可忽略

**缺点**：
- 自定义参数的网格不走缓存

### 5.3 方案 B：基于参数哈希的完整缓存

为每种参数组合计算哈希值，作为缓存 Key：

```cpp
struct MeshCacheKey
{
    PrimitiveType Type;
    uint32_t Param1;  // segments / subdivisions
    uint32_t Param2;  // rings
    
    bool operator==(const MeshCacheKey& other) const
    {
        return Type == other.Type && Param1 == other.Param1 && Param2 == other.Param2;
    }
};

struct MeshCacheKeyHash
{
    std::size_t operator()(const MeshCacheKey& key) const
    {
        std::size_t h1 = std::hash<int>()(static_cast<int>(key.Type));
        std::size_t h2 = std::hash<uint32_t>()(key.Param1);
        std::size_t h3 = std::hash<uint32_t>()(key.Param2);
        return h1 ^ (h2 << 16) ^ (h3 << 32);
    }
};

static std::unordered_map<MeshCacheKey, Ref<Mesh>, MeshCacheKeyHash> s_MeshCache;
```

**优点**：
- 任何参数组合都能缓存
- 完全避免重复创建

**缺点**：
- 实现复杂度高
- 缓存可能无限增长（不同参数组合）
- 需要 LRU 或大小限制策略
- 当前阶段没有自定义参数的使用场景

### 5.4 方案对比与推荐

| 维度 | 方案 A（仅默认参数） | 方案 B（完整参数哈希） |
|------|---------------------|----------------------|
| 实现复杂度 | 极低 | 高 |
| 缓存命中率 | 高（当前场景下 100%） | 更高 |
| 内存管理 | 固定上限（5 个） | 需要 LRU/大小限制 |
| 当前需求匹配度 | 完全满足 | 超出需求 |

**推荐方案 A**：当前阶段只需缓存默认参数网格。理由：
1. 编辑器创建的图元全部使用默认参数
2. 场景反序列化重建的图元也使用默认参数
3. 缓存上限固定为 5 个，无需担心内存泄漏

### 5.5 缓存生命周期管理

#### 5.5.1 初始化时机

缓存采用**懒加载**策略，不在 `Init` 中预创建，而是在首次 `CreatePrimitive` 调用时创建并缓存。

**理由**：
- 不是所有图元类型都会被使用
- 避免启动时的不必要开销

#### 5.5.2 清理时机

在 `Renderer3D::Shutdown()` 中调用 `MeshFactory::ClearCache()`：

```cpp
void Renderer3D::Shutdown()
{
    MeshFactory::ClearCache();
}
```

**理由**：
- `MeshFactory` 创建的 Mesh 持有 GPU 资源（VAO/VBO/EBO）
- 引擎关闭时需要确保 GPU 资源在 OpenGL 上下文销毁前释放
- `Renderer3D::Shutdown()` 是渲染器资源清理的标准位置

#### 5.5.3 场景切换时是否清理

**不需要**。原因：
- 缓存的是通用图元网格，不与特定场景绑定
- 场景切换后新场景可能仍然需要相同的图元
- 缓存总量极小（最多 5 个），不值得清理后重建

### 5.6 Copy-on-Write 安全性分析

缓存共享意味着多个实体的 `MeshFilterComponent` 持有同一个 `Ref<Mesh>` 对象。需要确认这是否安全：

| 操作 | 是否安全 | 说明 |
|------|---------|------|
| 读取顶点数据 | ? 安全 | 只读操作 |
| 渲染（DrawMesh） | ? 安全 | 只读取 VAO/VBO/EBO |
| 修改顶点数据 | ? 不安全 | 会影响所有共享该 Mesh 的实体 |
| 添加/修改 SubMesh | ? 不安全 | 同上 |
| 设置名称 | ?? 需注意 | 名称修改会影响所有引用 |

**当前阶段的安全性**：

当前引擎中，原生图元的 Mesh 数据在创建后**不会被修改**：
- 渲染器只读取顶点和索引数据
- Inspector 不提供网格编辑功能
- 没有运行时网格变形

因此，**当前阶段缓存共享是安全的**。

**未来风险**：

如果未来引入以下功能，需要实现 Copy-on-Write：
- 运行时网格变形
- 每实体独立的 SubMesh 配置
- 网格编辑器

**Copy-on-Write 预留接口**（可选，当前不实现）：

```cpp
/// <summary>
/// 获取网格的独立副本（用于需要修改网格数据的场景）
/// </summary>
static Ref<Mesh> CloneMesh(const Ref<Mesh>& source);
```

---

## 六、完整代码实现

### 6.1 MeshFactory.h 修改

在 Phase 1 的基础上，添加缓存相关声明：

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
    /// 支持默认参数网格缓存，避免重复创建
    /// </summary>
    class MeshFactory
    {
    public:
        /// <summary>
        /// 根据图元类型创建网格（统一入口）
        /// 使用默认参数，自动缓存
        /// 多次调用相同类型会返回缓存的同一个 Mesh 对象
        /// </summary>
        /// <param name="type">图元类型</param>
        /// <returns>创建的网格，类型为 None 时返回 nullptr</returns>
        static Ref<Mesh> CreatePrimitive(PrimitiveType type);
        
        /// <summary>
        /// 创建立方体网格（不走缓存，每次创建新实例）
        /// 中心在原点，边长为 1
        /// </summary>
        static Ref<Mesh> CreateCube();
        
        /// <summary>
        /// 创建平面网格（不走缓存，每次创建新实例）
        /// XZ 平面，中心在原点，大小为 1×1
        /// </summary>
        /// <param name="subdivisions">细分次数，默认 1</param>
        static Ref<Mesh> CreatePlane(uint32_t subdivisions = 1);
        
        /// <summary>
        /// 创建球体网格（不走缓存，每次创建新实例）
        /// 中心在原点，半径为 0.5
        /// </summary>
        /// <param name="segments">经线段数，默认 32，最小 3</param>
        /// <param name="rings">纬线段数，默认 16，最小 2</param>
        static Ref<Mesh> CreateSphere(uint32_t segments = 32, uint32_t rings = 16);
        
        /// <summary>
        /// 创建圆柱体网格（不走缓存，每次创建新实例）
        /// 中心在原点，高度为 1，半径为 0.5
        /// </summary>
        /// <param name="segments">圆周段数，默认 32，最小 3</param>
        static Ref<Mesh> CreateCylinder(uint32_t segments = 32);
        
        /// <summary>
        /// 创建胶囊体网格（不走缓存，每次创建新实例）
        /// 中心在原点，总高度为 2，半径为 0.5
        /// </summary>
        /// <param name="segments">圆周段数，默认 32，最小 3</param>
        /// <param name="rings">每个半球的纬线段数，默认 8，最小 2</param>
        static Ref<Mesh> CreateCapsule(uint32_t segments = 32, uint32_t rings = 8);
        
        /// <summary>
        /// 清除所有缓存的网格
        /// 应在引擎关闭时调用（Renderer3D::Shutdown）
        /// </summary>
        static void ClearCache();
        
    private:
        /// <summary>
        /// 获取图元类型对应的名称字符串
        /// </summary>
        static const char* GetPrimitiveTypeName(PrimitiveType type);
    };
}
```

### 6.2 MeshFactory.cpp 修改

在 Phase 1 的基础上，添加缓存实现：

```cpp
#include "lcpch.h"
#include "MeshFactory.h"

#include <glm/gtc/constants.hpp>

namespace Lucky
{
    // ==================== 缓存 ====================
    
    /// <summary>
    /// PrimitiveType 哈希函数
    /// </summary>
    struct PrimitiveTypeHash
    {
        std::size_t operator()(PrimitiveType type) const
        {
            return std::hash<int>()(static_cast<int>(type));
        }
    };
    
    /// <summary>
    /// 默认参数图元网格缓存
    /// </summary>
    static std::unordered_map<PrimitiveType, Ref<Mesh>, PrimitiveTypeHash> s_PrimitiveCache;
    
    // ==================== 统一入口（带缓存） ====================
    
    Ref<Mesh> MeshFactory::CreatePrimitive(PrimitiveType type)
    {
        if (type == PrimitiveType::None)
            return nullptr;
        
        // 查找缓存
        auto it = s_PrimitiveCache.find(type);
        if (it != s_PrimitiveCache.end())
        {
            return it->second;
        }
        
        // 缓存未命中，创建新网格
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
            s_PrimitiveCache[type] = mesh;  // 存入缓存
            LF_CORE_INFO("MeshFactory: Cached primitive mesh '{0}'", GetPrimitiveTypeName(type));
        }
        
        return mesh;
    }
    
    void MeshFactory::ClearCache()
    {
        s_PrimitiveCache.clear();
        LF_CORE_INFO("MeshFactory: Cache cleared");
    }
    
    // ==================== 其他方法保持不变 ====================
    // CreateCube(), CreatePlane(), CreateSphere(), CreateCylinder(), CreateCapsule()
    // GetPrimitiveTypeName()
    // ... 与 Phase 1 相同 ...
}
```

### 6.3 Renderer3D.cpp 修改

在 `Shutdown` 中添加缓存清理：

```cpp
void Renderer3D::Shutdown()
{
    MeshFactory::ClearCache();  // 清除图元网格缓存
}
```

---

## 七、文件变更总览

| 文件 | 操作 | 改动说明 |
|------|------|---------|
| `Lucky/Source/Lucky/Renderer/MeshFactory.h` | 修改 | 添加 `ClearCache()` 声明，更新注释说明缓存行为 |
| `Lucky/Source/Lucky/Renderer/MeshFactory.cpp` | 修改 | 添加 `s_PrimitiveCache` 静态变量、`PrimitiveTypeHash` 结构体、缓存查找/存储逻辑、`ClearCache()` 实现 |
| `Lucky/Source/Lucky/Renderer/Renderer3D.cpp` | 修改 | 在 `Shutdown()` 中调用 `MeshFactory::ClearCache()` |

---

## 八、验证方法

### 8.1 缓存命中验证

1. 在场景中创建两个 Cube 实体
2. 检查日志：第一个 Cube 应输出 `"MeshFactory: Cached primitive mesh 'Cube'"`
3. 第二个 Cube 不应输出缓存日志（命中缓存）
4. 验证两个实体的 `MeshFilterComponent.Mesh` 指向同一个对象（`Ref` 引用计数 > 1）

### 8.2 缓存清理验证

1. 创建场景并添加图元
2. 关闭引擎
3. 检查日志：应输出 `"MeshFactory: Cache cleared"`
4. 确认无内存泄漏（GPU 资源正确释放）

### 8.3 独立创建验证

1. 直接调用 `MeshFactory::CreateSphere(64, 32)`（自定义参数）
2. 确认返回的 Mesh 与缓存中的默认 Sphere 不是同一个对象
3. 确认自定义参数的 Sphere 有更多顶点

### 8.4 共享安全性验证

1. 创建两个 Cube 实体（共享同一个缓存 Mesh）
2. 分别为它们设置不同的材质
3. 确认渲染结果正确（材质独立，网格共享）

---

## 九、设计决策记录

| 决策 | 选择 | 原因 |
|------|------|------|
| 参数化方案 | 方案 B（保持独立参数） | 当前无自定义参数需求，序列化不支持参数持久化 |
| 缓存策略 | 方案 A（仅默认参数） | 实现简单，缓存上限固定，完全满足当前需求 |
| 缓存初始化 | 懒加载 | 避免启动时不必要的开销 |
| 缓存清理时机 | `Renderer3D::Shutdown()` | GPU 资源需要在 OpenGL 上下文销毁前释放 |
| Copy-on-Write | 当前不实现 | 原生图元在创建后不会被修改 |
