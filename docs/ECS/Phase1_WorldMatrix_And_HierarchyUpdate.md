# Phase 1：WorldMatrix 缓存 + 层级 Transform 更新 + 渲染集成

## 1. 概述

Phase 1 是 Transform 层级系统的核心基础层，目标是：
1. 为 `TransformComponent` 添加 **World Matrix 缓存**，区分 Local Transform 和 World Transform
2. 在 `Scene::OnUpdate` 中实现**按层级顺序的 Transform 更新**，使父节点的变换能正确传播到子节点
3. 修改渲染流程，使用 **World Matrix** 而非 Local Matrix 进行渲染

### 核心行为定义

- **Local Transform**：相对于父节点的变换（Translation、Rotation、Scale），即编辑器 Inspector 中显示和编辑的值
- **World Transform**：最终在世界空间中的变换，用于渲染
- **变换公式**：`WorldMatrix = Parent.WorldMatrix × LocalMatrix`
- **根节点**：`WorldMatrix = LocalMatrix`（父节点的 WorldMatrix 为单位矩阵）

---

## 2. 涉及的文件

### 需要修改的文件

| 文件路径 | 说明 |
|---------|------|
| `Lucky/Source/Lucky/Scene/Components/TransformComponent.h` | 添加 WorldMatrix 缓存字段，重命名方法 |
| `Lucky/Source/Lucky/Scene/Scene.h` | 添加 Transform 层级更新相关方法声明 |
| `Lucky/Source/Lucky/Scene/Scene.cpp` | 实现层级更新逻辑，修改渲染流程 |

### 不需要修改的文件

| 文件路径 | 说明 |
|---------|------|
| `Lucky/Source/Lucky/Scene/Components/RelationshipComponent.h` | 父子关系已实现，无需修改 |
| `Lucky/Source/Lucky/Scene/Entity.h / Entity.cpp` | Phase 1 不涉及 Entity 接口变更 |
| `Luck3DApp/Source/Panels/InspectorPanel.cpp` | Inspector 编辑的是 Local Transform，无需修改 |

---

## 3. TransformComponent 改造

### 3.1 目标

为 `TransformComponent` 添加 World Matrix 缓存，并明确区分 Local 和 World 的概念。

### 3.2 方案选择：GetTransform() 方法重命名策略

当前 `TransformComponent` 中的 `GetTransform()` 方法返回的是 Local Matrix。引入 World Matrix 后，需要明确命名以避免混淆。

#### 方案 A：重命名为 GetLocalTransform() + 新增 GetWorldTransform()（? 推荐，最优）

**做法**：将现有的 `GetTransform()` 重命名为 `GetLocalTransform()`，新增 `GetWorldTransform()` 返回缓存的 World Matrix。同时将 `SetTransform()` 重命名为 `SetLocalTransform()`。

**优点**：
- 命名语义最清晰，Local 和 World 一目了然
- 与 Unity 的 `localPosition` / `position` 命名哲学一致
- 编译器会在所有调用处报错，强制开发者审视每个调用点应该使用 Local 还是 World

**缺点**：
- 需要修改所有现有的 `GetTransform()` / `SetTransform()` 调用处
- 一次性改动量较大

**需要修改的调用处**：

| 文件 | 当前调用 | 应改为 |
|------|---------|--------|
| `Scene.cpp` → `OnUpdate` 渲染部分 | `transform.GetTransform()` | `transform.GetWorldTransform()` |
| `Renderer3D.cpp` → `DrawMesh` | 参数 `transform` 已经是 `mat4`，无需改 | 无需改（调用方传入 WorldMatrix 即可） |

#### 方案 B：保留 GetTransform() 作为 GetLocalTransform() 的别名（其次）

**做法**：保留 `GetTransform()` 不变（仍返回 Local Matrix），新增 `GetWorldTransform()`。

```cpp
// 保留旧接口（返回 Local Matrix）
glm::mat4 GetTransform() const { return GetLocalTransform(); }

// 新增
glm::mat4 GetLocalTransform() const { /* 原有逻辑 */ }
const glm::mat4& GetWorldTransform() const { return m_WorldMatrix; }
```

**优点**：
- 向后兼容，现有代码不需要修改
- 改动量最小

**缺点**：
- `GetTransform()` 的语义不够明确，新开发者可能误用
- 渲染处仍需手动改为 `GetWorldTransform()`，但其他地方可能遗漏

#### 方案 C：GetTransform() 改为返回 World Matrix（不推荐）

**做法**：让 `GetTransform()` 返回 World Matrix 而非 Local Matrix。

**优点**：
- 现有渲染代码无需修改

**缺点**：
- 语义突变，所有依赖 `GetTransform()` 返回 Local Matrix 的代码会静默出错
- 违反最小惊讶原则
- 不推荐

### 3.3 WorldMatrix 缓存字段的访问控制

#### 方案 A：WorldMatrix 为 private，仅通过 getter 访问（? 推荐，最优）

```cpp
private:
    glm::mat4 m_WorldMatrix = glm::mat4(1.0f);

public:
    const glm::mat4& GetWorldTransform() const { return m_WorldMatrix; }

    // Scene 需要在更新层级时写入 WorldMatrix
    friend class Scene;
```

**优点**：
- 封装性好，外部只能读取不能随意修改
- 明确表达 "WorldMatrix 由系统维护，不应手动设置" 的语义

**缺点**：
- 需要 `friend class Scene` 或提供一个 private setter

#### 方案 B：WorldMatrix 为 public（其次）

```cpp
public:
    glm::mat4 WorldMatrix = glm::mat4(1.0f);
    
    const glm::mat4& GetWorldTransform() const { return WorldMatrix; }
```

**优点**：
- 实现最简单，与现有的 `Translation`、`Scale` 等 public 字段风格一致
- Scene 可以直接写入，无需 friend

**缺点**：
- 任何代码都可以随意修改 WorldMatrix，可能导致不一致
- 但考虑到当前项目中 `Translation`、`Scale` 也是 public 的，风格上是一致的

### 3.4 完整的 TransformComponent 修改方案

以下基于 **3.2 方案 A** + **3.3 方案 B**（兼顾清晰命名和与现有风格一致）给出完整代码：

```cpp
#pragma once

#define GLM_ENABLE_EXPERIMENTAL
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtx/norm.hpp>
#include <glm/gtx/quaternion.hpp>

#include "Lucky/Math/Math.h"

namespace Lucky
{
    struct TransformComponent
    {
        // ==================== Local Transform ====================
        // 相对于父节点的变换，编辑器 Inspector 中显示和编辑的值
        glm::vec3 Translation = { 0.0f, 0.0f, 0.0f };
        glm::vec3 Scale = { 1.0f, 1.0f, 1.0f };

        // ==================== World Transform（缓存）====================
        // 由 Scene 在每帧更新时计算并写入，外部不应手动修改
        // WorldMatrix = Parent.WorldMatrix × LocalMatrix
        glm::mat4 WorldMatrix = glm::mat4(1.0f);

    private:
        // 旋转相关字段保持 private（原有设计不变）
        glm::vec3 RotationEuler = { 0.0f, 0.0f, 0.0f };
        glm::quat Rotation = { 1.0f, 0.0f, 0.0f, 0.0f };

    public:
        TransformComponent() = default;
        TransformComponent(const TransformComponent& other) = default;
        TransformComponent(const glm::vec3& translation)
            : Translation(translation)
        {
        }

        /// <summary>
        /// 获取 Local Transform 矩阵（相对于父节点）
        /// </summary>
        glm::mat4 GetLocalTransform() const
        {
            return glm::translate(glm::mat4(1.0f), Translation)
                * glm::toMat4(Rotation)
                * glm::scale(glm::mat4(1.0f), Scale);
        }

        /// <summary>
        /// 从矩阵设置 Local Transform（分解为 Translation、Rotation、Scale）
        /// </summary>
        void SetLocalTransform(const glm::mat4& transform)
        {
            Math::DecomposeTransform(transform, Translation, Rotation, Scale);
            RotationEuler = glm::eulerAngles(Rotation);
        }

        /// <summary>
        /// 获取 World Transform 矩阵（最终世界空间变换，用于渲染）
        /// </summary>
        const glm::mat4& GetWorldTransform() const
        {
            return WorldMatrix;
        }

        // ==================== 旋转相关方法（保持不变）====================

        glm::vec3 GetRotationEuler() const
        {
            return RotationEuler;
        }

        void SetRotationEuler(const glm::vec3& euler)
        {
            RotationEuler = euler;
            Rotation = glm::quat(RotationEuler);
        }

        glm::quat GetRotation() const
        {
            return Rotation;
        }

        void SetRotation(const glm::quat& quat)
        {
            // （原有的 SetRotation 逻辑完全保持不变）
            auto wrapToPi = [](glm::vec3 v)
            {
                return glm::mod(v + glm::pi<float>(), 2.0f * glm::pi<float>()) - glm::pi<float>();
            };

            auto originalEuler = RotationEuler;
            Rotation = quat;
            RotationEuler = glm::eulerAngles(Rotation);

            glm::vec3 alternate1 = { RotationEuler.x - glm::pi<float>(), glm::pi<float>() - RotationEuler.y, RotationEuler.z - glm::pi<float>() };
            glm::vec3 alternate2 = { RotationEuler.x + glm::pi<float>(), glm::pi<float>() - RotationEuler.y, RotationEuler.z - glm::pi<float>() };
            glm::vec3 alternate3 = { RotationEuler.x + glm::pi<float>(), glm::pi<float>() - RotationEuler.y, RotationEuler.z + glm::pi<float>() };
            glm::vec3 alternate4 = { RotationEuler.x - glm::pi<float>(), glm::pi<float>() - RotationEuler.y, RotationEuler.z + glm::pi<float>() };

            float distance0 = glm::length2(wrapToPi(RotationEuler - originalEuler));
            float distance1 = glm::length2(wrapToPi(alternate1 - originalEuler));
            float distance2 = glm::length2(wrapToPi(alternate2 - originalEuler));
            float distance3 = glm::length2(wrapToPi(alternate3 - originalEuler));
            float distance4 = glm::length2(wrapToPi(alternate4 - originalEuler));

            float best = distance0;
            if (distance1 < best)
            {
                best = distance1;
                RotationEuler = alternate1;
            }
            if (distance2 < best)
            {
                best = distance2;
                RotationEuler = alternate2;
            }
            if (distance3 < best)
            {
                best = distance3;
                RotationEuler = alternate3;
            }
            if (distance4 < best)
            {
                best = distance4;
                RotationEuler = alternate4;
            }

            RotationEuler = wrapToPi(RotationEuler);
        }
    };
}
```

> **注意**：`SetTransform()` 重命名为 `SetLocalTransform()`。如果项目中有其他地方调用了 `SetTransform()`，需要一并修改。可以通过全局搜索 `SetTransform` 和 `GetTransform` 来找到所有调用处。

---

## 4. Scene 层级 Transform 更新

### 4.1 目标

在每帧渲染前，按照父子层级顺序，从根节点开始递归计算每个实体的 World Matrix。

### 4.2 更新流程

```
Scene::OnUpdate(dt, camera)
│
├── 1. UpdateTransformHierarchy()          // 更新所有实体的 WorldMatrix
│   ├── 遍历所有根节点（Parent == 0）
│   │   ├── UpdateWorldTransform(rootEntity, glm::mat4(1.0f))
│   │   │   ├── WorldMatrix = parentWorldMatrix × LocalMatrix
│   │   │   ├── 递归处理每个子节点
│   │   │   │   └── UpdateWorldTransform(child, this.WorldMatrix)
│   │   │   └── ...
│   │   └── ...
│   └── ...
│
├── 2. Renderer3D::BeginScene(camera)      // 开始渲染
├── 3. 遍历所有 Mesh 实体，使用 WorldMatrix 渲染
└── 4. Renderer3D::EndScene()              // 结束渲染
```

### 4.3 方案选择：层级遍历策略

#### 方案 A：递归遍历（? 推荐，最优）

**做法**：从根节点开始，递归遍历子节点，将父节点的 WorldMatrix 传递给子节点。

```cpp
void Scene::UpdateTransformHierarchy()
{
    // 遍历所有拥有 TransformComponent 和 RelationshipComponent 的实体
    auto view = m_Registry.view<TransformComponent, RelationshipComponent>();
    for (auto entityID : view)
    {
        Entity entity{ entityID, this };
        // 只处理根节点（没有父节点的实体）
        if (entity.GetParentUUID() == 0)
        {
            UpdateWorldTransform(entity, glm::mat4(1.0f));
        }
    }
}

void Scene::UpdateWorldTransform(Entity entity, const glm::mat4& parentWorldMatrix)
{
    auto& transform = entity.GetComponent<TransformComponent>();
    
    // WorldMatrix = 父节点的 WorldMatrix × 自身的 LocalMatrix
    transform.WorldMatrix = parentWorldMatrix * transform.GetLocalTransform();
    
    // 递归更新所有子节点
    for (UUID childID : entity.GetChildren())
    {
        Entity child = GetEntityWithUUID(childID);
        UpdateWorldTransform(child, transform.WorldMatrix);
    }
}
```

**优点**：
- 实现直观，代码清晰易懂
- 天然保证父节点先于子节点更新
- 每个实体只计算一次

**缺点**：
- 层级很深时可能有栈溢出风险（但实际游戏场景中层级深度很少超过 20 层，不是问题）
- 每帧都会遍历所有实体，即使没有任何 Transform 变化

**性能分析**：
- 时间复杂度：O(N)，N 为实体总数
- 每个实体执行一次矩阵乘法（16 次乘法 + 12 次加法）
- 对于 1000 个实体，每帧约 28000 次浮点运算，完全可以忽略

#### 方案 B：迭代遍历 + 显式栈（其次）

**做法**：用显式栈替代递归，避免栈溢出风险。

```cpp
void Scene::UpdateTransformHierarchy()
{
    // 使用显式栈：pair<Entity, parentWorldMatrix>
    std::stack<std::pair<Entity, glm::mat4>> stack;
    
    // 将所有根节点入栈
    auto view = m_Registry.view<TransformComponent, RelationshipComponent>();
    for (auto entityID : view)
    {
        Entity entity{ entityID, this };
        if (entity.GetParentUUID() == 0)
        {
            stack.push({ entity, glm::mat4(1.0f) });
        }
    }
    
    // 深度优先遍历
    while (!stack.empty())
    {
        auto [entity, parentWorldMatrix] = stack.top();
        stack.pop();
        
        auto& transform = entity.GetComponent<TransformComponent>();
        transform.WorldMatrix = parentWorldMatrix * transform.GetLocalTransform();
        
        // 子节点入栈
        for (UUID childID : entity.GetChildren())
        {
            Entity child = GetEntityWithUUID(childID);
            stack.push({ child, transform.WorldMatrix });
        }
    }
}
```

**优点**：
- 不会栈溢出，适合极深层级
- 逻辑与递归版本等价

**缺点**：
- 代码稍复杂
- 需要额外的堆内存分配（stack）
- 对于当前项目阶段来说过度设计

#### 方案 C：拓扑排序 + 线性遍历（不推荐，后续优化时可考虑）

**做法**：维护一个按层级排序的实体列表，每帧线性遍历。

**优点**：
- 缓存友好，适合大规模场景
- 可以与 ECS 的 SoA 布局配合

**缺点**：
- 实现复杂，需要在父子关系变化时维护排序
- 当前项目规模不需要

### 4.4 Scene 类的修改

#### Scene.h 新增方法声明

```cpp
class Scene
{
    // ... 现有代码 ...

private:
    /// <summary>
    /// 更新 Transform 层级：计算所有实体的 WorldMatrix
    /// </summary>
    void UpdateTransformHierarchy();

    /// <summary>
    /// 递归更新实体的 WorldMatrix
    /// </summary>
    /// <param name="entity">当前实体</param>
    /// <param name="parentWorldMatrix">父节点的 WorldMatrix</param>
    void UpdateWorldTransform(Entity entity, const glm::mat4& parentWorldMatrix);
    
    // ... 现有代码 ...
};
```

#### Scene.cpp 修改 OnUpdate

```cpp
void Scene::OnUpdate(DeltaTime dt, EditorCamera& camera)
{
    // ===== 第一步：更新 Transform 层级 =====
    UpdateTransformHierarchy();
    
    // ===== 第二步：渲染 =====
    Renderer3D::BeginScene(camera);
    {
        auto meshGroup = m_Registry.group<TransformComponent>(entt::get<MeshFilterComponent>);

        for (auto entity : meshGroup)
        {
            auto [transform, meshFilter] = meshGroup.get<TransformComponent, MeshFilterComponent>(entity);

            // 使用 WorldMatrix 进行渲染（而非之前的 GetTransform() / GetLocalTransform()）
            Renderer3D::DrawMesh(transform.GetWorldTransform(), meshFilter.Mesh);
        }
    }
    Renderer3D::EndScene();
}
```

---

## 5. 验证清单

完成 Phase 1 后，应能通过以下验证：

### 5.1 基本功能验证

| # | 测试场景 | 预期结果 |
|---|---------|---------|
| 1 | 创建一个根节点 Cube，修改其 Position | Cube 在场景中移动（与之前行为一致） |
| 2 | 创建一个根节点 Cube，修改其 Rotation | Cube 在场景中旋转（与之前行为一致） |
| 3 | 创建一个根节点 Cube，修改其 Scale | Cube 在场景中缩放（与之前行为一致） |

### 5.2 父子层级验证

| # | 测试场景 | 预期结果 |
|---|---------|---------|
| 4 | 创建父节点 A（Position = (2,0,0)），创建子节点 B（Position = (0,0,0)），B 设为 A 的子节点 | B 在世界空间中位于 (2,0,0) |
| 5 | 在测试 4 的基础上，修改 B 的 Position 为 (1,0,0) | B 在世界空间中位于 (3,0,0) |
| 6 | 在测试 5 的基础上，修改 A 的 Position 为 (5,0,0) | B 在世界空间中位于 (6,0,0)，B 的 Inspector 中 Position 仍显示 (1,0,0) |
| 7 | 父节点 A 旋转 90°（Y 轴），子节点 B 的 Local Position = (1,0,0) | B 在世界空间中应出现在 A 的"前方"而非"右方"（旋转生效） |
| 8 | 父节点 A 的 Scale = (2,2,2)，子节点 B 的 Local Position = (1,0,0) | B 在世界空间中位于 A 的位置 + (2,0,0)（缩放影响子节点位置） |
| 9 | 三层嵌套：A → B → C，修改 A 的 Transform | C 的世界位置应正确反映 A 和 B 的累积变换 |

### 5.3 边界情况验证

| # | 测试场景 | 预期结果 |
|---|---------|---------|
| 10 | 没有父节点的实体 | WorldMatrix == LocalMatrix，行为与改造前完全一致 |
| 11 | 子节点的 Local Transform 全为默认值（Position=0, Rotation=0, Scale=1） | 子节点的 WorldMatrix == 父节点的 WorldMatrix |
| 12 | 场景中没有任何实体 | 不崩溃，正常运行 |

---

## 6. 注意事项

### 6.1 GetTransform / SetTransform 全局替换

在重命名 `GetTransform()` → `GetLocalTransform()` 和 `SetTransform()` → `SetLocalTransform()` 时，需要全局搜索以下关键词，确保所有调用处都已更新：

```
搜索关键词：
- GetTransform()
- SetTransform(
- .GetTransform()
- ->GetTransform()
```

当前已知的调用处：
- `Scene.cpp` → `OnUpdate` 中的 `transform.GetTransform()` → 改为 `transform.GetWorldTransform()`
- `InspectorPanel.cpp` → 不涉及 `GetTransform()` 调用（Inspector 直接操作 Translation/Rotation/Scale 字段）

### 6.2 序列化兼容性

如果项目中有场景序列化（SceneSerializer），`WorldMatrix` **不应被序列化**，因为它是每帧计算的缓存值。只需序列化 Local Transform（Translation、Rotation、Scale）。

### 6.3 性能说明

Phase 1 采用"每帧全量更新"策略，即使没有任何 Transform 发生变化，也会重新计算所有实体的 WorldMatrix。这对于中小规模场景（< 10000 实体）完全没有性能问题。后续 Phase 3 会引入脏标记优化。

---

## 7. 实现步骤总结

按以下顺序实现：

1. **修改 `TransformComponent.h`**
   - 添加 `WorldMatrix` 字段
   - 将 `GetTransform()` 重命名为 `GetLocalTransform()`
   - 将 `SetTransform()` 重命名为 `SetLocalTransform()`
   - 添加 `GetWorldTransform()` 方法

2. **全局搜索并修复编译错误**
   - 搜索所有 `GetTransform()` 和 `SetTransform()` 的调用处
   - 根据语义决定改为 `GetLocalTransform()` 还是 `GetWorldTransform()`

3. **修改 `Scene.h`**
   - 添加 `UpdateTransformHierarchy()` 和 `UpdateWorldTransform()` 方法声明

4. **修改 `Scene.cpp`**
   - 实现 `UpdateTransformHierarchy()` 和 `UpdateWorldTransform()`
   - 在 `OnUpdate()` 开头调用 `UpdateTransformHierarchy()`
   - 渲染部分改用 `transform.GetWorldTransform()`

5. **编译并测试**
   - 按照第 5 节的验证清单逐项测试
