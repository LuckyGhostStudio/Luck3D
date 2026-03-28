# Phase 3：脏标记优化 + Transform 变更通知

## 1. 概述

Phase 3 是 Transform 层级系统的性能优化层，目标是：
1. **脏标记（Dirty Flag）机制**：只有 Local Transform 发生变化的实体及其所有子孙才需要重新计算 WorldMatrix，避免每帧全量更新
2. **Transform 变更通知**：提供一种机制让其他系统（如物理、动画、音频等）能感知 Transform 的变化

### 前置依赖

- Phase 1 已完成（WorldMatrix 缓存 + 层级更新）
- Phase 2 已完成（Reparent + World Transform 访问接口）

### 何时需要 Phase 3

- 场景中实体数量超过 **5000+** 且大部分实体每帧不移动时，脏标记优化才有明显收益
- 需要物理系统、动画系统等与 Transform 联动时，需要变更通知机制
- 如果当前项目规模较小，可以延后实现

---

## 2. 涉及的文件

### 需要修改的文件

| 文件路径 | 说明 |
|---------|------|
| `Lucky/Source/Lucky/Scene/Components/TransformComponent.h` | 添加脏标记字段和 Transform 版本号 |
| `Lucky/Source/Lucky/Scene/Scene.h` | 修改层级更新方法 |
| `Lucky/Source/Lucky/Scene/Scene.cpp` | 实现基于脏标记的层级更新 |
| `Lucky/Source/Lucky/Scene/Entity.h` | 可能需要添加标记 dirty 的辅助方法 |
| `Lucky/Source/Lucky/Scene/Entity.cpp` | 实现标记 dirty 的逻辑 |

### 可能需要新建的文件

| 文件路径 | 说明 |
|---------|------|
| `Lucky/Source/Lucky/Scene/TransformSystem.h` | （可选）将 Transform 更新逻辑抽取为独立的 System 类 |
| `Lucky/Source/Lucky/Scene/TransformSystem.cpp` | （可选）TransformSystem 实现 |

---

## 3. 脏标记（Dirty Flag）机制

### 3.1 目标

当一个实体的 Local Transform 没有发生变化时，跳过其 WorldMatrix 的重新计算。当 Local Transform 发生变化时，标记该实体及其所有子孙为 "dirty"，在下次更新时重新计算。

### 3.2 核心概念

```
标记 Dirty 的时机：
    - Translation 被修改
    - Rotation 被修改
    - Scale 被修改
    - 父节点被标记为 Dirty（向下传播）

清除 Dirty 的时机：
    - WorldMatrix 被重新计算后
```

### 3.3 方案选择：脏标记的存储和传播方式

#### 方案 A：TransformComponent 内置 Dirty 标记 + 向下传播（? 推荐，最优）

**做法**：在 `TransformComponent` 中添加一个 `bool m_Dirty` 字段。当 Local Transform 被修改时设为 `true`。在 `UpdateTransformHierarchy` 中，如果父节点是 dirty 的，子节点也标记为 dirty。

**TransformComponent 修改**：

```cpp
struct TransformComponent
{
    // ... 现有字段 ...
    
private:
    bool m_Dirty = true;  // 初始为 true，确保首帧计算 WorldMatrix
    
public:
    bool IsDirty() const { return m_Dirty; }
    
    // 标记为脏（需要重新计算 WorldMatrix）
    void MarkDirty() { m_Dirty = true; }
    
    // 清除脏标记（WorldMatrix 已更新）
    void ClearDirty() { m_Dirty = false; }
};
```

**问题：如何在 Translation/Scale 被修改时自动标记 Dirty？**

当前 `Translation` 和 `Scale` 是 public 字段，外部可以直接修改（如 `transform.Translation.x = 5.0f`），无法自动触发 Dirty 标记。

**子方案 A-1：将 Translation/Scale 改为 private + Setter（? 最优，但改动大）**

```cpp
struct TransformComponent
{
private:
    glm::vec3 m_Translation = { 0.0f, 0.0f, 0.0f };
    glm::vec3 m_Scale = { 1.0f, 1.0f, 1.0f };
    bool m_Dirty = true;
    
public:
    const glm::vec3& GetTranslation() const { return m_Translation; }
    void SetTranslation(const glm::vec3& translation) 
    { 
        if (m_Translation != translation)
        {
            m_Translation = translation; 
            m_Dirty = true;
        }
    }
    
    const glm::vec3& GetScale() const { return m_Scale; }
    void SetScale(const glm::vec3& scale) 
    { 
        if (m_Scale != scale)
        {
            m_Scale = scale; 
            m_Dirty = true;
        }
    }
    
    // SetRotationEuler 和 SetRotation 中也添加 m_Dirty = true
};
```

**优点**：
- 完全自动化，不可能遗漏 Dirty 标记
- 封装性最好

**缺点**：
- 需要修改所有直接访问 `Translation` 和 `Scale` 的代码（如 InspectorPanel）
- 改动量较大
- 与现有的 public 字段风格不一致

**子方案 A-2：保持 public 字段 + 手动调用 MarkDirty()（其次，改动小）**

```cpp
struct TransformComponent
{
    // Translation 和 Scale 保持 public
    glm::vec3 Translation = { 0.0f, 0.0f, 0.0f };
    glm::vec3 Scale = { 1.0f, 1.0f, 1.0f };
    
private:
    bool m_Dirty = true;
    
public:
    bool IsDirty() const { return m_Dirty; }
    void MarkDirty() { m_Dirty = true; }
    void ClearDirty() { m_Dirty = false; }
};
```

调用方在修改 Translation/Scale 后需要手动调用 `MarkDirty()`：

```cpp
// InspectorPanel 中
if (ImGui::DragFloat3("Position", translation, 0.1f))
{
    transform.Translation = { translation[0], translation[1], translation[2] };
    transform.MarkDirty();  // 手动标记
}
```

**优点**：
- 改动最小，与现有代码风格兼容
- 简单直接

**缺点**：
- 容易遗漏 `MarkDirty()` 调用
- 如果忘记调用，WorldMatrix 不会更新，导致难以排查的 Bug

**子方案 A-3：每帧检测变化（折中方案）**

不使用显式的 Dirty 标记，而是在每帧更新时比较当前 Local Transform 与上一帧的缓存值：

```cpp
struct TransformComponent
{
    // ... 现有字段 ...
    
private:
    // 上一帧的 Local Transform 缓存（用于变化检测）
    glm::vec3 m_PrevTranslation = { 0.0f, 0.0f, 0.0f };
    glm::vec3 m_PrevScale = { 1.0f, 1.0f, 1.0f };
    glm::quat m_PrevRotation = { 1.0f, 0.0f, 0.0f, 0.0f };
    
public:
    bool HasChanged() const
    {
        return Translation != m_PrevTranslation 
            || Scale != m_PrevScale 
            || GetRotation() != m_PrevRotation;
    }
    
    void UpdatePrevState()
    {
        m_PrevTranslation = Translation;
        m_PrevScale = Scale;
        m_PrevRotation = GetRotation();
    }
};
```

**优点**：
- 不需要手动调用 MarkDirty()
- 不需要修改 Translation/Scale 的访问方式
- 自动检测变化

**缺点**：
- 额外的内存开销（每个实体多存储 10 个 float）
- 每帧需要比较所有实体的 Transform（但比矩阵乘法便宜）
- 浮点比较可能有精度问题

#### 方案 B：独立的 DirtyFlag 组件（不推荐）

**做法**：创建一个独立的 `TransformDirtyComponent`，只在需要更新时添加到实体上。

```cpp
struct TransformDirtyComponent {};  // 标签组件

// 标记 dirty
entity.AddComponent<TransformDirtyComponent>();

// 更新后移除
entity.RemoveComponent<TransformDirtyComponent>();
```

**优点**：
- 符合 ECS 的"标签组件"模式
- 可以用 entt 的 view 高效查询所有 dirty 实体

**缺点**：
- 频繁添加/移除组件有性能开销
- entt 的组件添加/移除会触发内部数据结构重组
- 不适合高频操作

### 3.4 层级更新逻辑修改

基于 **方案 A（子方案 A-1 或 A-2）**，修改 `UpdateTransformHierarchy`：

```cpp
void Scene::UpdateTransformHierarchy()
{
    auto view = m_Registry.view<TransformComponent, RelationshipComponent>();
    for (auto entityID : view)
    {
        Entity entity{ entityID, this };
        if (entity.GetParentUUID() == 0)
        {
            UpdateWorldTransform(entity, glm::mat4(1.0f), false);
        }
    }
}

void Scene::UpdateWorldTransform(Entity entity, const glm::mat4& parentWorldMatrix, bool parentDirty)
{
    auto& transform = entity.GetComponent<TransformComponent>();
    
    // 当前实体需要更新的条件：自身 dirty 或 父节点 dirty
    bool needsUpdate = transform.IsDirty() || parentDirty;
    
    if (needsUpdate)
    {
        transform.WorldMatrix = parentWorldMatrix * transform.GetLocalTransform();
        transform.ClearDirty();
    }
    
    // 递归更新子节点（传递 needsUpdate 作为 parentDirty）
    for (UUID childID : entity.GetChildren())
    {
        Entity child = GetEntityWithUUID(childID);
        UpdateWorldTransform(child, transform.WorldMatrix, needsUpdate);
    }
}
```

**关键点**：
- 即使当前实体不 dirty，仍需遍历子节点（因为子节点可能自身 dirty）
- `parentDirty` 参数向下传播，确保父节点变化时所有子孙都更新
- 如果当前实体和所有子孙都不 dirty，矩阵乘法被跳过（只有遍历开销）

### 3.5 进一步优化：子树剪枝（可选，高级）

如果能确定一个子树中没有任何 dirty 节点，可以跳过整个子树的遍历。这需要额外的 "子树中存在 dirty 节点" 标记。

```cpp
struct TransformComponent
{
    // ...
    bool m_Dirty = true;
    bool m_SubtreeHasDirty = true;  // 子树中是否有 dirty 节点
};
```

**标记逻辑**：当一个节点被标记为 dirty 时，向上传播 `m_SubtreeHasDirty = true` 到所有祖先节点。

**更新逻辑**：

```cpp
void Scene::UpdateWorldTransform(Entity entity, const glm::mat4& parentWorldMatrix, bool parentDirty)
{
    auto& transform = entity.GetComponent<TransformComponent>();
    
    bool needsUpdate = transform.IsDirty() || parentDirty;
    
    if (needsUpdate)
    {
        transform.WorldMatrix = parentWorldMatrix * transform.GetLocalTransform();
        transform.ClearDirty();
    }
    
    // 如果子树中没有 dirty 节点且当前也不需要更新，跳过子树
    if (!needsUpdate && !transform.m_SubtreeHasDirty)
    {
        return;  // 剪枝：跳过整个子树
    }
    
    transform.m_SubtreeHasDirty = false;  // 清除子树标记
    
    for (UUID childID : entity.GetChildren())
    {
        Entity child = GetEntityWithUUID(childID);
        UpdateWorldTransform(child, transform.WorldMatrix, needsUpdate);
    }
}
```

> **建议**：子树剪枝增加了实现复杂度，建议在确认有性能瓶颈后再实现。对于大多数场景，方案 A 的基本脏标记已经足够。

---

## 4. Transform 变更通知

### 4.1 目标

当实体的 Transform（World 或 Local）发生变化时，通知其他系统。典型的使用场景：
- **物理系统**：Transform 变化时需要同步更新刚体位置
- **音频系统**：3D 音源位置需要跟随 Transform
- **粒子系统**：发射器位置跟随 Transform
- **自定义脚本**：用户脚本可能需要响应 Transform 变化

### 4.2 方案选择：通知机制

#### 方案 A：版本号（Version Counter）（? 推荐，最优）

**做法**：在 `TransformComponent` 中维护一个递增的版本号。每次 WorldMatrix 更新时，版本号 +1。其他系统通过比较版本号来判断 Transform 是否发生了变化。

```cpp
struct TransformComponent
{
    // ... 现有字段 ...
    
private:
    uint32_t m_Version = 0;  // Transform 版本号
    
public:
    uint32_t GetVersion() const { return m_Version; }
    
    // 在 WorldMatrix 更新时调用（由 Scene 调用）
    void IncrementVersion() { ++m_Version; }
};
```

**使用方式**（其他系统）：

```cpp
// 物理系统示例
struct PhysicsBodyComponent
{
    uint32_t LastSyncedTransformVersion = 0;  // 上次同步时的 Transform 版本
};

void PhysicsSystem::Update()
{
    auto view = registry.view<TransformComponent, PhysicsBodyComponent>();
    for (auto entity : view)
    {
        auto& transform = view.get<TransformComponent>(entity);
        auto& physics = view.get<PhysicsBodyComponent>(entity);
        
        if (transform.GetVersion() != physics.LastSyncedTransformVersion)
        {
            // Transform 发生了变化，同步到物理引擎
            SyncTransformToPhysics(transform, physics);
            physics.LastSyncedTransformVersion = transform.GetVersion();
        }
    }
}
```

**在层级更新中递增版本号**：

```cpp
void Scene::UpdateWorldTransform(Entity entity, const glm::mat4& parentWorldMatrix, bool parentDirty)
{
    auto& transform = entity.GetComponent<TransformComponent>();
    
    bool needsUpdate = transform.IsDirty() || parentDirty;
    
    if (needsUpdate)
    {
        transform.WorldMatrix = parentWorldMatrix * transform.GetLocalTransform();
        transform.ClearDirty();
        transform.IncrementVersion();  // 版本号递增
    }
    
    // ... 递归子节点 ...
}
```

**优点**：
- 实现极其简单，只需一个 `uint32_t`
- 零开销：不需要回调、不需要事件系统
- 其他系统可以自主决定何时检查变化
- 不会有回调顺序问题
- 线程安全（如果使用 `std::atomic<uint32_t>`）

**缺点**：
- 是"拉取"模式，其他系统需要主动检查
- 无法知道具体是哪个属性变化了（Position? Rotation? Scale?）
- `uint32_t` 理论上会溢出（但 60fps 下需要运行约 2.27 年才会溢出，可忽略）

#### 方案 B：回调函数（Callback）（其次）

**做法**：在 `TransformComponent` 或 `Entity` 上注册回调函数，Transform 变化时调用。

```cpp
struct TransformComponent
{
    // ... 现有字段 ...
    
    using TransformChangedCallback = std::function<void(const TransformComponent&)>;
    
private:
    std::vector<TransformChangedCallback> m_OnChangedCallbacks;
    
public:
    void AddOnChangedCallback(TransformChangedCallback callback)
    {
        m_OnChangedCallbacks.push_back(std::move(callback));
    }
    
    void NotifyChanged()
    {
        for (auto& callback : m_OnChangedCallbacks)
        {
            callback(*this);
        }
    }
};
```

**优点**：
- "推送"模式，变化时立即通知
- 可以精确控制通知时机

**缺点**：
- `std::function` 有堆分配开销
- 回调的生命周期管理复杂（注册者被销毁后回调悬空）
- 回调执行顺序不确定
- 在 ECS 架构中，组件持有回调不太合适（组件应该是纯数据）
- 每个实体都需要存储回调列表，内存开销大

#### 方案 C：事件系统（Event System）（后续可考虑）

**做法**：通过全局或场景级别的事件系统发布 Transform 变化事件。

```cpp
struct TransformChangedEvent
{
    Entity entity;
    glm::mat4 oldWorldMatrix;
    glm::mat4 newWorldMatrix;
};

// 发布
EventSystem::Publish(TransformChangedEvent{ entity, oldWorld, newWorld });

// 订阅
EventSystem::Subscribe<TransformChangedEvent>([](const TransformChangedEvent& e) {
    // 处理变化
});
```

**优点**：
- 解耦最好，发布者和订阅者互不知道
- 可以携带丰富的变化信息

**缺点**：
- 需要一个完整的事件系统基础设施
- 大量实体变化时，事件数量可能很大
- 实现复杂度最高

### 4.3 推荐组合

| 需求场景 | 推荐方案 |
|---------|---------|
| 当前项目阶段（基础功能） | **方案 A（版本号）**，实现简单，足够使用 |
| 需要精确的变化通知 | 方案 A + 方案 C（版本号 + 事件系统） |
| 大规模场景 + 多系统联动 | 方案 A + 方案 C |

---

## 5. （可选）Transform 更新逻辑抽取为 TransformSystem

### 5.1 目标

将 Transform 层级更新逻辑从 `Scene` 类中抽取到独立的 `TransformSystem` 类，符合 ECS 的 "System" 概念。

### 5.2 方案选择

#### 方案 A：保持在 Scene 中（? 推荐，当前阶段）

**做法**：不抽取，`UpdateTransformHierarchy()` 和 `UpdateWorldTransform()` 保持为 `Scene` 的私有方法。

**优点**：
- 简单，不引入新的类
- Scene 已经是所有系统的协调者

**缺点**：
- Scene 类职责越来越多

#### 方案 B：抽取为 TransformSystem 类（其次，后续重构时可考虑）

```cpp
// TransformSystem.h
class TransformSystem
{
public:
    static void Update(entt::registry& registry);
    
private:
    static void UpdateWorldTransform(
        entt::registry& registry, 
        entt::entity entity, 
        const glm::mat4& parentWorldMatrix, 
        bool parentDirty
    );
};
```

```cpp
// Scene.cpp 中调用
void Scene::OnUpdate(DeltaTime dt, EditorCamera& camera)
{
    TransformSystem::Update(m_Registry);
    // ... 渲染 ...
}
```

**优点**：
- 职责分离，Scene 更轻量
- 符合 ECS 的 System 概念
- 可以独立测试

**缺点**：
- 需要新建文件
- TransformSystem 需要访问 Entity 的父子关系，可能需要额外的参数传递

---

## 6. 完整的 TransformComponent 最终形态

综合 Phase 1 ~ Phase 3 的所有修改，`TransformComponent` 的最终形态如下：

```cpp
struct TransformComponent
{
    // ==================== Local Transform ====================
    // 方案 A-1（推荐）：private + Setter
    // 方案 A-2（其次）：保持 public
    
    glm::vec3 Translation = { 0.0f, 0.0f, 0.0f };  // 或 private + Getter/Setter
    glm::vec3 Scale = { 1.0f, 1.0f, 1.0f };         // 或 private + Getter/Setter

    // ==================== World Transform（缓存）====================
    glm::mat4 WorldMatrix = glm::mat4(1.0f);

private:
    // 旋转
    glm::vec3 RotationEuler = { 0.0f, 0.0f, 0.0f };
    glm::quat Rotation = { 1.0f, 0.0f, 0.0f, 0.0f };
    
    // 脏标记
    bool m_Dirty = true;
    
    // 版本号（变更通知）
    uint32_t m_Version = 0;

public:
    // --- 构造函数 ---
    TransformComponent() = default;
    TransformComponent(const TransformComponent& other) = default;
    TransformComponent(const glm::vec3& translation) : Translation(translation) {}

    // --- Local Transform ---
    glm::mat4 GetLocalTransform() const;
    void SetLocalTransform(const glm::mat4& transform);

    // --- World Transform ---
    const glm::mat4& GetWorldTransform() const { return WorldMatrix; }

    // --- 旋转 ---
    glm::vec3 GetRotationEuler() const;
    void SetRotationEuler(const glm::vec3& euler);
    glm::quat GetRotation() const;
    void SetRotation(const glm::quat& quat);

    // --- 脏标记 ---
    bool IsDirty() const { return m_Dirty; }
    void MarkDirty() { m_Dirty = true; }
    void ClearDirty() { m_Dirty = false; }

    // --- 版本号 ---
    uint32_t GetVersion() const { return m_Version; }
    void IncrementVersion() { ++m_Version; }
};
```

---

## 7. 验证清单

### 7.1 脏标记验证

| # | 测试场景 | 预期结果 |
|---|---------|---------|
| 1 | 场景中有 100 个静止实体，只移动其中 1 个 | 只有被移动的实体及其子孙重新计算 WorldMatrix |
| 2 | 移动一个有 10 个子节点的父节点 | 父节点和所有 10 个子节点的 WorldMatrix 都更新 |
| 3 | 不做任何操作，连续运行多帧 | 第一帧后不再有任何矩阵计算（所有 dirty 标记已清除） |
| 4 | 新创建的实体 | 首帧 dirty = true，WorldMatrix 正确计算 |

### 7.2 版本号验证

| # | 测试场景 | 预期结果 |
|---|---------|---------|
| 5 | 实体不移动，连续读取 Version | Version 不变 |
| 6 | 移动实体，读取 Version | Version 递增 |
| 7 | 移动父节点，读取子节点 Version | 子节点 Version 也递增 |
| 8 | 其他系统通过 Version 检测变化 | 能正确检测到 Transform 变化 |

### 7.3 性能验证

| # | 测试场景 | 预期结果 |
|---|---------|---------|
| 9 | 1000 个静止实体 | 帧时间与 Phase 1 相比有明显降低 |
| 10 | 1000 个实体中移动 10 个 | 只有约 10 个实体的 WorldMatrix 被重新计算 |

---

## 8. 注意事项

### 8.1 脏标记与 Setter 的一致性

如果选择 **子方案 A-2（手动 MarkDirty）**，必须确保所有修改 Local Transform 的地方都调用了 `MarkDirty()`。建议在代码审查中重点关注此项。

已知需要调用 `MarkDirty()` 的位置：
- `InspectorPanel.cpp` 中修改 Position/Rotation/Scale 后
- `Entity::SetWorldPosition()` 等 Setter 中
- `Entity::SetParent()` 中（反算 Local Transform 后）
- 未来的动画系统、物理系统回写 Transform 时

### 8.2 序列化

`m_Dirty` 和 `m_Version` **不应被序列化**。反序列化后，`m_Dirty` 应初始化为 `true`（确保首帧计算 WorldMatrix），`m_Version` 初始化为 `0`。

### 8.3 版本号溢出

`uint32_t` 的最大值为 4,294,967,295。在 60fps 下，即使每帧都更新，也需要约 2.27 年才会溢出。溢出后会回绕到 0，可能导致一次误判（认为没有变化），但影响极小。如果需要绝对安全，可以使用 `uint64_t`。

---

## 9. 实现步骤总结

按以下顺序实现：

### 9.1 脏标记

1. **修改 `TransformComponent.h`**
   - 添加 `m_Dirty` 字段和 `IsDirty()` / `MarkDirty()` / `ClearDirty()` 方法
   - 根据选择的子方案，修改 Translation/Scale 的访问方式

2. **修改 `Scene.cpp`**
   - 修改 `UpdateWorldTransform()` 添加 `parentDirty` 参数
   - 在 `needsUpdate` 时才执行矩阵乘法
   - 更新后调用 `ClearDirty()`

3. **修改所有 Transform 修改处**
   - 如果选择子方案 A-2，在所有修改 Translation/Scale 的地方添加 `MarkDirty()` 调用
   - 如果选择子方案 A-1，修改所有直接访问 Translation/Scale 的代码改为使用 Setter

4. **修改 `Entity.cpp`**
   - 在 `SetParent()` 的 `SetLocalTransform()` 后调用 `MarkDirty()`
   - 在 `SetWorldPosition()` 等方法中调用 `MarkDirty()`

### 9.2 变更通知

5. **修改 `TransformComponent.h`**
   - 添加 `m_Version` 字段和 `GetVersion()` / `IncrementVersion()` 方法

6. **修改 `Scene.cpp`**
   - 在 `UpdateWorldTransform()` 中，WorldMatrix 更新后调用 `IncrementVersion()`

7. **编译并测试**
   - 按照第 7 节的验证清单逐项测试
