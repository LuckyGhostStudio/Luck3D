
# Transform 层级系统设计文档

> **目标**：实现类似 Unity/Unreal 的 Transform 层级传递机制，使父节点的变换（平移、旋转、缩放）能够正确影响所有子节点。

---

## 目录

1. [问题分析](#1-问题分析)
2. [方案选型](#2-方案选型)
3. [详细设计](#3-详细设计)
4. [修改清单](#4-修改清单)
5. [实现步骤](#5-实现步骤)
6. [测试验证](#6-测试验证)

---

## 1. 问题分析

### 1.1 当前架构

当前 ECS 系统中：

- `RelationshipComponent` 已正确实现父子关系（`Parent` UUID + `Children` UUID 列表）
- `TransformComponent` 存储 `Translation`、`Rotation`（四元数）、`RotationEuler`（欧拉角）、`Scale`
- `GetTransform()` 方法直接返回由 TRS 组合的**局部矩阵**
- 渲染时（`Scene::OnUpdate`）直接使用 `transform.GetTransform()` 作为世界矩阵传给渲染器

### 1.2 存在的问题

| 问题 | 位置 | 描述 |
|------|------|------|
| 渲染不考虑层级 | `Scene.cpp` L212 | `Renderer3D::DrawMesh(transform.GetTransform(), ...)` 直接使用局部矩阵作为世界矩阵 |
| 光源位置/方向不考虑层级 | `Scene.cpp` L100-160 | `transform.Translation`、`transform.GetForward()` 直接使用局部值 |
| Gizmo 操作不正确 | `SceneViewportPanel.cpp` L308-374 | Gizmo 使用局部矩阵定位，操作结果直接写回局部属性，有父节点时行为错误 |
| 灯光 Gizmo 不考虑层级 | `SceneViewportPanel.cpp` L136-155 | `transform.Translation`、`transform.GetForward()` 直接使用局部值绘制 Gizmo |

### 1.3 设计目标

1. **TransformComponent 存储的始终是 Local Transform**（与 Unity 行为一致）
2. **提供获取 World Transform 的能力**（世界矩阵、世界位置、世界方向等）
3. **Inspector 面板显示/编辑 Local Transform**（与 Unity 行为一致）
4. **Gizmo 在世界空间操作，结果正确转回局部空间**
5. **渲染和光源使用世界矩阵**
6. **序列化只保存 Local Transform**（WorldTransform 是运行时计算的）

---

## 2. 方案选型

### 2.1 方案 A：实时递归计算

**描述**：每次需要世界矩阵时，递归向上遍历父链实时计算。

```cpp
glm::mat4 GetWorldTransform(Entity entity)
{
    glm::mat4 local = entity.GetComponent<TransformComponent>().GetLocalTransform();
    Entity parent = entity.GetParent();
    if (parent)
    {
        return GetWorldTransform(parent) * local;
    }
    return local;
}
```

| 维度 | 评价 |
|------|------|
| 优点 | 实现最简单；无缓存一致性问题；无额外内存开销 |
| 缺点 | 深层级时性能差（O(depth) 每次调用）；同一帧内多次访问同一实体会重复计算；渲染时每个实体都要遍历父链 |
| 适用场景 | 层级很浅（1-2层）、实体数量极少的原型项目 |

### 2.2 方案 B：脏标记 + 延迟计算（Dirty Flag）

**描述**：缓存世界矩阵，Transform 变化时标记脏，访问时按需重算。

```cpp
struct TransformComponent
{
    // ... Local 属性 ...
    glm::mat4 WorldTransform = glm::mat4(1.0f);
    bool Dirty = true;
    
    void MarkDirty(); // 标记自身和所有子孙为脏
};
```

| 维度 | 评价 |
|------|------|
| 优点 | 性能最优（只在变化时重算）；适合大规模场景 |
| 缺点 | 实现复杂；需要在所有修改 Transform 的地方触发脏标记；脏标记需要向下传播到所有子孙；需要处理访问顺序问题 |
| 适用场景 | 大规模场景、高性能要求的成熟引擎 |

### 2.3 方案 C：每帧统一更新（推荐）

**描述**：每帧开始时，按层级顺序（从根到叶）统一计算所有实体的世界矩阵。

```cpp
void Scene::UpdateTransformHierarchy()
{
    // 遍历所有根节点，递归更新子树
    for (root : rootEntities)
    {
        UpdateWorldTransformRecursive(root, glm::mat4(1.0f));
    }
}
```

| 维度 | 评价 |
|------|------|
| 优点 | 实现清晰，不易出 bug；性能可预测（O(N) 每帧）；无需管理脏标记传播；代码侵入性低 |
| 缺点 | 即使未变化也会重算（可通过后续优化加入脏标记）；帧内修改 Transform 后需要等到下一帧才能看到更新的世界矩阵（或手动触发重算） |
| 适用场景 | 中小规模场景、开发阶段的引擎 |

### 2.4 方案选择

**推荐方案 C（每帧统一更新）**

推荐原因：
1. 当前项目处于开发阶段，场景规模不大，O(N) 的开销完全可接受
2. 实现简单清晰，不容易引入 bug
3. 代码侵入性最低，只需在 `OnUpdate` 开头加一次调用
4. 后续可平滑升级为方案 B（加入脏标记优化），无需改变外部接口
5. Unity 内部的 TransformHierarchy 系统也是类似的批量更新思路

**后续优化路径**：方案 C → 方案 C + 脏标记（混合方案）→ 方案 B（完全按需计算）

---

## 3. 详细设计

### 3.1 TransformComponent 修改

#### 3.1.1 新增成员

```cpp
struct TransformComponent
{
    // ======== 现有 Local 属性（不变） ========
    glm::vec3 Translation = { 0.0f, 0.0f, 0.0f };
    glm::vec3 Scale = { 1.0f, 1.0f, 1.0f };
private:
    glm::vec3 RotationEuler = { 0.0f, 0.0f, 0.0f };
    glm::quat Rotation = { 1.0f, 0.0f, 0.0f, 0.0f };

    // ======== 新增：缓存的世界变换矩阵 ========
    // 由 Scene::UpdateTransformHierarchy() 每帧更新
    // 不参与序列化
    glm::mat4 WorldTransform = glm::mat4(1.0f);

    // 声明 Scene 为友元，允许 Scene 直接写入 WorldTransform
    friend class Scene;

public:
    // ...
};
```

#### 3.1.2 方法重命名与新增

| 原方法 | 新方法 | 说明 |
|--------|--------|------|
| `GetTransform()` | `GetLocalTransform()` | 返回局部变换矩阵（TRS 组合） |
| - | `GetWorldTransform()` | 返回缓存的世界变换矩阵 |
| - | `GetWorldPosition()` | 从世界矩阵提取世界位置 |
| - | `GetWorldRotation()` | 从世界矩阵提取世界旋转（四元数） |
| - | `GetWorldForward()` | 获取世界空间前方向 |
| - | `GetWorldUp()` | 获取世界空间上方向 |
| - | `GetWorldRight()` | 获取世界空间右方向 |
| `GetForward()` | `GetLocalForward()` | 获取局部空间前方向（原 `GetForward()`） |
| `GetUp()` | `GetLocalUp()` | 获取局部空间上方向（原 `GetUp()`） |
| `GetRight()` | `GetLocalRight()` | 获取局部空间右方向（原 `GetRight()`） |

> **关于 `GetTransform()` 的处理方式有两个选项：**

**选项 A（推荐）：保留 `GetTransform()` 作为 `GetLocalTransform()` 的别名**

```cpp
// 保持向后兼容，但标记为 [[deprecated]]
[[deprecated("Use GetLocalTransform() or GetWorldTransform() instead")]]
glm::mat4 GetTransform() const { return GetLocalTransform(); }
```

优点：不会破坏现有代码编译，可以渐进式迁移
缺点：可能导致遗漏修改点

**选项 B：直接删除 `GetTransform()`，强制使用新接口**

优点：编译器会报错所有使用点，确保不遗漏
缺点：需要一次性修改所有调用点

**推荐选项 B**：因为当前项目调用点不多（仅 `Scene.cpp` 和 `SceneViewportPanel.cpp`），直接删除可以确保所有使用点都被正确更新。

#### 3.1.3 完整的 TransformComponent 新接口

```cpp
struct TransformComponent
{
    glm::vec3 Translation = { 0.0f, 0.0f, 0.0f };
    glm::vec3 Scale = { 1.0f, 1.0f, 1.0f };
private:
    glm::vec3 RotationEuler = { 0.0f, 0.0f, 0.0f };
    glm::quat Rotation = { 1.0f, 0.0f, 0.0f, 0.0f };
    glm::mat4 WorldTransform = glm::mat4(1.0f);

    friend class Scene;

public:
    TransformComponent() = default;
    TransformComponent(const TransformComponent& other) = default;
    TransformComponent(const glm::vec3& translation)
        : Translation(translation)
    {
    }

    // ======== Local Transform ========

    /// <summary>
    /// 获取局部变换矩阵（Translation * Rotation * Scale）
    /// </summary>
    glm::mat4 GetLocalTransform() const
    {
        return glm::translate(glm::mat4(1.0f), Translation)
            * glm::toMat4(Rotation)
            * glm::scale(glm::mat4(1.0f), Scale);
    }

    /// <summary>
    /// 设置局部变换矩阵（分解为 Translation、Rotation、Scale）
    /// </summary>
    void SetLocalTransform(const glm::mat4& transform)
    {
        Math::DecomposeTransform(transform, Translation, Rotation, Scale);
        RotationEuler = glm::eulerAngles(Rotation);
    }

    // ======== World Transform ========

    /// <summary>
    /// 获取世界变换矩阵（由 Scene::UpdateTransformHierarchy() 每帧更新）
    /// </summary>
    const glm::mat4& GetWorldTransform() const { return WorldTransform; }

    /// <summary>
    /// 获取世界位置
    /// </summary>
    glm::vec3 GetWorldPosition() const
    {
        return glm::vec3(WorldTransform[3]);
    }

    /// <summary>
    /// 获取世界旋转（四元数）
    /// </summary>
    glm::quat GetWorldRotation() const
    {
        glm::vec3 worldTranslation;
        glm::quat worldRotation;
        glm::vec3 worldScale;
        Math::DecomposeTransform(WorldTransform, worldTranslation, worldRotation, worldScale);
        return worldRotation;
    }

    /// <summary>
    /// 获取世界前方向（局部 +Z 轴经世界旋转后的方向）
    /// </summary>
    glm::vec3 GetWorldForward() const
    {
        return glm::normalize(glm::vec3(WorldTransform * glm::vec4(0.0f, 0.0f, 1.0f, 0.0f)));
    }

    /// <summary>
    /// 获取世界上方向（局部 +Y 轴经世界旋转后的方向）
    /// </summary>
    glm::vec3 GetWorldUp() const
    {
        return glm::normalize(glm::vec3(WorldTransform * glm::vec4(0.0f, 1.0f, 0.0f, 0.0f)));
    }

    /// <summary>
    /// 获取世界右方向（局部 +X 轴经世界旋转后的方向）
    /// </summary>
    glm::vec3 GetWorldRight() const
    {
        return glm::normalize(glm::vec3(WorldTransform * glm::vec4(1.0f, 0.0f, 0.0f, 0.0f)));
    }

    // ======== Local 方向（保留原有功能，重命名） ========

    /// <summary>
    /// 获取局部前方向（局部 +Z 轴经局部旋转后的方向）
    /// </summary>
    glm::vec3 GetLocalForward() const
    {
        return Rotation * glm::vec3(0.0f, 0.0f, 1.0f);
    }

    /// <summary>
    /// 获取局部上方向（局部 +Y 轴经局部旋转后的方向）
    /// </summary>
    glm::vec3 GetLocalUp() const
    {
        return Rotation * glm::vec3(0.0f, 1.0f, 0.0f);
    }

    /// <summary>
    /// 获取局部右方向（局部 +X 轴经局部旋转后的方向）
    /// </summary>
    glm::vec3 GetLocalRight() const
    {
        return Rotation * glm::vec3(1.0f, 0.0f, 0.0f);
    }

    // ======== Rotation 相关（不变） ========

    glm::vec3 GetRotationEuler() const { return RotationEuler; }
    void SetRotationEuler(const glm::vec3& euler) { /* 原逻辑不变 */ }
    glm::quat GetRotation() const { return Rotation; }
    void SetRotation(const glm::quat& quat) { /* 原逻辑不变 */ }
};
```

### 3.2 Scene 层级更新系统

#### 3.2.1 Scene.h 新增声明

```cpp
class Scene
{
public:
    // ... 现有接口 ...

    /// <summary>
    /// 更新 Transform 层级：从根节点递归计算所有实体的世界变换矩阵
    /// 每帧在 OnUpdate 开头调用
    /// </summary>
    void UpdateTransformHierarchy();

private:
    /// <summary>
    /// 递归更新实体及其子树的世界变换矩阵
    /// </summary>
    /// <param name="entity">当前实体</param>
    /// <param name="parentWorldTransform">父节点的世界变换矩阵</param>
    void UpdateWorldTransformRecursive(Entity entity, const glm::mat4& parentWorldTransform);
};
```

#### 3.2.2 Scene.cpp 实现

```cpp
void Scene::UpdateTransformHierarchy()
{
    auto view = m_Registry.view<TransformComponent, RelationshipComponent>();
    for (auto entityID : view)
    {
        auto& relationship = view.get<RelationshipComponent>(entityID);

        // 只处理根节点（没有父节点的实体）
        if (relationship.Parent == 0)
        {
            UpdateWorldTransformRecursive(Entity{ entityID, this }, glm::mat4(1.0f));
        }
    }
}

void Scene::UpdateWorldTransformRecursive(Entity entity, const glm::mat4& parentWorldTransform)
{
    auto& transform = entity.GetComponent<TransformComponent>();

    // 计算世界矩阵 = 父世界矩阵 × 局部矩阵
    transform.WorldTransform = parentWorldTransform * transform.GetLocalTransform();

    // 递归更新所有子节点
    for (UUID childID : entity.GetChildren())
    {
        Entity child = TryGetEntityWithUUID(childID);
        if (child)
        {
            UpdateWorldTransformRecursive(child, transform.WorldTransform);
        }
    }
}
```

#### 3.2.3 OnUpdate 调用位置

```cpp
void Scene::OnUpdate(DeltaTime dt, EditorCamera& camera)
{
    // 【新增】每帧更新 Transform 层级
    UpdateTransformHierarchy();

    // 收集所有光源数据（使用世界矩阵）
    SceneLightData sceneLightData;
    // ...
}
```

### 3.3 渲染系统修改

#### 3.3.1 网格渲染

```cpp
// Scene.cpp - OnUpdate 中的网格渲染
// 修改前：
Renderer3D::DrawMesh(transform.GetTransform(), meshFilter.Mesh, meshRenderer.Materials, ...);

// 修改后：
Renderer3D::DrawMesh(transform.GetWorldTransform(), meshFilter.Mesh, meshRenderer.Materials, ...);
```

#### 3.3.2 光源数据收集

```cpp
// Scene.cpp - OnUpdate 中的光源收集
// 修改前：
dirLight.Direction = transform.GetForward();
pointLight.Position = transform.Translation;
spotLight.Position = transform.Translation;
spotLight.Direction = transform.GetForward();

// 修改后：
dirLight.Direction = transform.GetWorldForward();
pointLight.Position = transform.GetWorldPosition();
spotLight.Position = transform.GetWorldPosition();
spotLight.Direction = transform.GetWorldForward();
```

### 3.4 Gizmo 操作修改

#### 3.4.1 ImGuizmo 操作（SceneViewportPanel.cpp）

Gizmo 需要在世界空间操作，但结果需要正确转回局部空间。

**核心逻辑变更：**

```cpp
void SceneViewportPanel::UI_DrawGizmos()
{
    UI_DrawViewOrientationGizmo();

    UUID selectionID = SelectionManager::GetSelection();
    if (selectionID != 0 && m_GizmoType != -1)
    {
        Entity entity = m_Scene->GetEntityWithUUID(selectionID);
        TransformComponent& transformComponent = entity.GetComponent<TransformComponent>();

        // 【修改】使用世界矩阵给 Gizmo 定位
        glm::mat4 transform = transformComponent.GetWorldTransform();

        ImGuizmo::SetOrthographic(false);
        ImGuizmo::AllowAxisFlip(false);
        ImGuizmo::SetDrawlist();
        ImGuizmo::SetRect(m_Bounds[0].x, m_Bounds[0].y,
                          m_Bounds[1].x - m_Bounds[0].x, m_Bounds[1].y - m_Bounds[0].y);

        glm::mat4 viewMatrix = m_EditorCamera.GetViewMatrix();
        glm::mat4 projectionMatrix = m_EditorCamera.GetProjectionMatrix();

        bool span = Input::IsKeyPressed(Key::LeftControl);
        float spanValue = 0.5f;
        if (m_GizmoType == ImGuizmo::OPERATION::ROTATE)
        {
            spanValue = 5.0f;
        }
        float spanValues[3] = { spanValue, spanValue, spanValue };

        ImGuizmo::Manipulate(
            glm::value_ptr(viewMatrix),
            glm::value_ptr(projectionMatrix),
            static_cast<ImGuizmo::OPERATION>(m_GizmoType),
            static_cast<ImGuizmo::MODE>(m_GizmoMode),
            glm::value_ptr(transform),
            nullptr,
            span ? spanValues : nullptr
        );

        if (ImGuizmo::IsUsing())
        {
            // 【修改】获取父节点的世界矩阵
            glm::mat4 parentWorldTransform = glm::mat4(1.0f);
            Entity parent = entity.GetParent();
            if (parent)
            {
                parentWorldTransform = parent.GetComponent<TransformComponent>().GetWorldTransform();
            }

            // 将 Gizmo 操作后的世界矩阵转回局部空间
            glm::mat4 newLocalTransform = glm::inverse(parentWorldTransform) * transform;

            glm::vec3 translation;
            glm::quat rotation;
            glm::vec3 scale;
            Math::DecomposeTransform(newLocalTransform, translation, rotation, scale);

            switch (m_GizmoType)
            {
                case ImGuizmo::OPERATION::TRANSLATE:
                    transformComponent.Translation = translation;
                    break;
                case ImGuizmo::OPERATION::ROTATE:
                    transformComponent.SetRotation(rotation);
                    break;
                case ImGuizmo::OPERATION::SCALE:
                    transformComponent.Scale = scale;
                    break;
            }
        }
    }
}
```

> **关于 Gizmo 旋转操作的方案选择：**

**选项 A（推荐）：直接使用 `SetRotation(quat)` 写入**

```cpp
case ImGuizmo::OPERATION::ROTATE:
    transformComponent.SetRotation(rotation);
    break;
```

优点：简单直接；`SetRotation()` 内部已有欧拉角最近值选择逻辑，能避免 180° 翻转
缺点：欧拉角显示可能在某些极端角度下有微小跳变

**选项 B：计算旋转增量后累积**

```cpp
case ImGuizmo::OPERATION::ROTATE:
{
    glm::quat oldLocalRotation = transformComponent.GetRotation();
    glm::quat deltaRotation = rotation * glm::inverse(oldLocalRotation);
    // ... 累积增量 ...
    break;
}
```

优点：可以支持超过 360° 的连续旋转
缺点：实现复杂；在世界→局部转换后再做增量计算容易引入误差

**推荐选项 A**：当前 `SetRotation()` 已经有完善的欧拉角稳定性处理逻辑，直接使用即可。

#### 3.4.2 灯光 Gizmo 绘制

```cpp
// SceneViewportPanel.cpp - 灯光 Gizmo 绘制
// 修改前：
GizmoRenderer::DrawDirectionalLightGizmo(transform.Translation, transform.GetForward(), light.Color);
GizmoRenderer::DrawPointLightGizmo(transform.Translation, light.Range, light.Color);
GizmoRenderer::DrawSpotLightGizmo(transform.Translation, transform.GetForward(), ...);

// 修改后：
GizmoRenderer::DrawDirectionalLightGizmo(transform.GetWorldPosition(), transform.GetWorldForward(), light.Color);
GizmoRenderer::DrawPointLightGizmo(transform.GetWorldPosition(), light.Range, light.Color);
GizmoRenderer::DrawSpotLightGizmo(transform.GetWorldPosition(), transform.GetWorldForward(), ...);
```

### 3.5 SetParent 时的 Transform 处理

#### 3.5.1 问题描述

当改变父子关系时（如拖拽实体到另一个父节点下），需要决定是否保持实体的**世界位置不变**。

#### 3.5.2 方案选择

**选项 A（推荐）：保持世界位置不变（Unity 默认行为）**

设置新父节点时，重新计算局部 Transform 使得世界位置/旋转/缩放保持不变。

```cpp
void Entity::SetParent(Entity parent)
{
    Entity currentParent = GetParent();
    if (currentParent == parent)
    {
        return;
    }

    auto& transform = GetComponent<TransformComponent>();

    // 保存当前世界矩阵
    glm::mat4 currentWorldTransform = transform.GetWorldTransform();

    // 从旧父节点移除
    if (currentParent)
    {
        currentParent.RemoveChild(*this);
    }

    // 设置新父节点
    SetParentUUID(parent ? parent.GetUUID() : UUID(0));

    if (parent)
    {
        std::vector<UUID>& parentChildren = parent.GetChildren();
        UUID id = GetUUID();
        if (std::find(parentChildren.begin(), parentChildren.end(), id) == parentChildren.end())
        {
            parentChildren.emplace_back(id);
        }

        // 计算新的局部 Transform 以保持世界位置不变
        glm::mat4 parentWorldTransform = parent.GetComponent<TransformComponent>().GetWorldTransform();
        glm::mat4 newLocalTransform = glm::inverse(parentWorldTransform) * currentWorldTransform;
        transform.SetLocalTransform(newLocalTransform);
    }
    else
    {
        // 无父节点：世界矩阵 = 局部矩阵
        transform.SetLocalTransform(currentWorldTransform);
    }
}
```

优点：符合 Unity 默认行为；用户拖拽时物体不会"跳动"
缺点：实现稍复杂；需要确保 `SetParent` 调用时世界矩阵已经是最新的

**选项 B：保持局部 Transform 不变**

设置新父节点时不修改局部 Transform，物体会"跳"到新的世界位置。

```cpp
void Entity::SetParent(Entity parent)
{
    // ... 只修改关系，不修改 Transform ...
}
```

优点：实现简单
缺点：不符合用户直觉；拖拽时物体会跳动

**推荐选项 A**：保持世界位置不变是主流引擎的标准行为。

### 3.6 Inspector 面板

**无需修改**。Inspector 显示/编辑的是 `Translation`、`RotationEuler`、`Scale`，这些都是局部属性，与 Unity 行为一致。

### 3.7 序列化

**无需修改**。`SceneSerializer` 序列化的是 `Translation`、`Rotation`（四元数）、`Scale`，这些都是局部属性。`WorldTransform` 是运行时计算的缓存值，不参与序列化。

---

## 4. 修改清单

### 4.1 引擎核心（Lucky/Source/Lucky/）

| 文件 | 修改类型 | 修改内容 |
|------|----------|----------|
| `Scene/Components/TransformComponent.h` | **重构** | 新增 `WorldTransform` 缓存；重命名 `GetTransform()` → `GetLocalTransform()`；新增 `GetWorldTransform()`、`GetWorldPosition()`、`GetWorldRotation()`、`GetWorldForward()`、`GetWorldUp()`、`GetWorldRight()`；重命名 `GetForward()` → `GetLocalForward()`（同理 Up/Right）；新增 `SetLocalTransform()`；声明 `friend class Scene` |
| `Scene/Scene.h` | **新增** | 声明 `UpdateTransformHierarchy()` 和 `UpdateWorldTransformRecursive()` |
| `Scene/Scene.cpp` | **修改** | 实现层级更新逻辑；`OnUpdate` 开头调用 `UpdateTransformHierarchy()`；光源收集使用 `GetWorldPosition()` / `GetWorldForward()`；网格渲染使用 `GetWorldTransform()` |
| `Scene/Entity.cpp` | **修改** | `SetParent()` 中保持世界位置不变的逻辑 |
| `Scene/Entity.h` | **可能修改** | 如果需要新增 `SetParent` 的重载（如带 `worldPositionStays` 参数） |

### 4.2 编辑器应用（Luck3DApp/Source/）

| 文件 | 修改类型 | 修改内容 |
|------|----------|----------|
| `Panels/SceneViewportPanel.cpp` | **修改** | Gizmo 使用 `GetWorldTransform()` 定位；操作结果通过逆父矩阵转回局部空间；灯光 Gizmo 使用 `GetWorldPosition()` / `GetWorldForward()` |
| `Panels/InspectorPanel.cpp` | **无需修改** | 已经是编辑局部属性 |
| `Panels/SceneHierarchyPanel.cpp` | **无需修改** | 只涉及关系操作，不涉及 Transform 值 |

### 4.3 不需要修改的文件

| 文件 | 原因 |
|------|------|
| `Serialization/SceneSerializer.cpp` | 序列化的是局部 Transform，无需改动 |
| `Renderer/Renderer3D.cpp` | 接收的是世界矩阵参数，内部逻辑不变 |
| `Renderer/GizmoRenderer.h/cpp` | 接收的是世界空间的 position/direction，内部逻辑不变 |

---

## 5. 实现步骤

### 步骤 1：修改 TransformComponent.h

1. 新增 `WorldTransform` 私有成员
2. 新增 `friend class Scene` 声明
3. 将 `GetTransform()` 重命名为 `GetLocalTransform()`
4. 将 `SetTransform()` 重命名为 `SetLocalTransform()`
5. 新增 `GetWorldTransform()`、`GetWorldPosition()`、`GetWorldRotation()`
6. 新增 `GetWorldForward()`、`GetWorldUp()`、`GetWorldRight()`
7. 将 `GetForward()` 重命名为 `GetLocalForward()`（同理 Up/Right）

### 步骤 2：修改 Scene.h / Scene.cpp

1. 在 `Scene.h` 中声明 `UpdateTransformHierarchy()` 和 `UpdateWorldTransformRecursive()`
2. 在 `Scene.cpp` 中实现这两个方法
3. 在 `OnUpdate()` 开头调用 `UpdateTransformHierarchy()`
4. 修改光源收集代码：使用 `GetWorldPosition()` / `GetWorldForward()`
5. 修改网格渲染代码：使用 `GetWorldTransform()`

### 步骤 3：修改 Entity.cpp

1. 修改 `SetParent()` 方法：在改变父子关系时保持世界位置不变

### 步骤 4：修改 SceneViewportPanel.cpp

1. Gizmo 定位使用 `GetWorldTransform()`
2. Gizmo 操作结果通过逆父矩阵转回局部空间
3. 灯光 Gizmo 使用 `GetWorldPosition()` / `GetWorldForward()`

### 步骤 5：编译修复

由于重命名了 `GetTransform()` → `GetLocalTransform()` 和 `GetForward()` → `GetLocalForward()`，编译器会报错所有旧调用点。逐一修复：

- `Scene.cpp` 中的 `transform.GetTransform()` → `transform.GetWorldTransform()`
- `Scene.cpp` 中的 `transform.GetForward()` → `transform.GetWorldForward()`
- `SceneViewportPanel.cpp` 中的 `transform.GetForward()` → `transform.GetWorldForward()`
- 其他可能的调用点

---

## 6. 测试验证

### 6.1 基本层级测试

1. 创建 Parent（Cube）和 Child（Cube），将 Child 设为 Parent 的子节点
2. 移动 Parent → 验证 Child 跟随移动
3. 旋转 Parent → 验证 Child 围绕 Parent 旋转
4. 缩放 Parent → 验证 Child 跟随缩放

### 6.2 多层级测试

1. 创建 A → B → C 三层层级
2. 移动 A → 验证 B 和 C 都跟随
3. 移动 B → 验证 C 跟随，A 不受影响

### 6.3 Gizmo 操作测试

1. 选中子节点，使用 Gizmo 平移 → 验证 Inspector 中 Position 正确更新（局部坐标）
2. 选中子节点，使用 Gizmo 旋转 → 验证 Inspector 中 Rotation 正确更新
3. 选中父节点，使用 Gizmo 操作 → 验证子节点跟随

### 6.4 SetParent 测试

1. 将一个有世界位置 (5, 0, 0) 的实体拖拽到位于 (3, 0, 0) 的父节点下
2. 验证实体视觉位置不变（仍在世界 (5, 0, 0)）
3. 验证 Inspector 中 Position 变为 (2, 0, 0)（相对于父节点的局部坐标）

### 6.5 光源层级测试

1. 创建一个空父节点，将 Point Light 设为其子节点
2. 移动父节点 → 验证光照效果跟随移动
3. 旋转父节点（含 Directional Light 子节点）→ 验证光照方向改变

### 6.6 序列化测试

1. 创建带层级的场景，保存
2. 重新加载 → 验证层级关系和 Transform 值正确恢复
3. 验证加载后世界矩阵正确计算（第一帧 `UpdateTransformHierarchy` 后）

---

## 附录 A：后续优化方向

### A.1 脏标记优化

当场景规模增大时，可以引入脏标记避免每帧重算所有实体：

```cpp
struct TransformComponent
{
    // ...
    bool Dirty = true;  // 标记是否需要重算世界矩阵
};
```

修改 Transform 时标记自身和所有子孙为脏：

```cpp
void MarkDirtyRecursive(Entity entity)
{
    entity.GetComponent<TransformComponent>().Dirty = true;
    for (UUID childID : entity.GetChildren())
    {
        MarkDirtyRecursive(GetEntityWithUUID(childID));
    }
}
```

### A.2 SetWorldPosition / SetWorldRotation

提供直接设置世界空间属性的便捷方法：

```cpp
void SetWorldPosition(const glm::vec3& worldPos, Entity parent)
{
    if (parent)
    {
        glm::mat4 invParent = glm::inverse(parent.GetComponent<TransformComponent>().GetWorldTransform());
        Translation = glm::vec3(invParent * glm::vec4(worldPos, 1.0f));
    }
    else
    {
        Translation = worldPos;
    }
}
```

### A.3 非均匀缩放处理

当父节点有非均匀缩放时，子节点的旋转可能产生剪切（Shear）。这是所有引擎都面临的问题，Unity 的处理方式是允许剪切存在于世界矩阵中，但 Inspector 显示的局部 TRS 始终是"干净"的。当前方案已经天然支持这一行为。

---

## 附录 B：与 Unity 行为对比

| 行为 | Unity | 本方案 |
|------|-------|--------|
| Inspector 显示 | Local Position/Rotation/Scale | Local Translation/RotationEuler/Scale |
| 世界矩阵计算 | TransformHierarchy 系统批量更新 | `UpdateTransformHierarchy()` 每帧批量更新 |
| Gizmo 操作空间 | 世界空间操作，结果写回局部 | 世界空间操作，结果通过逆父矩阵写回局部 |
| SetParent 行为 | 默认保持世界位置不变 | 保持世界位置不变 |
| 序列化 | 保存局部 Transform | 保存局部 Transform |
| 非均匀缩放 | 允许世界矩阵含剪切 | 允许世界矩阵含剪切 |
