
# Phase 5：Hierarchy 面板拖拽排序与重设父子关系

> **目标**：实现类似 Unity Hierarchy 面板的拖拽交互，支持通过拖拽改变实体的父子关系和同级排列顺序。

---

## 目录

1. [需求分析](#1-需求分析)
2. [当前架构分析](#2-当前架构分析)
3. [方案选型](#3-方案选型)
4. [详细设计](#4-详细设计)
5. [修改清单](#5-修改清单)
6. [实现步骤](#6-实现步骤)
7. [测试验证](#7-测试验证)

---

## 1. 需求分析

### 1.1 功能需求

| 编号 | 功能 | 描述 |
|------|------|------|
| F1 | 拖拽设置父节点 | 将节点 A 拖到节点 B 上方释放 → A 成为 B 的子节点 |
| F2 | 拖拽取消父节点 | 将子节点拖到面板空白区域释放 → 该节点变为根节点 |
| F3 | 拖拽改变同级顺序 | 将节点 A 拖到节点 B 的上方间隙/下方间隙释放 → A 移动到 B 的前面/后面（同一父节点下） |
| F4 | 拖拽跨层级移动并指定位置 | 将节点 A 拖到另一个父节点下的某个位置 → A 成为新父节点的子节点，并插入到指定位置 |
| F5 | 防循环检测 | 禁止将父节点拖到自己的子孙节点下（会形成循环引用） |

### 1.2 交互行为（参考 Unity）

Unity Hierarchy 的拖拽交互分为三种放置模式，通过鼠标在目标节点上的 Y 位置来区分：

```
┌─────────────────────────────────┐
│  上方区域（约 25%）→ 插入到该节点前面（同级）
│─────────────────────────────────│
│  中间区域（约 50%）→ 成为该节点的子节点
│─────────────────────────────────│
│  下方区域（约 25%）→ 插入到该节点后面（同级）
└─────────────────────────────────┘
```

### 1.3 视觉反馈

| 放置模式 | 视觉提示 |
|----------|----------|
| 插入到前面 | 节点上方显示一条水平线 |
| 成为子节点 | 整个节点高亮（矩形框） |
| 插入到后面 | 节点下方显示一条水平线 |
| 无效目标（循环） | 不显示任何提示 / 鼠标变为禁止图标 |

---

## 2. 当前架构分析

### 2.1 数据结构

```cpp
// RelationshipComponent.h
struct RelationshipComponent
{
    UUID Parent = 0;                // 父节点 UUID（0 表示根节点）
    std::vector<UUID> Children;     // 子节点 UUID 列表（有序）
};
```

**关键点**：`Children` 是 `std::vector<UUID>`，天然有序，顺序即为 Hierarchy 中的显示顺序。

### 2.2 已有方法

| 方法 | 位置 | 功能 |
|------|------|------|
| `Entity::SetParent(Entity parent)` | `Entity.cpp` | 设置父节点，保持世界位置不变 |
| `Entity::RemoveChild(Entity child)` | `Entity.cpp` | 从 Children 列表中移除指定子节点 |
| `Entity::GetParent()` | `Entity.cpp` | 获取父实体 |
| `Entity::GetChildren()` | `Entity.h` | 获取子节点 UUID 列表引用 |

### 2.3 Hierarchy 面板绘制流程

```cpp
// SceneHierarchyPanel.cpp - OnGUI()
for (auto entityID : m_Scene->GetAllEntitiesWith<IDComponent, RelationshipComponent>())
{
    Entity entity{ entityID, m_Scene.get() };
    if (entity.GetParentUUID() == 0)    // 只绘制根节点
    {
        DrawEntityNode(entity);         // 递归绘制子树
    }
}
```

### 2.4 根节点顺序问题

当前根节点的遍历顺序依赖 `entt::registry::view` 的返回顺序。entt 的 view 遍历顺序是**反向创建顺序**（后创建的先遍历），且不可控。

**影响**：如果要支持根节点之间的排序，需要额外维护一个有序列表。

---

## 3. 方案选型

### 3.1 拖拽放置区域判断方案

#### 方案 A（推荐）：单 Target + 鼠标 Y 位置三分区

在每个树节点上同时作为 DragDropTarget，根据鼠标在节点矩形中的 Y 位置判断放置模式。

```cpp
if (ImGui::BeginDragDropTarget())
{
    ImVec2 mousePos = ImGui::GetMousePos();
    ImVec2 itemMin = ImGui::GetItemRectMin();
    ImVec2 itemMax = ImGui::GetItemRectMax();
    float itemHeight = itemMax.y - itemMin.y;
    float relativeY = (mousePos.y - itemMin.y) / itemHeight;

    DropPosition position;
    if (relativeY < 0.25f)
        position = DropPosition::Before;    // 插入到前面
    else if (relativeY > 0.75f)
        position = DropPosition::After;     // 插入到后面
    else
        position = DropPosition::Inside;    // 成为子节点

    // 根据 position 执行不同操作...
    ImGui::EndDragDropTarget();
}
```

| 维度 | 评价 |
|------|------|
| 优点 | 不需要额外 UI 元素；交互与 Unity 一致；代码集中在一处；视觉反馈可以精确控制 |
| 缺点 | 需要手动计算鼠标相对位置；需要自行绘制视觉提示线 |
| 推荐原因 | 这是 Unity、Unreal、Godot 等主流引擎的标准实现方式 |

#### 方案 B：独立间隙 Separator 作为 Target

在每个节点之间插入一个很窄的 `InvisibleButton` 作为排序的 DragDropTarget。

```cpp
// 在每个节点之前
std::string gapID = std::format("##Gap_{}", index);
ImGui::InvisibleButton(gapID.c_str(), ImVec2(-1, 3.0f));
if (ImGui::BeginDragDropTarget()) { /* 插入到此位置 */ }

// 绘制节点本身
DrawEntityNode(entity);
// 节点本身的 DragDropTarget 只处理"成为子节点"
```

| 维度 | 评价 |
|------|------|
| 优点 | 逻辑分离清晰；Source/Target 职责明确 |
| 缺点 | 间隙太窄难以命中（3px）；太宽影响布局美观；需要额外管理间隙 ID；与 TreeNode 展开/折叠交互冲突；递归绘制时间隙管理复杂 |
| 不推荐原因 | 用户体验差，实现复杂度反而更高 |

#### 方案 C：ImGui TreeNode 内置 DragDrop + 自定义 Payload

利用 ImGui 的 `ImGuiTreeNodeFlags_AllowItemOverlap` 和 DragDrop API，但不做区域细分，只支持"成为子节点"。

| 维度 | 评价 |
|------|------|
| 优点 | 实现最简单 |
| 缺点 | 不支持同级排序；功能不完整 |
| 不推荐原因 | 无法满足 F3、F4 需求 |

**最终选择：方案 A**

---

### 3.2 根节点排序方案

#### 方案 A（推荐）：Scene 维护根节点顺序列表

在 `Scene` 中新增 `m_RootEntityOrder`（`std::vector<UUID>`），Hierarchy 面板按此列表顺序绘制根节点。

```cpp
// Scene.h
std::vector<UUID> m_RootEntityOrder;    // 根节点显示顺序
```

| 维度 | 评价 |
|------|------|
| 优点 | 顺序完全可控；序列化后加载顺序一致；与 Children 列表的排序逻辑统一 |
| 缺点 | 需要在创建/销毁/重设父节点时维护该列表 |
| 推荐原因 | 这是唯一能正确支持根节点排序的方案；Unity 内部也维护了类似的根节点列表 |

#### 方案 B：依赖 entt 创建顺序

不额外维护列表，根节点顺序由 entt 的 view 遍历顺序决定。

| 维度 | 评价 |
|------|------|
| 优点 | 无额外代码 |
| 缺点 | 顺序不可控（entt view 是反向创建顺序）；无法实现根节点排序；序列化后顺序可能变化 |
| 不推荐原因 | 无法满足 F3 需求中根节点排序的部分 |

**最终选择：方案 A**

---

### 3.3 视觉反馈方案

#### 方案 A（推荐）：使用 ImGui DrawList 绘制提示线/高亮

在 DragDrop 悬停时，通过 `ImGui::GetWindowDrawList()` 绘制：
- 插入线：一条水平线（带小三角箭头）
- 子节点高亮：半透明矩形覆盖

```cpp
ImDrawList* drawList = ImGui::GetWindowDrawList();
if (position == DropPosition::Before)
{
    drawList->AddLine(
        ImVec2(itemMin.x, itemMin.y),
        ImVec2(itemMax.x, itemMin.y),
        IM_COL32(70, 130, 230, 255), 2.0f
    );
}
```

| 维度 | 评价 |
|------|------|
| 优点 | 完全自定义外观；不影响布局；性能好 |
| 缺点 | 需要手动管理绘制时机（在 DragDrop 活跃时绘制） |
| 推荐原因 | 灵活度最高，与项目现有的 ImGui 使用方式一致 |

#### 方案 B：使用 ImGui 内置的 DragDropTarget 高亮

依赖 ImGui 默认的 `ImGuiCol_DragDropTarget` 颜色高亮。

| 维度 | 评价 |
|------|------|
| 优点 | 零额外代码 |
| 缺点 | 只有一种高亮模式（矩形框）；无法区分三种放置模式；无法绘制插入线 |
| 不推荐原因 | 视觉反馈不够明确，用户无法区分放置模式 |

**最终选择：方案 A**

---

## 4. 详细设计

### 4.1 数据结构

#### 4.1.1 拖拽 Payload

```cpp
// SceneHierarchyPanel.cpp 文件作用域
static const char* s_EntityDragDropType = "ENTITY_HIERARCHY";
```

Payload 数据为 `UUID`（8 字节），直接传递被拖拽实体的 UUID。

#### 4.1.2 放置位置枚举

```cpp
// SceneHierarchyPanel.h 或 SceneHierarchyPanel.cpp 文件作用域
enum class DropPosition
{
    Before,     // 插入到目标节点前面（同级）
    Inside,     // 成为目标节点的子节点
    After       // 插入到目标节点后面（同级）
};
```

#### 4.1.3 Scene 新增根节点顺序列表

```cpp
// Scene.h - private 成员
std::vector<UUID> m_RootEntityOrder;    // 根节点显示顺序
```

### 4.2 Scene 层新增方法

#### 4.2.1 根节点顺序管理

```cpp
// Scene.h - public 接口
/// <summary>
/// 获取根节点的有序列表（用于 Hierarchy 面板按序绘制）
/// </summary>
const std::vector<UUID>& GetRootEntityOrder() const { return m_RootEntityOrder; }

/// <summary>
/// 将实体插入到根节点列表的指定位置
/// </summary>
/// <param name="entityID">实体 UUID</param>
/// <param name="index">插入位置索引（-1 表示末尾）</param>
void InsertRootEntity(UUID entityID, int index = -1);

/// <summary>
/// 从根节点列表中移除实体
/// </summary>
/// <param name="entityID">实体 UUID</param>
void RemoveRootEntity(UUID entityID);
```

#### 4.2.2 实现

```cpp
// Scene.cpp
void Scene::InsertRootEntity(UUID entityID, int index)
{
    // 先确保不重复
    auto it = std::find(m_RootEntityOrder.begin(), m_RootEntityOrder.end(), entityID);
    if (it != m_RootEntityOrder.end())
    {
        m_RootEntityOrder.erase(it);
    }

    if (index < 0 || index >= static_cast<int>(m_RootEntityOrder.size()))
    {
        m_RootEntityOrder.push_back(entityID);
    }
    else
    {
        m_RootEntityOrder.insert(m_RootEntityOrder.begin() + index, entityID);
    }
}

void Scene::RemoveRootEntity(UUID entityID)
{
    auto it = std::find(m_RootEntityOrder.begin(), m_RootEntityOrder.end(), entityID);
    if (it != m_RootEntityOrder.end())
    {
        m_RootEntityOrder.erase(it);
    }
}
```

#### 4.2.3 维护时机

需要在以下位置维护 `m_RootEntityOrder`：

| 操作 | 维护逻辑 |
|------|----------|
| `CreateEntity(name)` / `CreateEntity(uuid, name)` | 无 parent → `m_RootEntityOrder.push_back(uuid)` |
| `CreateEntity(name, parent)` / `CreateEntity(uuid, name, parent)` | 有 parent → 不添加到根节点列表；无 parent → 添加到根节点列表 |
| `DestroyEntity()` | 如果是根节点 → `RemoveRootEntity(uuid)` |
| `Entity::SetParent(parent)` | 如果原来是根节点 → `RemoveRootEntity()`；如果新父节点为空（变为根节点）→ `InsertRootEntity()` |
| 反序列化 | 按序列化顺序重建 `m_RootEntityOrder` |

### 4.3 Entity 层新增方法

#### 4.3.1 带位置的 SetParent

```cpp
// Entity.h
/// <summary>
/// 设置父节点，并指定在新父节点 Children 列表中的插入位置
/// 保持世界位置不变
/// </summary>
/// <param name="parent">新父节点（无效 Entity 表示设为根节点）</param>
/// <param name="insertIndex">在新父节点 Children 列表中的插入位置（-1 表示末尾）</param>
void SetParent(Entity parent, int insertIndex = -1);
```

#### 4.3.2 实现

```cpp
// Entity.cpp
void Entity::SetParent(Entity parent, int insertIndex)
{
    Entity currentParent = GetParent();

    // 目标父节点与当前父节点相同，且不需要调整顺序
    if (currentParent == parent && insertIndex == -1)
    {
        return;
    }

    auto& transform = GetComponent<TransformComponent>();

    // 保存当前世界矩阵（用于保持世界位置不变）
    glm::mat4 currentWorldTransform = transform.GetWorldTransform();

    // 从旧父节点移除
    if (currentParent)
    {
        currentParent.RemoveChild(*this);
    }
    else
    {
        // 原来是根节点，从根节点列表移除
        m_Scene->RemoveRootEntity(GetUUID());
    }

    // 设置父节点 UUID
    SetParentUUID(parent ? parent.GetUUID() : UUID(0));

    if (parent)
    {
        std::vector<UUID>& parentChildren = parent.GetChildren();
        UUID id = GetUUID();

        // 插入到指定位置
        if (insertIndex < 0 || insertIndex >= static_cast<int>(parentChildren.size()))
        {
            parentChildren.push_back(id);
        }
        else
        {
            parentChildren.insert(parentChildren.begin() + insertIndex, id);
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

        // 插入到根节点列表
        m_Scene->InsertRootEntity(GetUUID(), insertIndex);
    }
}
```

#### 4.3.3 同级排序（不改变父节点，只改变顺序）

```cpp
// Entity.h
/// <summary>
/// 在同一父节点下移动到指定位置（不改变父子关系，不改变 Transform）
/// </summary>
/// <param name="newIndex">新的位置索引</param>
void MoveToIndex(int newIndex);
```

```cpp
// Entity.cpp
void Entity::MoveToIndex(int newIndex)
{
    UUID id = GetUUID();
    Entity parent = GetParent();

    if (parent)
    {
        std::vector<UUID>& siblings = parent.GetChildren();
        auto it = std::find(siblings.begin(), siblings.end(), id);
        if (it != siblings.end())
        {
            siblings.erase(it);
        }

        if (newIndex < 0 || newIndex >= static_cast<int>(siblings.size()))
        {
            siblings.push_back(id);
        }
        else
        {
            siblings.insert(siblings.begin() + newIndex, id);
        }
    }
    else
    {
        // 根节点排序
        m_Scene->RemoveRootEntity(id);
        m_Scene->InsertRootEntity(id, newIndex);
    }
}
```

### 4.4 循环检测

#### 4.4.1 辅助方法

```cpp
// SceneHierarchyPanel.h - private
/// <summary>
/// 检查 potentialChild 是否是 potentialParent 的祖先节点
/// 用于防止拖拽时形成循环引用
/// </summary>
/// <param name="potentialParent">潜在的新父节点</param>
/// <param name="potentialChild">被拖拽的节点（潜在的新子节点）</param>
/// <returns>如果 potentialParent 是 potentialChild 的子孙，返回 true（表示会形成循环）</returns>
bool WouldCreateCycle(Entity potentialParent, Entity potentialChild);
```

```cpp
// SceneHierarchyPanel.cpp
bool SceneHierarchyPanel::WouldCreateCycle(Entity potentialParent, Entity potentialChild)
{
    // 不能将节点设为自身的子节点
    if (potentialParent == potentialChild)
    {
        return true;
    }

    // 检查 potentialParent 是否是 potentialChild 的子孙
    Entity current = potentialParent.GetParent();
    while (current)
    {
        if (current == potentialChild)
        {
            return true;
        }
        current = current.GetParent();
    }

    return false;
}
```

### 4.5 SceneHierarchyPanel 拖拽逻辑

#### 4.5.1 DrawEntityNode 完整修改

```cpp
void SceneHierarchyPanel::DrawEntityNode(Entity entity)
{
    const std::string& name = entity.GetComponent<NameComponent>().Name;
    UUID id = entity.GetUUID();
    const std::string& strID = std::format("{0}##{1}", name, static_cast<uint64_t>(id));

    bool isLeaf = entity.GetChildren().empty();

    bool opened = UI::BeginTreeNode(strID.c_str(), false, SelectionManager::IsSelected(id), isLeaf);

    // 树结点被点击
    if (ImGui::IsItemClicked())
    {
        SelectionManager::Select(id);
        LF_TRACE("Selected Entity: [ENTT = {0}, UUID {1}, Name {2}]", static_cast<uint32_t>(entity), id, entity.GetName());
    }

    // ---- 拖拽源 ----
    if (ImGui::BeginDragDropSource(ImGuiDragDropFlags_SourceNoHoldToOpenOthers))
    {
        UUID draggedID = entity.GetUUID();
        ImGui::SetDragDropPayload(s_EntityDragDropType, &draggedID, sizeof(UUID));
        ImGui::Text("%s", name.c_str());    // 拖拽预览
        ImGui::EndDragDropSource();
    }

    // ---- 拖拽目标 ----
    if (ImGui::BeginDragDropTarget())
    {
        // 计算鼠标在节点矩形中的相对 Y 位置
        ImVec2 mousePos = ImGui::GetMousePos();
        ImVec2 itemMin = ImGui::GetItemRectMin();
        ImVec2 itemMax = ImGui::GetItemRectMax();
        float itemHeight = itemMax.y - itemMin.y;
        float relativeY = (mousePos.y - itemMin.y) / itemHeight;

        // 判断放置模式
        DropPosition dropPos;
        if (relativeY < 0.25f)
        {
            dropPos = DropPosition::Before;
        }
        else if (relativeY > 0.75f)
        {
            dropPos = DropPosition::After;
        }
        else
        {
            dropPos = DropPosition::Inside;
        }

        // 绘制视觉反馈
        ImDrawList* drawList = ImGui::GetWindowDrawList();
        ImU32 lineColor = IM_COL32(70, 130, 230, 255);

        switch (dropPos)
        {
            case DropPosition::Before:
                drawList->AddLine(
                    ImVec2(itemMin.x, itemMin.y),
                    ImVec2(itemMax.x, itemMin.y),
                    lineColor, 2.0f
                );
                break;
            case DropPosition::After:
                drawList->AddLine(
                    ImVec2(itemMin.x, itemMax.y),
                    ImVec2(itemMax.x, itemMax.y),
                    lineColor, 2.0f
                );
                break;
            case DropPosition::Inside:
                drawList->AddRect(itemMin, itemMax, lineColor, 2.0f, 0, 2.0f);
                break;
        }

        // 接收拖拽
        if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload(s_EntityDragDropType))
        {
            UUID droppedID = *static_cast<UUID*>(payload->Data);
            Entity droppedEntity = m_Scene->GetEntityWithUUID(droppedID);

            if (droppedEntity && droppedEntity != entity)
            {
                switch (dropPos)
                {
                    case DropPosition::Inside:
                    {
                        // 成为子节点（防循环检测）
                        if (!WouldCreateCycle(entity, droppedEntity))
                        {
                            droppedEntity.SetParent(entity);
                        }
                        break;
                    }
                    case DropPosition::Before:
                    {
                        // 插入到目标节点前面
                        Entity targetParent = entity.GetParent();
                        if (!WouldCreateCycle(targetParent, droppedEntity))
                        {
                            int targetIndex = GetEntityIndexInParent(entity);
                            droppedEntity.SetParent(targetParent, targetIndex);
                        }
                        break;
                    }
                    case DropPosition::After:
                    {
                        // 插入到目标节点后面
                        Entity targetParent = entity.GetParent();
                        if (!WouldCreateCycle(targetParent, droppedEntity))
                        {
                            int targetIndex = GetEntityIndexInParent(entity);
                            droppedEntity.SetParent(targetParent, targetIndex + 1);
                        }
                        break;
                    }
                }
            }
        }

        ImGui::EndDragDropTarget();
    }

    // ---- 右键菜单（不变） ----
    const std::string rightClickPopupID = std::format("{0}-ContextMenu", strID);
    bool entityDeleted = false;
    if (UI::BeginPopupContextItem(rightClickPopupID.c_str(), ImGuiPopupFlags_MouseButtonRight))
    {
        SelectionManager::Select(id);
        if (ImGui::MenuItem("Delete"))
        {
            entityDeleted = true;
        }
        ImGui::Separator();
        DrawEntityCreateMenu(entity);
        UI::EndPopup();
    }

    // 树结点已打开
    if (opened)
    {
        std::vector<UUID> children = entity.GetChildren();
        for (UUID childID : children)
        {
            Entity child = m_Scene->GetEntityWithUUID(childID);
            DrawEntityNode(child);
        }
        UI::EndTreeNode();
    }

    if (entityDeleted)
    {
        m_Scene->DestroyEntity(entity);
        if (SelectionManager::IsSelected(id))
        {
            SelectionManager::Deselect();
        }
    }
}
```

#### 4.5.2 获取实体在父节点中的索引

```cpp
// SceneHierarchyPanel.h - private
/// <summary>
/// 获取实体在其父节点 Children 列表中的索引
/// 如果是根节点，返回在根节点列表中的索引
/// </summary>
int GetEntityIndexInParent(Entity entity);
```

```cpp
// SceneHierarchyPanel.cpp
int SceneHierarchyPanel::GetEntityIndexInParent(Entity entity)
{
    UUID id = entity.GetUUID();
    Entity parent = entity.GetParent();

    if (parent)
    {
        const std::vector<UUID>& siblings = parent.GetChildren();
        auto it = std::find(siblings.begin(), siblings.end(), id);
        if (it != siblings.end())
        {
            return static_cast<int>(std::distance(siblings.begin(), it));
        }
    }
    else
    {
        const std::vector<UUID>& rootOrder = m_Scene->GetRootEntityOrder();
        auto it = std::find(rootOrder.begin(), rootOrder.end(), id);
        if (it != rootOrder.end())
        {
            return static_cast<int>(std::distance(rootOrder.begin(), it));
        }
    }

    return -1;
}
```

#### 4.5.3 OnGUI 修改 ? 根节点按序绘制 + 空白区域 DragDropTarget

```cpp
void SceneHierarchyPanel::OnGUI()
{
    std::string sceneName = m_Scene->GetName();
    const std::string& strSceneID = std::format("{0}##{1}", sceneName, typeid(Scene).hash_code());

    ImGui::PushFont(ImGui::GetIO().Fonts->Fonts[0]);
    bool opened = UI::BeginTreeNode(strSceneID.c_str(), true);
    ImGui::PopFont();

    if (opened)
    {
        // 【修改】按 m_RootEntityOrder 顺序绘制根节点
        const std::vector<UUID>& rootOrder = m_Scene->GetRootEntityOrder();
        for (UUID rootID : rootOrder)
        {
            Entity entity = m_Scene->TryGetEntityWithUUID(rootID);
            if (entity)
            {
                DrawEntityNode(entity);
            }
        }

        UI::EndTreeNode();
    }

    // 点击空白位置取消选中
    if (ImGui::IsMouseClicked(0) && ImGui::IsWindowHovered())
    {
        SelectionManager::Deselect();
    }

    // 【新增】空白区域作为 DragDropTarget：拖到空白 → 变为根节点（末尾）
    if (ImGui::BeginDragDropTarget())
    {
        if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload(s_EntityDragDropType))
        {
            UUID droppedID = *static_cast<UUID*>(payload->Data);
            Entity droppedEntity = m_Scene->GetEntityWithUUID(droppedID);
            if (droppedEntity)
            {
                droppedEntity.SetParent({});    // 设为根节点
            }
        }
        ImGui::EndDragDropTarget();
    }

    // 右键菜单（不变）
    if (UI::BeginPopupContextWindow(nullptr, ImGuiPopupFlags_MouseButtonRight | ImGuiPopupFlags_NoOpenOverItems))
    {
        DrawEntityCreateMenu({});
        UI::EndPopup();
    }
}
```

> **关于空白区域 DragDropTarget 的实现方式：**

**选项 A（推荐）：在窗口内容区域末尾添加一个大的 InvisibleButton 作为 Target**

```cpp
// 在 TreeNode 结束后、右键菜单之前
float remainingHeight = ImGui::GetContentRegionAvail().y;
if (remainingHeight > 0)
{
    ImGui::InvisibleButton("##HierarchyDropArea", ImVec2(-1, remainingHeight));
    if (ImGui::BeginDragDropTarget())
    {
        // ...
        ImGui::EndDragDropTarget();
    }
}
```

优点：明确的可交互区域；不会与其他 Target 冲突
缺点：需要计算剩余高度

**选项 B：使用 `ImGui::BeginDragDropTargetCustom` 覆盖整个窗口**

```cpp
ImRect windowRect(ImGui::GetWindowContentRegionMin() + ImGui::GetWindowPos(),
                  ImGui::GetWindowContentRegionMax() + ImGui::GetWindowPos());
if (ImGui::BeginDragDropTargetCustom(windowRect, ImGui::GetID("##HierarchyWindow")))
{
    // ...
}
```

优点：覆盖整个窗口，包括已有节点之间的空隙
缺点：可能与节点自身的 DragDropTarget 冲突（ImGui 优先匹配更小的 Target）

**推荐选项 A**：更安全，不会与节点 Target 冲突。

### 4.6 序列化支持

#### 4.6.1 序列化根节点顺序

```cpp
// SceneSerializer.cpp - Serialize()
// 在 "Entitys" 之前，序列化根节点顺序
out << YAML::Key << "RootEntityOrder" << YAML::Value << YAML::Flow << YAML::BeginSeq;
for (UUID rootID : m_Scene->m_RootEntityOrder)
{
    out << static_cast<uint64_t>(rootID);
}
out << YAML::EndSeq;
```

#### 4.6.2 反序列化根节点顺序

```cpp
// SceneSerializer.cpp - Deserialize()
// 在实体反序列化完成后，恢复根节点顺序
YAML::Node rootOrderNode = data["RootEntityOrder"];
if (rootOrderNode && rootOrderNode.IsSequence())
{
    m_Scene->m_RootEntityOrder.clear();
    for (auto node : rootOrderNode)
    {
        UUID id = node.as<uint64_t>();
        m_Scene->m_RootEntityOrder.push_back(id);
    }
}
else
{
    // 兼容旧场景文件：没有 RootEntityOrder 字段时，按实体创建顺序构建
    m_Scene->m_RootEntityOrder.clear();
    for (auto& [uuid, entity] : m_Scene->m_EntityIDMap)
    {
        if (entity.GetParentUUID() == 0)
        {
            m_Scene->m_RootEntityOrder.push_back(uuid);
        }
    }
}
```

### 4.7 CreateEntity / DestroyEntity 维护

#### 4.7.1 CreateEntity 修改

新增带 `parent` 参数的 `CreateEntity` 重载版本。在创建时就确定实体的层级关系，避免"先加到根节点列表再移除"的冗余操作。

**Scene.h 新增声明：**

```cpp
/// <summary>
/// 创建实体（指定父节点）
/// </summary>
/// <param name="name">实体名</param>
/// <param name="parent">父实体（无效 Entity 表示创建为根节点）</param>
/// <returns>实体</returns>
Entity CreateEntity(const std::string& name, Entity parent);
Entity CreateEntity(UUID uuid, const std::string& name, Entity parent);
```

**Scene.cpp 实现：**

```cpp
Entity Scene::CreateEntity(const std::string& name, Entity parent)
{
    return CreateEntity({}, name, parent);
}

Entity Scene::CreateEntity(UUID uuid, const std::string& name, Entity parent)
{
    Entity entity = { m_Registry.create(), this };

    entity.AddComponent<IDComponent>(uuid);
    entity.AddComponent<NameComponent>(name);
    entity.AddComponent<TransformComponent>();
    entity.AddComponent<RelationshipComponent>();

    m_EntityIDMap[uuid] = entity;

    if (parent)
    {
        // 有父节点：设置关系，不添加到根节点列表
        entity.SetParentUUID(parent.GetUUID());
        parent.GetChildren().push_back(uuid);
    }
    else
    {
        // 无父节点：添加到根节点列表
        m_RootEntityOrder.push_back(uuid);
    }

    LF_TRACE("Created Entity: [ENTT = {0}, UUID {1}, Name {2}]", static_cast<uint32_t>(entity), uuid, name);

    return entity;
}
```

**原有无 parent 参数的版本保持不变（创建为根节点）：**

```cpp
Entity Scene::CreateEntity(UUID uuid, const std::string& name)
{
    Entity entity = { m_Registry.create(), this };

    entity.AddComponent<IDComponent>(uuid);
    entity.AddComponent<NameComponent>(name);
    entity.AddComponent<TransformComponent>();
    entity.AddComponent<RelationshipComponent>();

    m_EntityIDMap[uuid] = entity;

    // 无 parent 参数 → 默认为根节点
    m_RootEntityOrder.push_back(uuid);

    LF_TRACE("Created Entity: [ENTT = {0}, UUID {1}, Name {2}]", static_cast<uint32_t>(entity), uuid, name);

    return entity;
}
```

> **设计说明**：
> - `parent` 为无效 Entity（默认构造）时，行为与原有无参版本一致（创建为根节点）
> - `parent` 有效时，直接建立父子关系，不经过根节点列表
> - 新创建的子节点 Transform 为默认值（局部原点），即在父节点的位置上，这是期望的行为
> - 不使用 `SetParent()` 是因为 `SetParent()` 会保持世界位置不变（重算局部 Transform），而新创建的实体应该在父节点的局部原点

#### 4.7.2 DestroyEntity 修改

```cpp
// Scene.cpp - DestroyEntity()
void Scene::DestroyEntity(Entity entity)
{
    if (!entity)
    {
        return;
    }

    // 拷贝子节点列表后再遍历
    std::vector<UUID> children = entity.GetChildren();
    for (UUID childID : children)
    {
        Entity child = GetEntityWithUUID(childID);
        DestroyEntity(child);
    }

    // 将当前节点从父节点移除
    Entity parent = entity.GetParent();
    if (parent)
    {
        parent.RemoveChild(entity);
    }

    UUID id = entity.GetUUID();

    // 【新增】如果是根节点，从根节点列表移除
    RemoveRootEntity(id);

    LF_TRACE("Removed Entity: [ENTT = {0}, UUID {1}, Name {2}]", static_cast<uint32_t>(entity), id, entity.GetName());

    m_Registry.destroy(entity);
    m_EntityIDMap.erase(id);
}
```

### 4.8 SetParent 中维护根节点列表

在 `Entity::SetParent` 中已经包含了对 `m_Scene->RemoveRootEntity()` 和 `m_Scene->InsertRootEntity()` 的调用（见 4.3.2 节）。

### 4.9 SceneHierarchyPanel 中 DrawEntityCreateMenu 修改

由于新增了带 `parent` 参数的 `CreateEntity` 重载（见 4.7.1 节），`DrawEntityCreateMenu` 的修改变得非常简洁。

**修改前（当前代码）：**

```cpp
newEntity = m_Scene->CreateEntity(uniqueName);
// ...
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
```

**修改后：**

```cpp
// 创建实体时直接传入 parent，CreateEntity 内部处理层级关系和根节点列表
newEntity = m_Scene->CreateEntity(uniqueName, parent);
// ...
if (newEntity)
{
    // 不需要任何额外的父子关系处理，CreateEntity 已经搞定了
    SelectionManager::Deselect();
    SelectionManager::Select(newEntity.GetUUID());
}
```

所有创建实体的调用点统一改为：

```cpp
// 创建空物体
if (ImGui::MenuItem("Create Empty"))
{
    std::string uniqueName = GenerateUniqueName("Entity", parent);
    newEntity = m_Scene->CreateEntity(uniqueName, parent);
}

// 创建 Cube
if (ImGui::MenuItem("Cube"))
{
    std::string uniqueName = GenerateUniqueName("Cube", parent);
    newEntity = m_Scene->CreateEntity(uniqueName, parent);
    newEntity.AddComponent<MeshFilterComponent>(PrimitiveType::Cube);
    // ...
}

// 其他类型同理...
```

**优点**：
- 语义清晰：创建时就确定了层级关系，不存在中间状态
- 调用方简洁：不需要额外的根节点列表维护代码
- 无冗余操作：不存在"先加到根节点列表再移除"的过程
- 一致性好：所有创建实体的路径都通过统一的接口

---

## 5. 修改清单

### 5.1 引擎核心（Lucky/Source/Lucky/）

| 文件 | 修改类型 | 修改内容 |
|------|----------|----------|
| `Scene/Scene.h` | **新增** | 添加 `m_RootEntityOrder` 成员；添加 `GetRootEntityOrder()`、`InsertRootEntity()`、`RemoveRootEntity()` 方法；新增带 `parent` 参数的 `CreateEntity` 重载声明 |
| `Scene/Scene.cpp` | **修改** | 实现 `InsertRootEntity()`、`RemoveRootEntity()`；实现带 `parent` 参数的 `CreateEntity` 重载；原有 `CreateEntity()` 中添加到根节点列表；`DestroyEntity()` 中从根节点列表移除 |
| `Scene/Entity.h` | **修改** | `SetParent()` 签名添加 `insertIndex` 参数；新增 `MoveToIndex()` 方法 |
| `Scene/Entity.cpp` | **修改** | 重写 `SetParent()` 支持 insertIndex 和根节点列表维护；实现 `MoveToIndex()` |
| `Serialization/SceneSerializer.cpp` | **修改** | 序列化/反序列化 `RootEntityOrder` |

### 5.2 编辑器应用（Luck3DApp/Source/）

| 文件 | 修改类型 | 修改内容 |
|------|----------|----------|
| `Panels/SceneHierarchyPanel.h` | **修改** | 新增 `DropPosition` 枚举；新增 `WouldCreateCycle()`、`GetEntityIndexInParent()` 方法声明 |
| `Panels/SceneHierarchyPanel.cpp` | **修改** | `DrawEntityNode()` 添加 DragDropSource/Target 逻辑；`OnGUI()` 改为按 `m_RootEntityOrder` 绘制；添加空白区域 DragDropTarget；实现辅助方法；`DrawEntityCreateMenu()` 改用带 `parent` 参数的 `CreateEntity` |

### 5.3 不需要修改的文件

| 文件 | 原因 |
|------|------|
| `Scene/Components/RelationshipComponent.h` | Children 已是 vector，天然有序 |
| `Scene/Components/TransformComponent.h` | 不涉及 Transform 逻辑变更 |
| `Panels/InspectorPanel.cpp` | 不涉及层级操作 |
| `Panels/SceneViewportPanel.cpp` | 不涉及层级操作 |

---

## 6. 实现步骤

### 步骤 1：Scene 层 ? 根节点顺序列表

1. 在 `Scene.h` 中添加 `m_RootEntityOrder` 成员和相关方法声明
2. 在 `Scene.cpp` 中实现 `InsertRootEntity()` 和 `RemoveRootEntity()`
3. 新增带 `parent` 参数的 `CreateEntity` 重载：有 parent 时设置父子关系，无 parent 时添加到 `m_RootEntityOrder`
4. 修改原有 `CreateEntity()`：创建实体后添加到 `m_RootEntityOrder` 末尾
5. 修改 `DestroyEntity()`：销毁前从 `m_RootEntityOrder` 移除

### 步骤 2：Entity 层 ? SetParent 支持 insertIndex

1. 修改 `Entity.h` 中 `SetParent()` 签名，添加 `int insertIndex = -1` 参数
2. 重写 `Entity.cpp` 中 `SetParent()` 实现，支持 insertIndex 和根节点列表维护
3. 新增 `MoveToIndex()` 方法

### 步骤 3：SceneHierarchyPanel ? 拖拽逻辑

1. 在 `SceneHierarchyPanel.h` 中添加 `DropPosition` 枚举和辅助方法声明
2. 在 `SceneHierarchyPanel.cpp` 中定义 `s_EntityDragDropType` 常量
3. 修改 `DrawEntityNode()`：添加 DragDropSource 和 DragDropTarget 逻辑
4. 实现 `WouldCreateCycle()` 和 `GetEntityIndexInParent()`
5. 修改 `OnGUI()`：按 `m_RootEntityOrder` 顺序绘制根节点
6. 添加空白区域 DragDropTarget

### 步骤 4：DrawEntityCreateMenu 修改

1. 将所有 `m_Scene->CreateEntity(uniqueName)` 调用改为 `m_Scene->CreateEntity(uniqueName, parent)`
2. 删除 `if (parent) { ... }` 中手动设置父子关系的代码块（CreateEntity 内部已处理）

### 步骤 5：序列化支持

1. 修改 `SceneSerializer::Serialize()`：在实体序列之前序列化 `RootEntityOrder`
2. 修改 `SceneSerializer::Deserialize()`：反序列化 `RootEntityOrder`，兼容旧文件

### 步骤 6：编译验证与调试

1. 编译修复所有错误
2. 验证基本拖拽功能
3. 验证视觉反馈
4. 验证序列化/反序列化

---

## 7. 测试验证

### 7.1 基本拖拽设置父节点

1. 创建 A、B 两个根节点
2. 将 A 拖到 B 的中间区域释放 → A 成为 B 的子节点
3. 验证 A 的世界位置不变
4. 验证 Inspector 中 A 的 Position 变为相对于 B 的局部坐标

### 7.2 拖拽取消父节点

1. 将 B 的子节点 A 拖到面板空白区域释放 → A 变为根节点
2. 验证 A 的世界位置不变
3. 验证 A 出现在根节点列表末尾

### 7.3 同级排序

1. 创建 A、B、C 三个根节点（顺序为 A、B、C）
2. 将 C 拖到 A 的上方区域释放 → 顺序变为 C、A、B
3. 将 A 拖到 B 的下方区域释放 → 顺序变为 C、B、A

### 7.4 跨层级移动并指定位置

1. 创建 Parent（含子节点 Child1、Child2）和 Root
2. 将 Root 拖到 Child1 的下方区域释放 → Root 成为 Parent 的子节点，位于 Child1 和 Child2 之间
3. 验证 Root 的世界位置不变

### 7.5 防循环检测

1. 创建 A → B → C 层级
2. 尝试将 A 拖到 C 的中间区域 → 操作被拒绝（不执行）
3. 尝试将 A 拖到 B 的中间区域 → 操作被拒绝
4. 尝试将 A 拖到 A 自身 → 操作被拒绝

### 7.6 序列化测试

1. 创建带层级的场景，调整根节点顺序
2. 保存场景
3. 重新加载 → 验证根节点顺序正确恢复
4. 验证子节点顺序正确恢复

### 7.7 兼容性测试

1. 加载旧场景文件（无 `RootEntityOrder` 字段）→ 验证不崩溃，根节点按默认顺序显示

### 7.8 边界情况

1. 拖拽唯一的根节点到空白区域 → 无变化
2. 拖拽节点到自身的直接子节点的上方/下方区域 → 正常执行（改变子节点顺序）
3. 快速连续拖拽多次 → 无崩溃，状态一致

---

## 附录 A：ImGui DragDrop API 速查

```cpp
// 拖拽源
if (ImGui::BeginDragDropSource(ImGuiDragDropFlags flags = 0))
{
    ImGui::SetDragDropPayload(const char* type, const void* data, size_t size);
    // 绘制预览内容...
    ImGui::EndDragDropSource();
}

// 拖拽目标
if (ImGui::BeginDragDropTarget())
{
    if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload(const char* type))
    {
        // 处理 payload->Data...
    }
    ImGui::EndDragDropTarget();
}
```

常用 Flags：
- `ImGuiDragDropFlags_SourceNoHoldToOpenOthers`：拖拽时不触发其他节点的展开
- `ImGuiDragDropFlags_SourceNoPreviewTooltip`：不显示预览 tooltip
- `ImGuiDragDropFlags_AcceptNoDrawDefaultRect`：不绘制默认的高亮矩形

---

## 附录 B：与 Unity 行为对比

| 行为 | Unity | 本方案 |
|------|-------|--------|
| 拖拽设置父节点 | 拖到节点中间区域 | 拖到节点中间 50% 区域 |
| 拖拽排序 | 拖到节点上/下边缘 | 拖到节点上/下 25% 区域 |
| 拖拽到空白 | 变为根节点（末尾） | 变为根节点（末尾） |
| 世界位置保持 | 默认保持 | 保持 |
| 防循环 | 禁止操作 | 禁止操作（静默忽略） |
| 视觉反馈 | 蓝色插入线 / 蓝色高亮框 | 蓝色插入线 / 蓝色高亮框 |
| 根节点排序 | 支持 | 支持（通过 m_RootEntityOrder） |
| 多选拖拽 | 支持 | 暂不支持（后续扩展） |

---

## 附录 C：后续扩展方向

### C.1 多选拖拽

支持 Ctrl+Click 多选后批量拖拽。需要：
- `SelectionManager` 支持多选
- 拖拽时 Payload 传递 UUID 列表
- 批量执行 SetParent

### C.2 拖拽时自动展开

当拖拽悬停在折叠的父节点上超过一定时间（如 0.5s）时，自动展开该节点。

```cpp
// 使用 ImGuiDragDropFlags_AcceptBeforeDelivery 配合计时器
static float s_HoverTimer = 0.0f;
static UUID s_HoverTarget = 0;

if (/* 正在拖拽悬停在折叠节点上 */)
{
    if (s_HoverTarget != id)
    {
        s_HoverTarget = id;
        s_HoverTimer = 0.0f;
    }
    s_HoverTimer += dt;
    if (s_HoverTimer > 0.5f)
    {
        ImGui::GetStateStorage()->SetInt(ImGui::GetID(strID.c_str()), 1); // 强制展开
    }
}
```

### C.3 Undo/Redo 支持

拖拽操作应该支持撤销/重做。需要记录：
- 操作前的父节点和索引
- 操作前的局部 Transform
- 操作后的父节点和索引
- 操作后的局部 Transform
