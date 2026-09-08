# 组件设置菜单（Component Context Menu）设计文档

## 1. 概述

本设计文档描述 Inspector 中每个组件头部右上角"齿轮/Settings"按钮所弹出的上下文菜单（下文统称 **Component Context Menu**）的可扩展架构。目标是：

1. 支持一组**通用菜单项**（Reset / Copy / Paste / Remove 等）自动挂到所有组件上；
2. 支持组件**独有菜单项**（如脚本组件的 `Edit Script`）由该组件自行声明；
3. 每一项支持**运行期可见性 (`IsVisible`) 与启用性 (`IsEnabled`)** 判定；
4. 新增菜单项**不需要修改** `InspectorPanel`，也不需要在 `ComponentDescriptor` 上无限制加字段。

对齐参考：Unity Inspector 组件齿轮菜单模型（通用项 + 组件私有项 + 运行期启用/禁用）。

### 1.1 当前状态

- `ComponentDescriptor` 中只有 `bool CanRemove` + `RemoveFn Remove`；[ComponentDescriptor.h](../../Lucky/Source/Lucky/Scene/ComponentDescriptor.h)。
- `InspectorPanel::DrawComponentHeader` 中弹窗内容硬编码：只显示 `Remove Component` 一项；[InspectorPanel.cpp](../../Luck3DApp/Source/Panels/InspectorPanel.cpp)。
- 弹窗 ID 为固定字符串 `"ComponentSettings"`，未按组件区分。

### 1.2 本文档范围

**做**：

1. 新增 `ComponentContextMenuItem` 数据结构；
2. 新增 `ComponentContextMenuRegistry` 静态类，管理**通用项**注册；
3. 在 `ComponentDescriptor` 中新增 `ExtraContextMenuItems` 字段，承载**组件私有项**；
4. 改造 `InspectorPanel::DrawComponentHeader` 的 Settings 弹窗：以"通用段 + 私有段"两段绘制；
5. 修复弹窗 ID 冲突（`ComponentSettings##<Type>`）；
6. 内置注册一个通用项：`Remove Component`（迁移现有行为，保持零回归）。

**不做**：

- 不引入组件剪贴板（`ComponentClipboard`）；`Copy / Paste` 项作为后续独立特性接入，本文档只留出扩展位；
- 不引入 `Reset` 语义（缺少每个组件的默认值来源，属独立议题）；
- 不引入撤销/重做集成（未来 `EditorAction` 系统统一接入）；
- 不引入组件排序/`Move Up / Move Down`（依赖 Inspector 排序特性）。

---

## 2. 涉及的文件

### 2.1 需要新建

| 文件 | 说明 |
|------|------|
| `Lucky/Source/Lucky/Editor/ComponentContextMenuRegistry.h` | 通用菜单项注册表接口 |
| `Lucky/Source/Lucky/Editor/ComponentContextMenuRegistry.cpp` | 通用菜单项注册表实现 + 内置项注册 |

### 2.2 需要修改

| 文件 | 说明 |
|------|------|
| `Lucky/Source/Lucky/Scene/ComponentDescriptor.h` | 新增 `ComponentContextMenuItem` 结构体；新增 `ExtraContextMenuItems` 字段 |
| `Lucky/Source/Lucky/Scene/ComponentRegistry.h` | `RegisterInspector` 增加 `extraContextMenuItems` 参数（推荐 P0，见 §5.4） |
| `Lucky/Source/Lucky/Scene/ComponentRegistry.cpp` | `RegisterInspector` 实现同步 |
| `Lucky/Source/Lucky/Editor/ComponentInspectors.cpp` | 各组件 `RegisterInspector` 调用点补齐 `{}` 空私有段参数 |
| `Luck3DApp/Source/Panels/InspectorPanel.cpp` | Settings 弹窗改为遍历"通用段 + 私有段" |
| `Luck3DApp/Source/EditorLayer.cpp` | `OnAttach` 中在 `ComponentRegistry::RegisterAll()` 之后一行调用 `ComponentContextMenuRegistry::RegisterBuiltins()`；`OnDetach` 中 `Clear()` |

### 2.3 无需改动但需关注

- `EditorLayer` 的注册顺序：`ComponentRegistry::RegisterAll()` 必须先于 `ComponentContextMenuRegistry::RegisterBuiltins()`，因为后者会引用 `desc.Remove / desc.CanRemove`（虽然目前实现里通用项只在绘制期读 Descriptor，注册期没有硬依赖，但为一致性保留此顺序）。
- `UI::IconMenuItem(icon, label, disabled=false)` 与 `UI::BeginPopup / EndPopup` 已封装，可直接复用；[Widgets.h](../../Lucky/Source/Lucky/UI/Widgets.h)。

---

## 3. 现状分析

### 3.1 现状代码

`InspectorPanel::DrawComponentHeader` 中 Settings 弹窗片段（现状）：

```cpp
// 移除组件
bool componentRemoved = false;
if (UI::BeginPopup("ComponentSettings"))
{
    if (desc.CanRemove && desc.Remove)
    {
        if (ImGui::MenuItem("Remove Component"))
        {
            componentRemoved = true;
        }
    }

    UI::EndPopup();
}
```

### 3.2 问题清单

| # | 问题 | 影响 |
|---|------|------|
| P1 | 每加一个通用菜单项都必须改 `ComponentDescriptor` + `InspectorPanel` 两处 | 违反"数据驱动 UI"，与 `ComponentAddMenuItem` 已列表化的风格不一致 |
| P2 | 无法表达"组件私有项"（如脚本的 `Edit Script`） | Phase 1 `ScriptComponent` 落地时无处安放 |
| P3 | 无法表达"运行期启用/禁用"（如 Paste 项需剪贴板兼容） | 未来 Copy/Paste 无法接入 |
| P4 | 弹窗 ID `"ComponentSettings"` 未按组件区分 | 两个组件同时打开时 ImGui 状态串扰（bug） |
| P5 | Remove 行为绑死为菜单项 `Remove Component`，缺乏统一渲染入口 | 未来加图标、加分隔线、加快捷键提示时无处扩展 |

---

## 4. 数据模型设计

### 4.1 `ComponentContextMenuItem` 结构体

统一表达 Settings 弹窗中的一个菜单条目（通用与私有共用同一结构）。

```cpp
namespace Lucky
{
    class Entity;
    class Texture2D;
    struct ComponentDescriptor;

    /// <summary>
    /// 组件设置弹窗（齿轮按钮）中的一项菜单
    /// 通用项与组件私有项复用同一结构
    /// </summary>
    struct ComponentContextMenuItem
    {
        std::string Label;                                                                              // 菜单显示名（如 "Remove Component"、"Edit Script"）
        std::function<const Ref<Texture2D>&()> GetIcon;                                                 // 菜单项图标；nullptr 表示无图标
        std::function<bool(Entity, const ComponentDescriptor&)> IsVisible;                              // 运行期可见性判定；nullptr 视为始终可见
        std::function<bool(Entity, const ComponentDescriptor&)> IsEnabled;                              // 运行期启用性判定；nullptr 视为始终启用
        std::function<void(Entity, const ComponentDescriptor&)> Execute;                                // 点击回调（必填）
        bool SeparatorAfter = false;                                                                    // 在此项之后画一条分隔线
    };
}
```

设计要点：

1. `IsVisible` / `IsEnabled` / `Execute` 的形参统一为 `(Entity, const ComponentDescriptor&)`：
   - 通用项需要 `desc` 才能通过 `desc.Remove(entity)` / `desc.CanRemove` 等复用 Descriptor 已有能力；
   - 私有项也会需要 `desc` 才能拿到 `desc.Name`（例如 `"Remove {} Component"` 这种拼接）；
   - 统一形参可以让通用/私有项共用同一 `DrawItem` 函数，避免两条分支。
2. `IsVisible` 与 `IsEnabled` 分离（不合并为一个 `IsActive`）：
   - Unity 一致：不满足条件时项本身依然可见但灰化，才符合用户对 Copy/Paste 一直存在的心智模型；
   - `IsVisible` 仅用于"根本不应该出现"的场景（如 Remove 对 `CanRemove == false` 的组件）。
3. `SeparatorAfter` 而非 `SeparatorBefore`：便于在组装列表时"最后一项之后要不要分隔"的语义与"是否还有下一段"无关，简化通用段与私有段拼接逻辑。

### 4.2 `ComponentDescriptor` 新增字段

在 [ComponentDescriptor.h](../../Lucky/Source/Lucky/Scene/ComponentDescriptor.h) 中追加：

```cpp
// ---- 组件私有 Settings 菜单项 ----

std::vector<ComponentContextMenuItem> ExtraContextMenuItems;                                            // 组件独有的 Settings 弹窗菜单项（如脚本的 Edit Script）；空表示无
```

现有 `bool CanRemove = true;` 与 `RemoveFn Remove;` **完全保留**：它们的语义仍然清晰（"该组件是否允许移除"以及"如何移除"），并作为"通用 Remove 项"的**数据来源**被通用段的 `IsVisible / Execute` 读取。

### 4.3 `ComponentContextMenuRegistry` 静态类

集中管理通用菜单项（跨所有组件）。

```cpp
namespace Lucky
{
    /// <summary>
    /// 组件设置弹窗的通用菜单项注册表
    /// 
    /// 生命周期：EditorLayer::OnAttach 中调用 RegisterBuiltins() 完成一次性注册；OnDetach 中调用 Clear()
    /// 遍历顺序：与注册顺序一致（即项在弹窗中的显示顺序）
    /// </summary>
    class ComponentContextMenuRegistry
    {
    public:
        /// <summary>
        /// 注册所有内置通用菜单项（Remove Component 等）
        /// </summary>
        static void RegisterBuiltins();

        /// <summary>
        /// 清空注册表
        /// </summary>
        static void Clear();

        /// <summary>
        /// 追加一项通用菜单项到末尾
        /// 供未来其他子系统（Prefab / Undo / Clipboard 等）注入自己的通用项
        /// </summary>
        static void Register(ComponentContextMenuItem item);

        /// <summary>
        /// 按注册顺序返回全部通用项
        /// </summary>
        static const std::vector<ComponentContextMenuItem>& All();
    };
}
```

`All()` 返回 `const std::vector<...>&`（与 `ComponentRegistry::All()` 保持相同接口风格）。

---

## 5. 关键决策点与方案对比

### 5.1 【决策 A】通用项存放位置

**问题**：`Reset / Copy / Paste / Remove` 这类每个组件都出现的菜单项，代码应放在哪？

| 方案 | 描述 | 优点 | 缺点 | 优先级 |
|------|------|------|------|:-:|
| **A1**（推荐）| 独立静态类 `ComponentContextMenuRegistry` | 与 `ComponentRegistry` 正交；未来 Prefab/Undo/Clipboard 等模块可**由自身模块**在 Init 时注入通用项，不用改 UI 层；接口风格与 `ComponentRegistry` 一致 | 多了一个类 + 一个新文件；调用方要多认识一个 API | **P0** |
| **A2** | 写在 `InspectorPanel` 内的 `static const std::vector<ComponentContextMenuItem>` | 无新增文件，最简单 | UI 层承担了"通用项定义"职责；跨模块扩展必须改 UI；不符合项目"注册表化"整体走向 | P2 |
| **A3** | 塞进 `ComponentRegistry`（作为额外的全局字段） | 只加一个类的接口 | 与 `ComponentRegistry`"每组件一个 Descriptor"的语义冲突（通用项不属于任何单个组件）；容易与 `desc.ExtraContextMenuItems` 混淆 | P3 |

**推荐 A1，理由**：

1. 通用项在概念上就不属于任何单个组件，塞进 `ComponentRegistry` 语义拧巴（A3 淘汰）；
2. UI 层只应负责**绘制**，不应成为"通用业务规则的定义地"（A2 淘汰）；
3. 未来 Prefab / Undo / Clipboard 等模块想扩展通用项时，A1 允许它们在自己的模块 Init 中调用 `ComponentContextMenuRegistry::Register(...)`，实现真正的模块间解耦。

### 5.2 【决策 B】菜单项状态回调的形参

**问题**：`IsVisible / IsEnabled / Execute` 只传 `Entity`，还是 `(Entity, const ComponentDescriptor&)`？

| 方案 | 描述 | 优点 | 缺点 | 优先级 |
|------|------|------|------|:-:|
| **B1**（推荐）| `(Entity, const ComponentDescriptor&)` | 通用项能读 `desc.CanRemove / desc.Remove / desc.Name`；通用与私有段可共用同一 `DrawItem` 函数 | 单参数私有项调用时多一个未使用形参 | **P0** |
| **B2** | 仅 `(Entity)` | 签名最短 | 通用项无法通过 Descriptor 复用能力（例如 Remove 项内部无法直接调 `desc.Remove(entity)`，必须通过闭包捕获，而"通用项一次注册所有组件共用"的场景下没有 `desc` 可捕获）；private 项想拿组件名做拼接文本时只能自己 `ComponentRegistry::GetByType()` 反查 | P3 |
| **B3** | `(Entity, ComponentType)` | 比 B2 多一层类型信息 | Descriptor 里其他字段仍然拿不到，还要 `GetByType` 反查一次 | P3 |

**推荐 B1，理由**：本文档采用 A1 方案的通用注册表模型，通用项的回调运行时才拿到"当前正在渲染的哪个 Descriptor"，形参必须提供 `desc` 才能内部调用 `desc.Remove(entity)`；如果仅传 `Entity`，通用 Remove 项无法在不额外查表的情况下工作。

### 5.3 【决策 C】通用项与私有项之间的分隔线

**问题**：通用段绘制完后，如果 `ExtraContextMenuItems` 非空，是否强制在两段之间插一条分隔线？

| 方案 | 描述 | 优点 | 缺点 | 优先级 |
|------|------|------|------|:-:|
| **C1**（推荐）| `InspectorPanel` 在两段之间自动插分隔线（仅当两段都非空） | 视觉上"通用/私有"边界清晰；调用方无需感知 | 若某私有项希望紧贴通用段（无分隔），无法表达 | **P0** |
| **C2** | 完全由 `SeparatorAfter` 手动控制 | 100% 灵活 | 每个组件的最后一个通用项 / 首个私有项都要显式设置，容易漏 | P2 |
| **C3** | 通用/私有共用一条列表，注册者自决分隔 | 数据模型最扁平 | 通用与私有的语义在数据层被抹平，未来做"隐藏私有段但保留通用段"这类需求（如运行模式下）代价高 | P3 |

**推荐 C1，理由**：通用/私有的分层是设计意图，UI 层应体现这份意图；单项级别的 `SeparatorAfter` 依然保留，用于同段内部的分组（例如通用段内 `Reset` 与 `Copy` 之间加分隔）。

### 5.4 【决策 D】`RegisterInspector` 签名是否新增参数

**问题**：`ExtraContextMenuItems` 字段增加后，`ComponentRegistry::RegisterInspector` 的形参是否也追加一个 `extraContextMenuItems`？

| 方案 | 描述 | 优点 | 缺点 | 优先级 |
|------|------|------|------|:-:|
| **D1**（推荐）| 在 `RegisterInspector` 末尾追加 `std::vector<ComponentContextMenuItem> extraContextMenuItems = {}`（带默认空值） | 所有 Inspector 元信息在一处填齐，风格与 `AddMenuItems` 参数一致；默认空值让现有 9 处调用点**只需补一个 `{}`** 或依赖默认值 | 参数数增至 8 个，签名较长 | **P0** |
| **D2** | 保留 `RegisterInspector` 不变，新增独立函数 `RegisterContextMenuItems(type, items)` | 单函数签名短 | Inspector 元信息被拆到两次调用，注册代码更?嗦；容易出现"忘了调用第二个"的漏注册 | P2 |
| **D3** | 直接暴露 `ComponentDescriptor` 让注册者写完再 push | 极致灵活 | 破坏 `ComponentRegistry` 已建立的"分三段注册"结构 | P3 |

**推荐 D1，理由**：`AddMenuItems` 已经是列表化参数并跟在 `RegisterInspector` 中，追加 `ExtraContextMenuItems` 是完全对称的自然扩展；且带默认空值时，9 处已有调用点的迁移压力为零（甚至可以不改）。

### 5.5 【决策 E】弹窗 ID 修复方式

**问题**：现状 `"ComponentSettings"` 全局唯一 ID，多个组件间会串扰。

| 方案 | 描述 | 优点 | 缺点 | 优先级 |
|------|------|------|------|:-:|
| **E1**（推荐）| `std::format("ComponentSettings##{}", static_cast<int>(desc.Type))` | 与已有 TreeNode ID 生成风格一致（见 `strComponentID`）；同一实体的同一组件同一时刻只可能打开一个，ID 只需按 `Type` 区分 | 需要 `<format>` 头（项目已在用） | **P0** |
| **E2** | `std::format("ComponentSettings##{}{}", entityUUID, typeInt)` | 严格唯一到实体+组件 | Inspector 一次只能显示一个实体的组件，无必要区分到 UUID | P2 |

**推荐 E1**。

---

## 6. 弹窗绘制流程

### 6.1 时序（伪流程）

```
DrawComponentHeader(entity, desc)
├─ 绘制外壳（HorizontalLine + TreeNode + 图标 + Name）
├─ 齿轮按钮：ImGui::OpenPopup("ComponentSettings##<TypeInt>")
├─ if (UI::BeginPopup("ComponentSettings##<TypeInt>"))
│    │
│    ├─ // ---- 通用段 ----
│    ├─ for item in ComponentContextMenuRegistry::All():
│    │     DrawContextMenuItem(item, entity, desc)
│    │
│    ├─ // ---- 段间分隔（决策 C1） ----
│    ├─ if 通用段有可见项 && 私有段非空:
│    │     ImGui::Separator()
│    │
│    ├─ // ---- 私有段 ----
│    ├─ for item in desc.ExtraContextMenuItems:
│    │     DrawContextMenuItem(item, entity, desc)
│    │
│    └─ UI::EndPopup()
│
└─ if opened: desc.Draw(entity)
```

### 6.2 `DrawContextMenuItem` 实现（放 `InspectorPanel.cpp` 内匿名命名空间）

```cpp
namespace
{
    /// <summary>
    /// 绘制单个 Settings 菜单项：处理可见性、启用性、图标、点击回调、分隔线
    /// </summary>
    void DrawContextMenuItem(const ComponentContextMenuItem& item, Entity entity, const ComponentDescriptor& desc)
    {
        if (item.IsVisible && !item.IsVisible(entity, desc))
        {
            return;
        }

        bool enabled = !item.IsEnabled || item.IsEnabled(entity, desc);

        const Ref<Texture2D>& icon = item.GetIcon ? item.GetIcon() : Ref<Texture2D>{};

        bool clicked = false;
        if (icon)
        {
            clicked = UI::IconMenuItem(icon, item.Label.c_str(), !enabled);
        }
        else
        {
            if (!enabled)
            {
                ImGui::BeginDisabled();
            }
            clicked = ImGui::MenuItem(item.Label.c_str());
            if (!enabled)
            {
                ImGui::EndDisabled();
            }
        }

        if (clicked && enabled && item.Execute)
        {
            item.Execute(entity, desc);
        }

        if (item.SeparatorAfter)
        {
            ImGui::Separator();
        }
    }

    /// <summary>
    /// 判定通用段内是否至少存在一个可见项（用于决定是否需要段间分隔线）
    /// </summary>
    bool HasAnyVisibleBuiltin(Entity entity, const ComponentDescriptor& desc)
    {
        for (const ComponentContextMenuItem& item : ComponentContextMenuRegistry::All())
        {
            if (!item.IsVisible || item.IsVisible(entity, desc))
            {
                return true;
            }
        }
        return false;
    }
}
```

> 备注：`UI::IconMenuItem` 的第三个参数就是 `disabled`，已经原生支持灰化，见 [Widgets.h](../../Lucky/Source/Lucky/UI/Widgets.h) 第 126 行。

### 6.3 `DrawComponentHeader` 中弹窗片段（改造后）

```cpp
// 生成按组件类型区分的弹窗 ID，避免多个组件间的 ID 串扰（决策 E1）
const std::string popupID = std::format("ComponentSettings##{}", static_cast<int>(desc.Type));

// 齿轮按钮部分：将原有的 OpenPopup("ComponentSettings") 替换为 OpenPopup(popupID.c_str())
if (UI::ImageButtonFlipped(settingsIcon, ImVec2(lineHeight, lineHeight), 0))
{
    ImGui::OpenPopup(popupID.c_str());
}

// ...（外壳其余代码保持不变）

// Settings 弹窗：通用段 + 私有段
if (UI::BeginPopup(popupID.c_str()))
{
    // ---- 通用段 ----
    for (const ComponentContextMenuItem& item : ComponentContextMenuRegistry::All())
    {
        DrawContextMenuItem(item, entity, desc);
    }

    // ---- 段间分隔（决策 C1） ----
    if (!desc.ExtraContextMenuItems.empty() && HasAnyVisibleBuiltin(entity, desc))
    {
        ImGui::Separator();
    }

    // ---- 私有段 ----
    for (const ComponentContextMenuItem& item : desc.ExtraContextMenuItems)
    {
        DrawContextMenuItem(item, entity, desc);
    }

    UI::EndPopup();
}

// 注意：原本用 `bool componentRemoved` 记录是否点击了 Remove 再在 Popup 结束后统一执行的模式
//       在新架构下，`Execute` 回调直接在 Popup 内被调用即可 ?? Remove 会走 desc.Remove(entity)，
//       与原有行为等价，无需额外的"延后执行"分支
```

---

## 7. 内置通用项：`Remove Component`

`ComponentContextMenuRegistry::RegisterBuiltins()` 中注册的第一项即 Remove，用于**平迁**现有能力。

```cpp
void ComponentContextMenuRegistry::RegisterBuiltins()
{
    // ---- Remove Component ----
    ComponentContextMenuItem removeItem;
    removeItem.Label = "Remove Component";
    removeItem.IsVisible = [](Entity, const ComponentDescriptor& desc)
    {
        return desc.CanRemove && desc.Remove != nullptr;
    };
    removeItem.Execute = [](Entity entity, const ComponentDescriptor& desc)
    {
        desc.Remove(entity);
    };
    Register(std::move(removeItem));
}
```

关键点：

1. `IsVisible` 复用现有 `CanRemove` + `Remove != nullptr` 判定，语义与旧代码等价；
2. `Execute` 内直接调 `desc.Remove(entity)`；**无需**引入"延迟到 Popup 结束再执行"的中间 bool，因为 Remove 后续没有绘制该组件的代码路径（Popup 结束后立刻 `if (opened) desc.Draw(entity);` 但 `desc` 已由 `RegisterInspector` 期决定的引用，实际当前一帧仍会继续绘制"已被 Remove 的组件"）；

   > **? 注意事项**：上述最后一句其实指向一个隐藏坑 ?? 当前旧代码之所以用 `componentRemoved` 变量是为了**先关闭 Popup 再 Remove**，避免 Popup 绘制到已删除的组件 header 上。**新架构必须保留这个语义**。因此实际实现里，`Execute` 内不应直接调 `desc.Remove`，而应通过一个"延后执行"机制。见 §7.1。

### 7.1 【决策 F】"删除组件"的即时/延后执行

**问题**：Remove 一旦执行，`desc.Draw(entity)` 会访问不存在的组件，产生 UB。

| 方案 | 描述 | 优点 | 缺点 | 优先级 |
|------|------|------|------|:-:|
| **F1**（推荐）| `DrawComponentHeader` 内保留一个 `bool pendingRemove = false;`，Remove 项的 `Execute` 通过 lambda 捕获这个 bool 并置位，Popup 结束后统一处理 | 与现有 `componentRemoved` 模式行为完全一致，零回归 | 需要在通用项注册时"知道有这么个 bool"?? 与"通用项 = 全局注册"矛盾 | P2 |
| **F2**（推荐）| 引入极轻量的"帧末延后动作"：`InspectorPanel` 内一个 `std::vector<std::function<void()>> m_PendingComponentActions;`，通用/私有项的 `Execute` 内 `push` 一个 lambda；`DrawComponents` 末尾统一 flush | 通用项定义完全无侵入，任何 `Execute` 都可以安全"修改 Entity 组件集合"而不担心迭代失效；未来 Add/Reorder 组件同样受益 | 引入了一个新的状态字段 | **P0** |
| **F3** | 让 `Execute` 直接调 Remove，然后立即 `return` 出 `DrawComponentHeader`（配合返回值改成 `bool`） | 无额外状态 | 需要修改函数签名并让上层循环感知返回值；未来若多个组件同帧删除更麻烦 | P3 |

**推荐 F2，理由**：

1. Popup / Execute / 帧内组件集合修改是一个反复出现的问题（未来 Copy/Paste/Reset 也会遇到），一次性建立"帧末延后动作队列"是最干净的通解；
2. 通用项注册端**完全无感知**，符合决策 A/B 的解耦目标。

按 F2 落地时，`DrawContextMenuItem` 与 `Execute` 保持 §6.2 中的形态；`InspectorPanel` 内部：

```cpp
// InspectorPanel.h（private 段）
std::vector<std::function<void()>> m_PendingComponentActions;

// InspectorPanel.cpp DrawComponents 末尾
for (const auto& action : m_PendingComponentActions)
{
    action();
}
m_PendingComponentActions.clear();
```

Remove 通用项的 `Execute` 改为：

```cpp
removeItem.Execute = [this](Entity entity, const ComponentDescriptor& desc)
{
    m_PendingComponentActions.emplace_back([entity, remove = desc.Remove]()
    {
        remove(entity);
    });
};
```

但注意 ?? 通用项是**全局注册**，没有 `this` 可捕获。所以需要一个"能被通用项访问的延后队列入口"：

- **F2a**（推荐）：在 `ComponentContextMenuRegistry` 中新增静态方法 `EnqueueDeferredAction(std::function<void()> action)` 和 `FlushDeferredActions()`。`InspectorPanel::DrawComponents` 末尾调 `Flush`。
- **F2b**：改 `Execute` 签名为 `(Entity, const ComponentDescriptor&, DeferredActionQueue&)`，`DrawComponentHeader` 传入本地队列。签名更长，但依赖显式。

推荐 **F2a**，签名最简；`ComponentContextMenuRegistry` 反正是"编辑器 UI 域"的静态类，承载一个进程内延后队列是合适的。

最终 Remove 项：

```cpp
removeItem.Execute = [](Entity entity, const ComponentDescriptor& desc)
{
    ComponentContextMenuRegistry::EnqueueDeferredAction([entity, remove = desc.Remove]()
    {
        remove(entity);
    });
};
```

---

## 8. 私有段示例：脚本组件的 `Edit Script`

**注**：本文档不新增 `ScriptComponent`，此处仅示例"未来接入方式"，供 Phase 1 参考，无需在本 Phase 落地。

```cpp
// 未来 ScriptComponent 的 RegisterInspector 调用中：
{
    ComponentContextMenuItem editScriptItem;
    editScriptItem.Label = "Edit Script";
    editScriptItem.IsEnabled = [](Entity e, const ComponentDescriptor&)
    {
        const ScriptComponent& sc = e.GetComponent<ScriptComponent>();
        return sc.SourceAsset.IsValid();
    };
    editScriptItem.Execute = [](Entity e, const ComponentDescriptor&)
    {
        const ScriptComponent& sc = e.GetComponent<ScriptComponent>();
        ScriptEditorLauncher::Open(sc.SourceAsset);
    };

    std::vector<ComponentContextMenuItem> scriptExtras = { std::move(editScriptItem) };

    RegisterInspector(ComponentType::Script,
        &Draw_Script,
        &DefaultIcon<ScriptComponent>,
        { /* AddMenuItems */ },
        [](Entity e) { e.RemoveComponent<ScriptComponent>(); },
        /* showInHierarchyIcons = */ true,
        /* canRemove = */ true,
        std::move(scriptExtras));
}
```

其他组件不需要写任何私有项代码 ?? 默认参数 `{}` 即空。

---

## 9. 代码规范落实要点

以下约束必须在实现代码时严格遵守，来源于 [Coding_Style_Guide.md](../Coding_Style_Guide.md)：

1. **花括号**：所有 `if / for / while` 一律加花括号，即使单条语句（例：`if (icon) { UI::IconMenuItem(...); }`，禁止 `if (icon) UI::IconMenuItem(...);`）。
2. **命名**：
   - 结构体/类：`ComponentContextMenuItem` / `ComponentContextMenuRegistry`（PascalCase）；
   - 公有成员：`Label / GetIcon / IsVisible / IsEnabled / Execute / SeparatorAfter`（PascalCase）；
   - 静态方法：`RegisterBuiltins / Register / All / Clear / EnqueueDeferredAction / FlushDeferredActions`；
   - 私有成员（若有）：`m_XxxYyy`。
3. **注释**：
   - 公有接口使用 `/// <summary>` XML 注释；
   - 结构体字段行内使用 `//`，字段之后加**大量空格**让 `//` 纵向对齐（对齐现有 `ComponentDescriptor` 字段风格）；
   - **禁止**写"本 Phase 会补齐"、"P1 会加"等阶段性注释；
   - **禁止**写"分层原则：本类只做绘制"这类文档式说明性长注释。
4. **智能指针**：`Ref<Texture2D>` / `Ref<Scene>`，禁止 `std::shared_ptr`；创建用 `CreateRef<T>()`。
5. **`auto` 使用**：函数返回智能指针引用等场景显式写类型；仅在迭代器、`for (const auto& x : container)`、右侧类型显然（`CreateRef<T>()`）时用 `auto`。本文档 §6/§7 示例已遵循。
6. **`#pragma once`** + 项目内 include 顺序（PCH 只在 `.cpp` 首行）。
7. **前向声明**：`ComponentDescriptor.h` 内 `class Entity; class Texture2D;` 保留现状，只追加 `ComponentContextMenuItem` 结构体本身（结构体内部使用 `std::function<...(Entity, const ComponentDescriptor&)>`，`ComponentDescriptor` 在同一文件后续定义，需要前向声明或调整顺序 ?? 见 §11.1 落地顺序）。

---

## 10. 逐步实施步骤

严格按下列顺序执行；每一步都可独立编译通过。

### Step 1：`ComponentDescriptor.h` 扩展

1. 在文件顶部（`ComponentAddMenuItem` 结构体之后、`ComponentDescriptor` 之前）**前向声明** `ComponentDescriptor`：

   ```cpp
   struct ComponentDescriptor;
   ```

2. 新增 `ComponentContextMenuItem` 结构体（内容见 §4.1）。
3. 在 `ComponentDescriptor` 尾部 `bool CanRemove = true;` 之后追加：

   ```cpp
   std::vector<ComponentContextMenuItem> ExtraContextMenuItems;                                        // 组件独有的 Settings 弹窗菜单项（空表示无）
   ```

### Step 2：新建 `ComponentContextMenuRegistry.h / .cpp`

按 §4.3 与 §7 落地接口与内置注册。文件路径：

- `Lucky/Source/Lucky/Editor/ComponentContextMenuRegistry.h`
- `Lucky/Source/Lucky/Editor/ComponentContextMenuRegistry.cpp`

`.cpp` 内使用 `.h` 中未展示的 `EnqueueDeferredAction / FlushDeferredActions / All / Register / Clear` 全部实现，内部用两个 `static std::vector` 分别承载"通用项列表"和"延后动作队列"。

### Step 3：`ComponentRegistry` 签名扩展（决策 D1）

- `ComponentRegistry.h` 的 `RegisterInspector` 追加最后一个参数 `std::vector<ComponentContextMenuItem> extraContextMenuItems = {}`；
- `ComponentRegistry.cpp` 中赋值 `desc.ExtraContextMenuItems = std::move(extraContextMenuItems);`；
- `ComponentInspectors.cpp` 内 9 处 `RegisterInspector(...)` 调用**无需改动**（利用默认参数值）。

### Step 4：`InspectorPanel.cpp` 弹窗改造

1. 在 `.cpp` 顶部 include：
   ```cpp
   #include "Lucky/Editor/ComponentContextMenuRegistry.h"
   ```
2. 匿名命名空间中添加 §6.2 的 `DrawContextMenuItem` 与 `HasAnyVisibleBuiltin`；
3. `DrawComponentHeader` 内齿轮按钮的 `OpenPopup("ComponentSettings")` 替换为 §6.3 中 `popupID`；
4. 现有 `bool componentRemoved` / 弹窗内 `if (desc.CanRemove && desc.Remove)` 等硬编码替换为 §6.3 完整的通用段+私有段循环；
5. **DrawComponents 末尾**新增：
   ```cpp
   ComponentContextMenuRegistry::FlushDeferredActions();
   ```

### Step 5：`EditorLayer.cpp` 生命周期挂接

- `OnAttach`：在 `ComponentRegistry::RegisterAll();` 下一行追加：
  ```cpp
  ComponentContextMenuRegistry::RegisterBuiltins();
  ```
- `OnDetach`：在 `ComponentRegistry::Clear();` 上一行/下一行追加：
  ```cpp
  ComponentContextMenuRegistry::Clear();
  ```

### Step 6：构建与目录挂接

- 若项目使用 Premake（见 [Build-Lucky.lua](../../Lucky/Build-Lucky.lua)）按通配符自动纳入 `Lucky/Source/**.h / **.cpp`，则无需修改；否则同步 `.vcxproj` / `.vcxproj.filters`。

### Step 7：手工验证

1. 打开 Inspector，任意组件（如 `Transform`）点齿轮：应只显示 `Remove Component`，且 `Transform` 的 `CanRemove == false` 会自动隐藏该项 → 空弹窗（符合预期）；
2. `MeshRenderer` 点齿轮：显示 `Remove Component`，点击后组件被移除，Inspector 无崩溃；
3. 打开两个不同实体切换过程中，先在 A 组件上打开 Settings，切到 B 实体：ImGui 会自动关闭 Popup；
4. 若在 A 实体上依次点开两个不同组件的齿轮：不再有"两个 Popup 同名 ID"导致的状态串扰（决策 E1 验证）。

---

## 11. 落地注意事项

### 11.1 前向声明与包含循环

`ComponentContextMenuItem::IsVisible / IsEnabled / Execute` 引用 `const ComponentDescriptor&`，而 `ComponentDescriptor` 内又持有 `std::vector<ComponentContextMenuItem>`。两者在同一头文件中：

- 前向声明 `struct ComponentDescriptor;` 放在 `ComponentContextMenuItem` 之前；
- `ComponentContextMenuItem` 结构体只在 `std::function<...(Entity, const ComponentDescriptor&)>` 内**通过引用**使用 `ComponentDescriptor`，前向声明即可编译。

### 11.2 `EnqueueDeferredAction` 的执行时机

- 必须在 `InspectorPanel::DrawComponents` **末尾**、即所有 `DrawComponentHeader` 完成之后调用 `Flush`；
- 不可以在 `DrawComponentHeader` 内部就 `Flush`，否则 Remove 一个组件之后本轮 `ForEach` 循环仍在遍历 Descriptor 列表，虽然不影响遍历本身（Registry 结构不变），但会导致后续组件绘制看到"已被 Remove 的组件"的旧状态；
- 每帧一次 `Flush` 是安全的：即便队列在同一帧内被多次 push（例如极端情况下批量删除），都在 `DrawComponents` 末尾一次性执行。

### 11.3 通用项与 `ScopedStyle` 的作用域

Settings 弹窗现有代码在齿轮按钮周围包了若干 `UI::ScopedStyle` / `UI::ScopedColor`（`ImGuiCol_Button` 透明化等）?? 这些**只作用于按钮本身**，`BeginPopup` 已经在这些 Scoped 作用域之外，不受影响；改造时保持原有作用域范围不变即可。

### 11.4 与未来 Undo/Redo 的接口预留

`Execute` 直接对 Entity 做变更（如 `desc.Remove(entity)`）目前未接入 Undo 栈。当未来引入 `EditorAction` 时：

- 通用 Remove 项的 `Execute` 内部改为 `EditorActionQueue::Push(RemoveComponentAction{entity, desc.Type});`；
- `ComponentContextMenuItem` 本身**不需要改动**；

因此本设计对 Undo 是**扩展开放**的，无需在本 Phase 提前规划。

---

## 12. 验收清单

- [ ] `ComponentContextMenuItem` 结构体新增，字段命名与注释对齐既有 `ComponentAddMenuItem` 风格；
- [ ] `ComponentDescriptor` 新增 `ExtraContextMenuItems` 字段；`CanRemove` / `Remove` 保留不动；
- [ ] `ComponentContextMenuRegistry` 完整落地，`RegisterBuiltins` 中至少注册一项 `Remove Component`；
- [ ] `RegisterInspector` 签名追加带默认空值的 `extraContextMenuItems` 参数，现有 9 处调用点不改仍能编译；
- [ ] `InspectorPanel` 弹窗改为"通用段 + 段间分隔（自动） + 私有段"；
- [ ] 弹窗 ID 使用 `ComponentSettings##<TypeInt>`；
- [ ] `EditorLayer::OnAttach / OnDetach` 挂接 `RegisterBuiltins / Clear`；
- [ ] `InspectorPanel::DrawComponents` 末尾调用 `FlushDeferredActions`；
- [ ] 现有所有组件 Inspector 行为无回归（Transform/Relationship 不出现 Remove 项、其他组件正常 Remove）；
- [ ] 同一实体切换不同组件齿轮时 ImGui 弹窗状态不串扰。
