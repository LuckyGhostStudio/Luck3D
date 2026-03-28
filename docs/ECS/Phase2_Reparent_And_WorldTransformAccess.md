# Phase 2：Reparent 保持 World Transform + World Transform 访问接口

## 1. 概述

Phase 2 在 Phase 1 的基础上，进一步完善 Transform 层级系统的用户体验，目标是：
1. **Reparent 时保持 World Transform 不变**：当实体的父节点发生变化时，自动反算新的 Local Transform，使实体在世界空间中的位置/旋转/缩放保持不变
2. **提供 World Position / Rotation / Scale 的便捷访问接口**：允许直接获取和设置实体在世界空间中的变换

### 前置依赖

- Phase 1 已完成（TransformComponent 具有 WorldMatrix 缓存，Scene 每帧更新层级）

---

## 2. 涉及的文件

### 需要修改的文件

| 文件路径 | 说明 |
|---------|------|
| `Lucky/Source/Lucky/Scene/Entity.h` | 添加 World Transform 访问方法声明 |
| `Lucky/Source/Lucky/Scene/Entity.cpp` | 实现 World Transform 访问方法 + 修改 SetParent 逻辑 |
| `Lucky/Source/Lucky/Scene/Components/TransformComponent.h` | 可能需要添加辅助方法 |
| `Lucky/Source/Lucky/Math/Math.h` | 可能需要添加新的数学工具函数 |
| `Lucky/Source/Lucky/Math/Math.cpp` | 实现新的数学工具函数 |

---

## 3. Reparent 时保持 World Transform 不变

### 3.1 目标

当调用 `Entity::SetParent(newParent)` 时，实体在世界空间中的位置、旋转、缩放应保持不变。系统需要自动反算出新的 Local Transform。

### 3.2 数学原理

```
当前状态：
    WorldMatrix = OldParent.WorldMatrix × OldLocal

目标：
    WorldMatrix = NewParent.WorldMatrix × NewLocal  （WorldMatrix 不变）

求解：
    NewLocal = inverse(NewParent.WorldMatrix) × WorldMatrix
```

然后将 `NewLocal` 矩阵分解为 Translation、Rotation、Scale，写回 TransformComponent。

### 3.3 方案选择：SetParent 行为策略

#### 方案 A：默认保持 World Transform 不变（? 推荐，最优）

**做法**：`SetParent()` 默认行为是保持 World Transform 不变，自动反算 Local Transform。

```cpp
void Entity::SetParent(Entity newParent)
{
    Entity currentParent = GetParent();
    if (currentParent == newParent)
    {
        return;
    }

    // 保存当前的 World Matrix
    auto& transform = GetComponent<TransformComponent>();
    glm::mat4 currentWorldMatrix = transform.WorldMatrix;

    // --- 维护父子关系（原有逻辑）---
    if (currentParent)
    {
        currentParent.RemoveChild(*this);
    }

    SetParentUUID(newParent ? newParent.GetUUID() : UUID(0));

    if (newParent)
    {
        std::vector<UUID>& parentChildren = newParent.GetChildren();
        UUID id = GetUUID();
        if (std::find(parentChildren.begin(), parentChildren.end(), id) == parentChildren.end())
        {
            parentChildren.emplace_back(GetUUID());
        }
    }

    // --- 反算新的 Local Transform ---
    glm::mat4 newParentWorldMatrix = newParent
        ? newParent.GetComponent<TransformComponent>().WorldMatrix
        : glm::mat4(1.0f);

    glm::mat4 newLocalMatrix = glm::inverse(newParentWorldMatrix) * currentWorldMatrix;
    transform.SetLocalTransform(newLocalMatrix);
}
```

**优点**：
- 与 Unity 的默认行为一致
- 用户体验最好：拖拽实体到新父节点下时，实体不会"跳动"
- 最符合直觉

**缺点**：
- 涉及矩阵求逆和分解，有浮点精度损失（但通常可忽略）
- 非均匀缩放 + 旋转的组合可能导致分解不精确（存在 Shear）

#### 方案 B：提供参数控制是否保持 World Transform（其次）

**做法**：为 `SetParent()` 添加一个 `bool keepWorldTransform = true` 参数。

```cpp
void Entity::SetParent(Entity newParent, bool keepWorldTransform = true);
```

- `keepWorldTransform = true`：保持 World Transform 不变（方案 A 的行为）
- `keepWorldTransform = false`：保持 Local Transform 不变（实体会"跳"到新父节点的坐标系下）

**优点**：
- 灵活性最高，两种行为都支持
- 某些场景下确实需要保持 Local Transform（如从模板实例化子物体）

**缺点**：
- API 稍复杂
- 需要在所有调用处考虑使用哪种模式

#### 方案 C：默认保持 Local Transform 不变（不推荐）

**做法**：`SetParent()` 不做任何 Transform 调整，Local Transform 保持原值。

**优点**：
- 实现最简单，不需要任何额外计算

**缺点**：
- 用户体验差：拖拽实体到新父节点下时，实体会"跳动"到新位置
- 与 Unity 行为不一致
- 不推荐

### 3.4 Reparent 时的特殊情况处理

#### 3.4.1 设置父节点为空（变为根节点）

```cpp
// newParent 为空时，newParentWorldMatrix 为单位矩阵
glm::mat4 newParentWorldMatrix = newParent
    ? newParent.GetComponent<TransformComponent>().WorldMatrix
    : glm::mat4(1.0f);
```

此时 `NewLocal = inverse(I) × WorldMatrix = WorldMatrix`，即 Local Transform 变为当前的 World Transform。

#### 3.4.2 循环引用检测

在设置父节点时，需要检测是否会形成循环引用（A 是 B 的父节点，又试图将 A 设为 B 的子节点）。

**方案 A：在 SetParent 中添加循环检测（? 推荐）**

```cpp
bool Entity::IsDescendantOf(Entity ancestor) const
{
    Entity current = GetParent();
    while (current)
    {
        if (current == ancestor)
        {
            return true;
        }
        current = current.GetParent();
    }
    return false;
}

void Entity::SetParent(Entity newParent)
{
    // 循环引用检测
    if (newParent && newParent.IsDescendantOf(*this))
    {
        LF_CORE_WARN("Cannot set parent: would create a cycle!");
        return;
    }
    
    // ... 其余逻辑 ...
}
```

**方案 B：不检测，依赖调用者保证（不推荐）**

不做检测，如果形成循环，`UpdateTransformHierarchy` 会无限递归导致栈溢出。

---

## 4. World Transform 访问接口

### 4.1 目标

提供便捷的方法来获取和设置实体在世界空间中的 Position、Rotation、Scale。

### 4.2 方案选择：接口放置位置

#### 方案 A：在 Entity 类上提供接口（? 推荐，最优）

**做法**：在 `Entity` 类上添加 `GetWorldPosition()`、`SetWorldPosition()` 等方法。

```cpp
// Entity.h
class Entity
{
public:
    // ... 现有代码 ...

    // ==================== World Transform 访问接口 ====================
    
    /// <summary>
    /// 获取世界空间位置
    /// </summary>
    glm::vec3 GetWorldPosition() const;
    
    /// <summary>
    /// 获取世界空间旋转（四元数）
    /// </summary>
    glm::quat GetWorldRotation() const;
    
    /// <summary>
    /// 获取世界空间缩放
    /// </summary>
    glm::vec3 GetWorldScale() const;
    
    /// <summary>
    /// 设置世界空间位置（自动反算 Local Position）
    /// </summary>
    void SetWorldPosition(const glm::vec3& worldPosition);
    
    /// <summary>
    /// 设置世界空间旋转（自动反算 Local Rotation）
    /// </summary>
    void SetWorldRotation(const glm::quat& worldRotation);
    
    /// <summary>
    /// 设置世界空间缩放（自动反算 Local Scale）
    /// </summary>
    void SetWorldScale(const glm::vec3& worldScale);
};
```

**优点**：
- 语义清晰：World Transform 是实体在场景中的属性，放在 Entity 上合理
- Entity 可以访问父节点信息，方便反算
- 与 Unity 的 `transform.position` / `transform.localPosition` 设计一致

**缺点**：
- Entity 类的职责增加

#### 方案 B：在 TransformComponent 上提供接口（其次）

**做法**：在 `TransformComponent` 上添加 World 访问方法，但需要传入父节点的 WorldMatrix。

```cpp
struct TransformComponent
{
    // Getter：从缓存的 WorldMatrix 中分解
    glm::vec3 GetWorldPosition() const;
    glm::quat GetWorldRotation() const;
    glm::vec3 GetWorldScale() const;
    
    // Setter：需要传入父节点的 WorldMatrix
    void SetWorldPosition(const glm::vec3& worldPos, const glm::mat4& parentWorldMatrix);
    void SetWorldRotation(const glm::quat& worldRot, const glm::mat4& parentWorldMatrix);
};
```

**优点**：
- 符合 ECS 的"数据在组件上"原则

**缺点**：
- Setter 需要额外传入 `parentWorldMatrix` 参数，调用不方便
- TransformComponent 作为纯数据结构，不应承担太多逻辑

### 4.3 World Transform Getter 实现

#### 4.3.1 GetWorldPosition

世界位置可以直接从 WorldMatrix 的第 4 列提取：

```cpp
glm::vec3 Entity::GetWorldPosition() const
{
    const auto& transform = GetComponent<TransformComponent>();
    return glm::vec3(transform.WorldMatrix[3]);  // 第 4 列的 xyz
}
```

> **说明**：glm 使用列主序，`WorldMatrix[3]` 是第 4 列，即平移分量。

#### 4.3.2 GetWorldRotation 和 GetWorldScale

需要从 WorldMatrix 中分解出旋转和缩放：

```cpp
glm::quat Entity::GetWorldRotation() const
{
    const auto& transform = GetComponent<TransformComponent>();
    glm::vec3 translation, scale;
    glm::quat rotation;
    Math::DecomposeTransform(transform.WorldMatrix, translation, rotation, scale);
    return rotation;
}

glm::vec3 Entity::GetWorldScale() const
{
    const auto& transform = GetComponent<TransformComponent>();
    glm::vec3 translation, scale;
    glm::quat rotation;
    Math::DecomposeTransform(transform.WorldMatrix, translation, rotation, scale);
    return scale;
}
```

#### 4.3.3 优化方案：合并分解（可选）

如果频繁同时需要 Rotation 和 Scale，可以提供一个一次性分解的方法：

```cpp
void Entity::GetWorldTransformDecomposed(glm::vec3& outPosition, glm::quat& outRotation, glm::vec3& outScale) const
{
    const auto& transform = GetComponent<TransformComponent>();
    Math::DecomposeTransform(transform.WorldMatrix, outPosition, outRotation, outScale);
}
```

### 4.4 World Transform Setter 实现

#### 4.4.1 SetWorldPosition

**数学原理**：

```
目标：让实体的世界位置变为 targetWorldPos
当前：WorldMatrix = ParentWorld × LocalMatrix

方法：
1. 计算当前世界位置与目标的差值（在世界空间）
2. 将差值转换到父节点的局部空间
3. 加到 Local Translation 上
```

**实现方式 A：通过矩阵逆变换（? 推荐）**

```cpp
void Entity::SetWorldPosition(const glm::vec3& worldPosition)
{
    auto& transform = GetComponent<TransformComponent>();
    
    Entity parent = GetParent();
    if (parent)
    {
        // 将世界位置转换到父节点的局部空间
        glm::mat4 parentWorldInverse = glm::inverse(parent.GetComponent<TransformComponent>().WorldMatrix);
        glm::vec3 localPosition = glm::vec3(parentWorldInverse * glm::vec4(worldPosition, 1.0f));
        transform.Translation = localPosition;
    }
    else
    {
        // 根节点：World Position == Local Position
        transform.Translation = worldPosition;
    }
}
```

**实现方式 B：通过差值计算（其次）**

```cpp
void Entity::SetWorldPosition(const glm::vec3& worldPosition)
{
    glm::vec3 currentWorldPos = GetWorldPosition();
    glm::vec3 delta = worldPosition - currentWorldPos;
    
    Entity parent = GetParent();
    if (parent)
    {
        // 将世界空间的差值转换到父节点的局部空间
        glm::mat4 parentWorldInverse = glm::inverse(parent.GetComponent<TransformComponent>().WorldMatrix);
        glm::vec3 localDelta = glm::vec3(parentWorldInverse * glm::vec4(delta, 0.0f));  // w=0 表示方向
        transform.Translation += localDelta;
    }
    else
    {
        transform.Translation += delta;
    }
}
```

> 方式 A 更直接准确，推荐使用。

#### 4.4.2 SetWorldRotation

```cpp
void Entity::SetWorldRotation(const glm::quat& worldRotation)
{
    auto& transform = GetComponent<TransformComponent>();
    
    Entity parent = GetParent();
    if (parent)
    {
        // LocalRotation = inverse(ParentWorldRotation) × WorldRotation
        glm::quat parentWorldRotation = parent.GetWorldRotation();
        glm::quat localRotation = glm::inverse(parentWorldRotation) * worldRotation;
        transform.SetRotation(localRotation);
    }
    else
    {
        transform.SetRotation(worldRotation);
    }
}
```

#### 4.4.3 SetWorldScale

**注意**：世界缩放的反算在存在非均匀缩放 + 旋转时可能不精确。

```cpp
void Entity::SetWorldScale(const glm::vec3& worldScale)
{
    auto& transform = GetComponent<TransformComponent>();
    
    Entity parent = GetParent();
    if (parent)
    {
        glm::vec3 parentWorldScale = parent.GetWorldScale();
        
        // 避免除以零
        glm::vec3 localScale;
        localScale.x = (glm::abs(parentWorldScale.x) > glm::epsilon<float>()) 
            ? worldScale.x / parentWorldScale.x : worldScale.x;
        localScale.y = (glm::abs(parentWorldScale.y) > glm::epsilon<float>()) 
            ? worldScale.y / parentWorldScale.y : worldScale.y;
        localScale.z = (glm::abs(parentWorldScale.z) > glm::epsilon<float>()) 
            ? worldScale.z / parentWorldScale.z : worldScale.z;
        
        transform.Scale = localScale;
    }
    else
    {
        transform.Scale = worldScale;
    }
}
```

> **重要警告**：当父节点存在非均匀缩放且子节点有旋转时，`WorldScale = ParentScale × LocalScale` 的简单关系不成立。此时 WorldMatrix 中会包含 Shear（剪切）分量，无法精确分解为独立的 Rotation 和 Scale。这是 3D 引擎的通用限制，Unity 也存在同样的问题。

---

## 5. Math 工具函数扩展

### 5.1 需要确认的现有函数

当前 `Math::DecomposeTransform` 已经可以从 `mat4` 中分解出 `translation`、`rotation`（四元数）、`scale`。Phase 2 的实现依赖此函数，无需额外添加数学工具函数。

### 5.2 可选的辅助函数

如果需要，可以在 `Math` 命名空间中添加：

```cpp
namespace Math
{
    // 已有
    bool DecomposeTransform(const glm::mat4& transform, glm::vec3& translation, glm::quat& rotation, glm::vec3& scale);
    
    // 可选新增：从矩阵中仅提取平移分量（比完整分解更高效）
    inline glm::vec3 GetTranslation(const glm::mat4& matrix)
    {
        return glm::vec3(matrix[3]);
    }
    
    // 可选新增：从矩阵中仅提取缩放分量
    inline glm::vec3 GetScale(const glm::mat4& matrix)
    {
        return glm::vec3(
            glm::length(glm::vec3(matrix[0])),
            glm::length(glm::vec3(matrix[1])),
            glm::length(glm::vec3(matrix[2]))
        );
    }
}
```

---

## 6. 验证清单

### 6.1 Reparent 验证

| # | 测试场景 | 预期结果 |
|---|---------|---------|
| 1 | 实体 A 在世界位置 (3,0,0)，将 A 设为 B（位于 (1,0,0)）的子节点 | A 的世界位置仍为 (3,0,0)，A 的 Local Position 变为 (2,0,0) |
| 2 | 实体 A 是 B 的子节点，将 A 从 B 下移出（设为根节点） | A 的世界位置不变，A 的 Local Position 变为之前的 World Position |
| 3 | 实体 A 是 B 的子节点，将 A 移到 C 下 | A 的世界位置不变，Local Position 自动调整 |
| 4 | 父节点 B 有旋转，将 A 设为 B 的子节点 | A 的世界位置和旋转不变 |
| 5 | 父节点 B 有缩放 (2,2,2)，将 A 设为 B 的子节点 | A 的世界位置和缩放不变，Local Scale 自动调整 |

### 6.2 循环引用检测验证

| # | 测试场景 | 预期结果 |
|---|---------|---------|
| 6 | A 是 B 的父节点，尝试将 A 设为 B 的子节点 | 操作被拒绝，输出警告日志 |
| 7 | A → B → C，尝试将 A 设为 C 的子节点 | 操作被拒绝，输出警告日志 |
| 8 | 尝试将 A 设为自己的子节点 | 操作被拒绝 |

### 6.3 World Transform 访问接口验证

| # | 测试场景 | 预期结果 |
|---|---------|---------|
| 9 | 根节点 A 在 (3,0,0)，调用 `GetWorldPosition()` | 返回 (3,0,0) |
| 10 | A（(2,0,0)）→ B（Local (1,0,0)），对 B 调用 `GetWorldPosition()` | 返回 (3,0,0) |
| 11 | 对测试 10 中的 B 调用 `SetWorldPosition((5,0,0))` | B 的 Local Position 变为 (3,0,0)，World Position 为 (5,0,0) |
| 12 | 根节点调用 `SetWorldPosition` | 等价于直接设置 Translation |
| 13 | 父节点有旋转，对子节点调用 `SetWorldPosition` | Local Position 正确反算，考虑了父节点的旋转 |

---

## 7. 注意事项

### 7.1 WorldMatrix 的时效性

`SetWorldPosition()` 等 Setter 方法修改的是 Local Transform。修改后，当前帧的 `WorldMatrix` 缓存可能已过时（因为 `UpdateTransformHierarchy()` 在帧开始时已经执行过了）。

**处理策略**：

| 策略 | 说明 | 推荐度 |
|------|------|--------|
| **不处理，下一帧自动更新** | WorldMatrix 会在下一帧的 `UpdateTransformHierarchy()` 中更新 | ? 推荐（Phase 2 阶段足够） |
| **Setter 后立即更新当前实体及子树的 WorldMatrix** | 调用 `UpdateWorldTransform(entity, parentWorldMatrix)` | 其次（如果需要同帧内读取更新后的 WorldMatrix） |
| **标记 dirty，惰性更新** | Phase 3 的内容 | 后续实现 |

### 7.2 非均匀缩放的限制

当父节点存在非均匀缩放（如 Scale = (2, 1, 3)）且子节点有旋转时：
- `WorldMatrix` 中会包含 Shear（剪切）分量
- `DecomposeTransform` 分解出的 Rotation 和 Scale 可能不精确
- 这是所有 3D 引擎的通用限制

**建议**：在文档或编辑器中提示用户避免在有旋转子节点的父节点上使用非均匀缩放。

### 7.3 Reparent 中 WorldMatrix 的来源

在 `SetParent()` 中读取 `transform.WorldMatrix` 时，这个值是上一次 `UpdateTransformHierarchy()` 计算的结果。如果在同一帧内先修改了 Local Transform 再 Reparent，WorldMatrix 可能不是最新的。

**建议**：在 Reparent 前，可以手动重新计算当前的 WorldMatrix：

```cpp
// 确保使用最新的 WorldMatrix
glm::mat4 currentWorldMatrix = parentWorldMatrix * transform.GetLocalTransform();
// 其中 parentWorldMatrix 来自当前父节点（Reparent 前的父节点）
```

或者更简单地，直接使用缓存值（绝大多数情况下是准确的）。

---

## 8. 实现步骤总结

按以下顺序实现：

1. **修改 `Entity.h`**
   - 添加 `IsDescendantOf()` 方法声明
   - 添加 World Transform Getter/Setter 方法声明

2. **修改 `Entity.cpp`**
   - 实现 `IsDescendantOf()`
   - 修改 `SetParent()` 添加循环检测和 World Transform 保持逻辑
   - 实现 `GetWorldPosition()`、`GetWorldRotation()`、`GetWorldScale()`
   - 实现 `SetWorldPosition()`、`SetWorldRotation()`、`SetWorldScale()`

3. **（可选）扩展 `Math.h` / `Math.cpp`**
   - 添加 `GetTranslation()`、`GetScale()` 等辅助函数

4. **编译并测试**
   - 按照第 6 节的验证清单逐项测试
