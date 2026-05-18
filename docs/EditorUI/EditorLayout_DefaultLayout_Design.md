# 编辑器默认布局系统设计文档

> **版本**：1.1  
> **创建日期**：2026-05-18  
> **最后更新**：2026-05-18（5.1 布局逻辑放置位置改为方案 B：独立 EditorLayoutManager 类）  
> **前置依赖**：EditorDockSpace、PanelManager、EditorPanel 基类  
> **方案**：方案一 —— 基于 ImGui DockBuilder API 的程序化布局（仅预设布局，暂不支持自定义保存）

---

## 一、目标

实现类似 Unity 的 **Window → Layouts → Default** 功能，在编辑器中提供程序化的默认布局：

- 首次启动（无 `imgui.ini` 缓存）时自动应用默认布局
- 用户可通过菜单 **Window → Layouts → Default** 随时重置为默认布局
- 架构设计需为未来扩展预留空间（2 by 3、Wide 等预设布局，以及自定义布局保存/加载）

---

## 二、当前架构概述

### 2.1 调用时序

```
EditorLayer::OnImGuiRender()
  → m_EditorDockSpace.ImGuiRender()    // 创建 DockSpace 容器
  → UI_DrawMenuBar()                    // 绘制菜单栏
  → m_PanelManager->OnImGuiRender()    // 渲染所有面板（内部调用 ImGui::Begin/End）
```

### 2.2 EditorDockSpace 现状

```cpp
// EditorDockSpace.cpp
void EditorDockSpace::ImGuiRender()
{
    // ... 设置全屏窗口样式 ...
    ImGui::Begin("DockSpace", nullptr, m_WindowFlags);
    {
        // ...
        ImGui::DockSpace(ImGui::GetID("EditorDockSpace"), ImVec2(0.0f, 0.0f),
                         m_Flags | ImGuiDockNodeFlags_NoWindowMenuButton | ImGuiDockNodeFlags_NoCloseButton);
    }
    ImGui::End();
}
```

**关键信息**：DockSpace 的 ID 为 `ImGui::GetID("EditorDockSpace")`。

### 2.3 面板注册方式（EditorLayer::OnAttach）

```cpp
m_PanelManager->AddPanel<SceneHierarchyPanel>(SCENE_HIERARCHY_PANEL_ID, "Hierarchy", true, m_Scene);
m_PanelManager->AddPanel<SceneViewportPanel>(SCENE_VIEWPORT_PANEL_ID, "Scene", true, m_Scene);
m_PanelManager->AddPanel<InspectorPanel>(INSPECTOR_PANEL_ID, "Inspector", true, m_Scene);
m_PanelManager->AddPanel<ProjectAssetsPanel>(PROJECT_ASSETS_PANEL_ID, "Project", true);
m_PanelManager->AddPanel<RenderPipelinePanel>(RENDER_PIPELINE_PANEL_ID, "Render Pipeline", true);
m_PanelManager->AddPanel<PreferencesPanel>(PREFERENCES_PANEL_ID, "Preferences", false);
m_PanelManager->AddPanel<LightingPanel>(LIGHTING_PANEL_ID, "Lighting", false, m_Scene);
```

### 2.4 面板窗口名称映射

`DockBuilderDockWindow()` 的第一个参数是窗口名称，必须与 `ImGui::Begin(name)` 中的 `name` 完全一致。在本项目中，该名称来自 `PanelData::Name`（即 `AddPanel` 的第二个参数 `displayName`）。

| PanelData::Name | 面板类 | 默认打开 | 说明 |
|-----------------|--------|----------|------|
| `"Hierarchy"` | SceneHierarchyPanel | ? | 场景层级 |
| `"Scene"` | SceneViewportPanel | ? | 场景视口 |
| `"Inspector"` | InspectorPanel | ? | 检查器 |
| `"Project"` | ProjectAssetsPanel | ? | 项目资产 |
| `"Render Pipeline"` | RenderPipelinePanel | ? | 渲染管线 |
| `"Preferences"` | PreferencesPanel | ? | 偏好设置 |
| `"Lighting"` | LightingPanel | ? | 光照设置 |

> **注意**：默认布局只需要停靠 `IsOpen = true` 的面板。`Preferences` 和 `Lighting` 默认不打开，不参与默认布局的停靠定义。

### 2.5 ImGui DockBuilder API 概览

```cpp
// imgui_internal.h 中声明的 DockBuilder 系列 API
void    DockBuilderDockWindow(const char* window_name, ImGuiID node_id);
ImGuiDockNode* DockBuilderGetNode(ImGuiID node_id);
ImGuiID DockBuilderAddNode(ImGuiID node_id = 0, ImGuiDockNodeFlags flags = 0);
void    DockBuilderRemoveNode(ImGuiID node_id);
void    DockBuilderRemoveNodeDockedWindows(ImGuiID node_id, bool clear_settings_refs = true);
void    DockBuilderRemoveNodeChildNodes(ImGuiID node_id);
void    DockBuilderSetNodePos(ImGuiID node_id, ImVec2 pos);
void    DockBuilderSetNodeSize(ImGuiID node_id, ImVec2 size);
ImGuiID DockBuilderSplitNode(ImGuiID node_id, ImGuiDir split_dir,
                              float size_ratio_for_node_at_dir,
                              ImGuiID* out_id_at_dir, ImGuiID* out_id_at_opposite_dir);
void    DockBuilderFinish(ImGuiID node_id);
```

**使用流程**：
1. `DockBuilderRemoveNode(dockspaceID)` — 清除旧布局
2. `DockBuilderAddNode(dockspaceID, ImGuiDockNodeFlags_DockSpace)` — 创建新的 DockSpace 节点
3. `DockBuilderSetNodeSize(dockspaceID, size)` — 设置节点大小
4. `DockBuilderSplitNode(...)` — 分割节点（可多次调用，构建树形布局）
5. `DockBuilderDockWindow(windowName, nodeID)` — 将窗口停靠到指定节点
6. `DockBuilderFinish(dockspaceID)` — 完成布局构建

---

## 三、涉及的文件清单

| 文件 | 操作 | 说明 |
|------|------|------|
| `Luck3DApp/Source/EditorLayoutManager.h` | **新建** | 布局管理器头文件 |
| `Luck3DApp/Source/EditorLayoutManager.cpp` | **新建** | 布局管理器实现 |
| `Luck3DApp/Source/EditorDockSpace.h` | **修改** | 新增布局相关接口，持有 EditorLayoutManager |
| `Luck3DApp/Source/EditorDockSpace.cpp` | **修改** | 集成布局管理器，处理首次启动和重置逻辑 |
| `Luck3DApp/Source/EditorLayer.cpp` | **修改** | 菜单集成 |
| `Luck3DApp/Source/EditorLayer.h` | **不修改** | 无需改动（EditorDockSpace 已是成员） |

---

## 四、默认布局定义

### 4.1 目标布局（类似 Unity Default）

```
┌─────────────────────────────────────────────────────────────┐
│                        Menu Bar                             │
├────────────┬──────────────────────────────┬─────────────────┤
│            │                              │                 │
│ Hierarchy  │        Scene (Viewport)      │   Inspector     │
│            │                              │                 │
│   (20%)    │          (55%)               │    (25%)        │
│            │                              │                 │
│            ├──────────────────────────────┤                 │
│            │   Project | Render Pipeline  │                 │
│            │          (30% 高度)          │                 │
├────────────┴──────────────────────────────┴─────────────────┤
```

### 4.2 分割步骤

```
Step 1: 从 DockSpace 根节点左侧分割出 Hierarchy（比例 0.2）
        dockLeft = Hierarchy
        dockRemaining = 剩余区域

Step 2: 从 dockRemaining 右侧分割出 Inspector（比例 0.25）
        dockRight = Inspector
        dockCenter = 中间区域

Step 3: 从 dockCenter 底部分割出 Bottom 区域（比例 0.3）
        dockBottom = Project + Render Pipeline（Tab 共享）
        dockViewport = Scene
```

分割树结构：

```
EditorDockSpace (root)
├── dockLeft (Hierarchy)                    [ImGuiDir_Left, 0.2]
└── dockRemaining
    ├── dockRight (Inspector)               [ImGuiDir_Right, 0.25]
    └── dockCenter
        ├── dockViewport (Scene)            [上方]
        └── dockBottom (Project, Render Pipeline)  [ImGuiDir_Down, 0.3]
```

---

## 五、方案设计

### 5.1 布局逻辑放置位置

#### 方案 A：在 `EditorDockSpace` 类中直接实现（? 推荐，最优）

将布局逻辑封装在 `EditorDockSpace` 内部，通过新增方法和标志位控制。

```cpp
// EditorDockSpace.h
class EditorDockSpace
{
public:
    EditorDockSpace(bool fullScreen = true);

    /// <summary>
    /// 绘制 DockSpace
    /// </summary>
    void ImGuiRender();

    /// <summary>
    /// 请求重置为默认布局（下一帧生效）
    /// </summary>
    void ResetToDefaultLayout();
private:
    /// <summary>
    /// 应用默认布局
    /// </summary>
    /// <param name="dockspaceID">DockSpace 节点 ID</param>
    void ApplyDefaultLayout(uint32_t dockspaceID);
private:
    bool m_IsFullScreen = true;     // 是否全屏
    uint32_t m_Flags;               // DockSpace 标志（ImGuiDockNodeFlags）
    uint32_t m_WindowFlags;         // DockSpace 窗口标志（ImGuiWindowFlags）
    bool m_ResetLayout = false;     // 是否需要重置布局
};
```

**优点**：
- 布局逻辑与 DockSpace 紧密相关，放在一起职责清晰
- 改动最小，仅修改 `EditorDockSpace` 一个类
- `EditorLayer` 只需调用 `m_EditorDockSpace.ResetToDefaultLayout()` 即可
- 当前阶段只有一个默认布局，无需引入额外的管理类

**缺点**：
- 未来布局增多时，`EditorDockSpace` 可能变得臃肿（但可以在那时再重构提取）

#### 方案 B：新建独立的 `EditorLayoutManager` 类（? 其次）

创建一个独立的布局管理器类，`EditorDockSpace` 持有它。

```cpp
// EditorLayoutManager.h（新建文件）
class EditorLayoutManager
{
public:
    EditorLayoutManager() = default;

    /// <summary>
    /// 应用默认布局
    /// </summary>
    /// <param name="dockspaceID">DockSpace 节点 ID</param>
    void ApplyDefaultLayout(uint32_t dockspaceID);
};

// EditorDockSpace.h（修改）
class EditorDockSpace
{
    // ...
private:
    EditorLayoutManager m_LayoutManager;
    bool m_ResetLayout = false;
};
```

**优点**：
- 职责分离更彻底，布局管理独立于 DockSpace
- 未来扩展多种布局时结构更清晰

**缺点**：
- 当前只有一个默认布局，引入额外类增加了不必要的复杂度
- 需要新建两个文件（.h/.cpp），改动范围更大
- 过度设计（YAGNI 原则）

#### 结论

使用**方案 B**。虽然当前只有一个默认布局，但考虑到未来明确会扩展多种布局（2 by 3、Wide 等）以及自定义布局保存/加载，提前将布局逻辑独立为 `EditorLayoutManager` 类，可以避免后续重构成本，且职责分离更清晰。

---

### 5.2 首次启动检测方式

#### 方案 A：检查 DockBuilder 节点是否存在（? 推荐，最优）

在 `ImGui::DockSpace()` 调用之后，检查 DockSpace 节点是否已经有子节点（即是否已有布局）。如果没有，说明是首次启动或布局被清除，自动应用默认布局。

```cpp
void EditorDockSpace::ImGuiRender()
{
    // ... 现有的窗口设置代码 ...

    ImGui::Begin("DockSpace", nullptr, m_WindowFlags);
    {
        // ... 现有的样式弹出代码 ...

        ImGuiIO& io = ImGui::GetIO();
        if (io.ConfigFlags & ImGuiConfigFlags_DockingEnable)
        {
            ImGuiID dockspaceID = ImGui::GetID("EditorDockSpace");
            ImGui::DockSpace(dockspaceID, ImVec2(0.0f, 0.0f),
                             m_Flags | ImGuiDockNodeFlags_NoWindowMenuButton | ImGuiDockNodeFlags_NoCloseButton);

            // 首次启动检测：节点无子节点说明没有已保存的布局
            ImGuiDockNode* node = ImGui::DockBuilderGetNode(dockspaceID);
            if (node == nullptr || !node->IsSplitNode())
            {
                ApplyDefaultLayout(dockspaceID);
            }

            // 用户请求重置布局
            if (m_ResetLayout)
            {
                ApplyDefaultLayout(dockspaceID);
                m_ResetLayout = false;
            }
        }
    }
    ImGui::End();
}
```

**优点**：
- 不依赖外部文件或标志位
- 自动适应各种情况（首次启动、imgui.ini 被删除等）
- 逻辑简洁，易于理解

**缺点**：
- `IsSplitNode()` 在极端情况下可能误判（如用户手动将所有面板合并到一个 Tab 中，此时节点不是 Split 但有布局）

#### 方案 B：使用静态标志位 + 首帧检测（? 其次）

使用一个 `m_FirstFrame` 标志位，在第一帧时应用默认布局。

```cpp
// EditorDockSpace.h
class EditorDockSpace
{
    // ...
private:
    bool m_FirstFrame = true;   // 是否是第一帧
};

// EditorDockSpace.cpp
void EditorDockSpace::ImGuiRender()
{
    // ...
    ImGuiID dockspaceID = ImGui::GetID("EditorDockSpace");
    ImGui::DockSpace(dockspaceID, ...);

    if (m_FirstFrame)
    {
        ApplyDefaultLayout(dockspaceID);
        m_FirstFrame = false;
    }

    if (m_ResetLayout)
    {
        ApplyDefaultLayout(dockspaceID);
        m_ResetLayout = false;
    }
    // ...
}
```

**优点**：
- 实现极其简单，逻辑清晰
- 不存在误判问题

**缺点**：
- **每次启动都会覆盖用户上次的布局**。即使用户手动调整了面板位置并通过 `imgui.ini` 保存了，下次启动仍会被重置为默认布局
- 这与 Unity 的行为不一致（Unity 会记住用户上次的布局，只有显式点击 Default 才重置）

#### 方案 C：检查 imgui.ini 文件是否存在（? 其次）

通过检查 `imgui.ini` 文件是否存在来判断是否是首次启动。

```cpp
void EditorDockSpace::ImGuiRender()
{
    // ...
    ImGuiID dockspaceID = ImGui::GetID("EditorDockSpace");
    ImGui::DockSpace(dockspaceID, ...);

    if (m_FirstFrame)
    {
        // 检查 imgui.ini 是否存在
        ImGuiIO& io = ImGui::GetIO();
        bool hasIniFile = io.IniFilename && std::filesystem::exists(io.IniFilename);

        if (!hasIniFile)
        {
            ApplyDefaultLayout(dockspaceID);
        }

        m_FirstFrame = false;
    }

    if (m_ResetLayout)
    {
        ApplyDefaultLayout(dockspaceID);
        m_ResetLayout = false;
    }
    // ...
}
```

**优点**：
- 精确判断是否是真正的首次启动
- 不会覆盖用户已保存的布局

**缺点**：
- 依赖文件系统操作
- 需要额外引入 `<filesystem>` 头文件
- 如果 `io.IniFilename` 为 `nullptr`（手动管理 ini），此方案失效
- `imgui.ini` 存在但不包含 DockSpace 布局信息时（如从旧版本升级），无法正确检测

#### 结论

使用**方案 A**。通过检查 DockBuilder 节点状态来判断是否需要应用默认布局，这是最可靠且与 ImGui 内部机制最一致的方式。它能正确处理首次启动、ini 文件被删除、以及用户手动重置等所有场景。

---

### 5.3 布局应用时机

#### 方案 A：在 `ImGui::DockSpace()` 调用之后立即应用（? 推荐，最优）

```cpp
void EditorDockSpace::ImGuiRender()
{
    // ...
    ImGuiID dockspaceID = ImGui::GetID("EditorDockSpace");
    ImGui::DockSpace(dockspaceID, ...);

    // 在 DockSpace() 之后、面板 Begin() 之前应用布局
    if (/* 需要应用布局 */)
    {
        ApplyDefaultLayout(dockspaceID);
    }
    // ...
}
```

**优点**：
- DockSpace 节点已创建，DockBuilder 可以正确操作
- 布局在面板 `ImGui::Begin()` 之前生效，面板会自动停靠到指定位置
- 与当前调用时序完美契合：`EditorDockSpace::ImGuiRender()` → `UI_DrawMenuBar()` → `PanelManager::OnImGuiRender()`

**缺点**：
- 无

#### 方案 B：在 `EditorLayer::OnImGuiRender()` 中单独调用（? 不推荐）

```cpp
void EditorLayer::OnImGuiRender()
{
    m_EditorDockSpace.ImGuiRender();

    // 在这里检查并应用布局
    if (m_EditorDockSpace.NeedsLayoutReset())
    {
        m_EditorDockSpace.ApplyDefaultLayout(...);
    }

    UI_DrawMenuBar();
    m_PanelManager->OnImGuiRender();
}
```

**优点**：
- EditorLayer 对布局有更多控制权

**缺点**：
- 需要将 `dockspaceID` 暴露给 `EditorLayer`，破坏封装
- 布局逻辑分散在两个类中，不利于维护
- `ApplyDefaultLayout` 需要访问 DockSpace 内部的 ID，增加耦合

#### 结论

使用**方案 A**。布局应用逻辑完全封装在 `EditorDockSpace::ImGuiRender()` 内部，在 `ImGui::DockSpace()` 调用之后立即执行。

---

### 5.4 菜单集成方式

#### 方案 A：在现有 Window 菜单中新增 Layouts 子菜单（? 推荐，最优）

```cpp
// EditorLayer.cpp - UI_DrawMenuBar()
if (ImGui::BeginMenu("Window"))
{
    // ---- 新增 Layouts 子菜单 ----
    if (ImGui::BeginMenu("Layouts"))
    {
        if (ImGui::MenuItem("Default"))
        {
            m_EditorDockSpace.ResetToDefaultLayout();
        }

        ImGui::EndMenu();
    }

    ImGui::Separator();

    if (ImGui::BeginMenu("Panels"))
    {
        // ... 现有面板菜单项 ...
    }

    // ... 现有 Rendering 子菜单 ...

    ImGui::EndMenu();
}
```

**优点**：
- 与 Unity 的菜单结构一致（Window → Layouts → Default）
- 未来扩展新布局只需在 Layouts 子菜单中添加 MenuItem
- 不影响现有菜单结构

**缺点**：
- 无

#### 方案 B：在 Window 菜单顶层直接添加 "Reset Layout" 菜单项（? 其次）

```cpp
if (ImGui::BeginMenu("Window"))
{
    if (ImGui::MenuItem("Reset Layout"))
    {
        m_EditorDockSpace.ResetToDefaultLayout();
    }

    ImGui::Separator();
    // ...
}
```

**优点**：
- 实现更简单，少一层子菜单

**缺点**：
- 不符合 Unity 的菜单习惯
- 未来扩展多种布局时需要重构为子菜单

#### 结论

使用**方案 A**。采用 `Window → Layouts → Default` 的子菜单结构，与 Unity 一致，且为未来扩展预留空间。

---

### 5.5 DockSpace ID 获取方式

#### 方案 A：在 ImGuiRender 内部通过 `ImGui::GetID()` 获取（? 推荐，最优）

```cpp
void EditorDockSpace::ImGuiRender()
{
    // ... Begin("DockSpace") ...
    ImGuiID dockspaceID = ImGui::GetID("EditorDockSpace");
    ImGui::DockSpace(dockspaceID, ...);

    // 直接使用 dockspaceID
    if (/* 需要应用布局 */)
    {
        ApplyDefaultLayout(dockspaceID);
    }
    // ...
}
```

**优点**：
- ID 在 DockSpace 窗口的上下文中计算，保证与 `ImGui::DockSpace()` 使用的 ID 一致
- 无需存储额外成员变量

**缺点**：
- `ApplyDefaultLayout` 只能在 `ImGuiRender()` 内部调用（因为需要正确的 ImGui 窗口上下文）

#### 方案 B：将 DockSpace ID 存储为成员变量（? 其次）

```cpp
// EditorDockSpace.h
class EditorDockSpace
{
    // ...
private:
    uint32_t m_DockSpaceID = 0; // DockSpace 节点 ID
};

// EditorDockSpace.cpp
void EditorDockSpace::ImGuiRender()
{
    // ...
    m_DockSpaceID = ImGui::GetID("EditorDockSpace");
    ImGui::DockSpace(m_DockSpaceID, ...);
    // ...
}
```

**优点**：
- 其他方法可以访问 DockSpace ID
- 未来如果需要在 `ImGuiRender()` 外部操作布局，更灵活

**缺点**：
- `ImGui::GetID()` 依赖当前窗口栈上下文，在 `ImGuiRender()` 外部使用存储的 ID 可能导致不一致
- 增加了一个成员变量

#### 结论

使用**方案 A**。在 `ImGuiRender()` 内部获取 ID 并直接传递给 `ApplyDefaultLayout()`，保证 ID 的正确性。

---

## 六、详细实现设计

### 6.1 EditorLayoutManager.h（新建）

```cpp
#pragma once

namespace Lucky
{
    typedef unsigned int uint32_t;

    /// <summary>
    /// 编辑器布局管理器：负责管理编辑器面板的停靠布局
    /// </summary>
    class EditorLayoutManager
    {
    public:
        EditorLayoutManager() = default;

        /// <summary>
        /// 应用默认布局
        /// </summary>
        /// <param name="dockspaceID">DockSpace 节点 ID</param>
        void ApplyDefaultLayout(uint32_t dockspaceID);
    };
}
```

### 6.2 EditorLayoutManager.cpp（新建）

```cpp
#include "EditorLayoutManager.h"

#include <imgui/imgui.h>
#include <imgui/imgui_internal.h>

namespace Lucky
{
    void EditorLayoutManager::ApplyDefaultLayout(uint32_t dockspaceID)
    {
        ImGuiViewport* viewport = ImGui::GetMainViewport();

        // 清除旧布局
        ImGui::DockBuilderRemoveNode(dockspaceID);

        // 创建新的 DockSpace 节点
        ImGui::DockBuilderAddNode(dockspaceID, ImGuiDockNodeFlags_DockSpace);
        ImGui::DockBuilderSetNodeSize(dockspaceID, viewport->Size);

        ImGuiID dockMain = dockspaceID;

        // Step 1: 左侧分割出 Hierarchy（20%）
        ImGuiID dockLeft;
        ImGuiID dockRemaining;
        ImGui::DockBuilderSplitNode(dockMain, ImGuiDir_Left, 0.2f, &dockLeft, &dockRemaining);

        // Step 2: 右侧分割出 Inspector（25% of remaining）
        ImGuiID dockRight;
        ImGuiID dockCenter;
        ImGui::DockBuilderSplitNode(dockRemaining, ImGuiDir_Right, 0.25f, &dockRight, &dockCenter);

        // Step 3: 中间底部分割出 Bottom 区域（30% 高度）
        ImGuiID dockBottom;
        ImGuiID dockViewport;
        ImGui::DockBuilderSplitNode(dockCenter, ImGuiDir_Down, 0.3f, &dockBottom, &dockViewport);

        // 将窗口停靠到对应节点
        ImGui::DockBuilderDockWindow("Hierarchy", dockLeft);
        ImGui::DockBuilderDockWindow("Scene", dockViewport);
        ImGui::DockBuilderDockWindow("Inspector", dockRight);
        ImGui::DockBuilderDockWindow("Project", dockBottom);
        ImGui::DockBuilderDockWindow("Render Pipeline", dockBottom);    // 与 Project 共享 Tab

        // 完成布局构建
        ImGui::DockBuilderFinish(dockspaceID);
    }
}
```

### 6.3 EditorDockSpace.h 修改

```cpp
#pragma once

#include "EditorLayoutManager.h"

namespace Lucky
{
    typedef unsigned int uint32_t;

    /// <summary>
    /// 编辑器停靠空间
    /// </summary>
    class EditorDockSpace
    {
    public:
        EditorDockSpace(bool fullScreen = true);

        /// <summary>
        /// 绘制 DockSpace
        /// </summary>
        void ImGuiRender();

        /// <summary>
        /// 请求重置为默认布局（下一帧生效）
        /// </summary>
        void ResetToDefaultLayout();
    private:
        bool m_IsFullScreen = true;             // 是否全屏
        uint32_t m_Flags;                       // DockSpace 标志（ImGuiDockNodeFlags）
        uint32_t m_WindowFlags;                 // DockSpace 窗口标志（ImGuiWindowFlags）
        bool m_ResetLayout = false;             // 是否需要重置布局
        EditorLayoutManager m_LayoutManager;    // 布局管理器
    };
}
```

### 6.4 EditorDockSpace.cpp 修改

```cpp
#include "EditorDockSpace.h"

#include <imgui/imgui.h>
#include <imgui/imgui_internal.h>

namespace Lucky
{
    EditorDockSpace::EditorDockSpace(bool fullScreen)
        : m_IsFullScreen(fullScreen),
        m_Flags(ImGuiDockNodeFlags_None),
        m_WindowFlags(ImGuiWindowFlags_MenuBar | ImGuiWindowFlags_NoDocking)
    {

    }

    void EditorDockSpace::ImGuiRender()
    {
        if (m_IsFullScreen)
        {
            ImGuiViewport* viewport = ImGui::GetMainViewport();

            ImGui::SetNextWindowPos(viewport->Pos);
            ImGui::SetNextWindowSize(viewport->Size);
            ImGui::SetNextWindowViewport(viewport->ID);

            ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
            ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);

            m_WindowFlags |= ImGuiWindowFlags_NoTitleBar;
            m_WindowFlags |= ImGuiWindowFlags_NoCollapse;
            m_WindowFlags |= ImGuiWindowFlags_NoResize;
            m_WindowFlags |= ImGuiWindowFlags_NoMove;
            m_WindowFlags |= ImGuiWindowFlags_NoBringToFrontOnFocus;
            m_WindowFlags |= ImGuiWindowFlags_NoNavFocus;
        }

        if (m_Flags)
        {
            m_WindowFlags |= ImGuiWindowFlags_NoBackground;
        }

        static ImVec4 barColor = ImGui::GetStyle().Colors[ImGuiCol_MenuBarBg];
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));
        ImGui::PushStyleColor(ImGuiCol_WindowBg, barColor);
        ImGui::PushStyleColor(ImGuiCol_Border, barColor);
        // DockSpace 窗口
        ImGui::Begin("DockSpace", nullptr, m_WindowFlags);
        {
            ImGui::PopStyleColor(2);
            ImGui::PopStyleVar();

            if (m_IsFullScreen)
            {
                ImGui::PopStyleVar(2);
            }

            ImGuiIO& io = ImGui::GetIO();

            if (io.ConfigFlags & ImGuiConfigFlags_DockingEnable)
            {
                ImGuiID dockspaceID = ImGui::GetID("EditorDockSpace");
                ImGui::DockSpace(dockspaceID, ImVec2(0.0f, 0.0f),
                                 m_Flags | ImGuiDockNodeFlags_NoWindowMenuButton | ImGuiDockNodeFlags_NoCloseButton);

                // ---- 布局管理 ----

                // 首次启动检测：节点不存在或无子节点分割，说明没有已保存的布局
                ImGuiDockNode* node = ImGui::DockBuilderGetNode(dockspaceID);
                if (node == nullptr || !node->IsSplitNode())
                {
                    m_LayoutManager.ApplyDefaultLayout(dockspaceID);
                }

                // 用户请求重置布局
                if (m_ResetLayout)
                {
                    m_LayoutManager.ApplyDefaultLayout(dockspaceID);
                    m_ResetLayout = false;
                }
            }
        }
        ImGui::End();
    }

    void EditorDockSpace::ResetToDefaultLayout()
    {
        m_ResetLayout = true;
    }
}
```

### 6.5 EditorLayer.cpp 菜单修改

在 `UI_DrawMenuBar()` 的 `Window` 菜单中，在 `Panels` 子菜单**之前**新增 `Layouts` 子菜单：

```cpp
if (ImGui::BeginMenu("Window"))
{
    // ---- Layouts 子菜单 ----
    if (ImGui::BeginMenu("Layouts"))
    {
        if (ImGui::MenuItem("Default"))
        {
            m_EditorDockSpace.ResetToDefaultLayout();
        }

        ImGui::EndMenu();
    }

    ImGui::Separator();

    // ---- 现有的 Panels 子菜单（不变）----
    if (ImGui::BeginMenu("Panels"))
    {
        // ... 现有代码不变 ...
    }

    // ---- 现有的 Rendering 子菜单（不变）----
    if (ImGui::BeginMenu("Rendering"))
    {
        // ... 现有代码不变 ...
    }

    ImGui::EndMenu();
}
```

---

## 七、关键设计决策说明

### 7.1 窗口名称硬编码 vs 常量化

#### 方案 A：直接使用字符串字面量（? 推荐，最优）

```cpp
ImGui::DockBuilderDockWindow("Hierarchy", dockLeft);
ImGui::DockBuilderDockWindow("Scene", dockViewport);
ImGui::DockBuilderDockWindow("Inspector", dockRight);
ImGui::DockBuilderDockWindow("Project", dockBottom);
ImGui::DockBuilderDockWindow("Render Pipeline", dockBottom);
```

**优点**：
- 简洁直观，一目了然
- `ApplyDefaultLayout` 是 `EditorLayoutManager` 的方法，布局定义集中在一处
- 窗口名称变更频率极低

**缺点**：
- 如果面板的 `displayName` 修改了，需要同步修改此处（但这种情况极少发生）

#### 方案 B：通过 PanelManager 获取面板名称（? 不推荐）

```cpp
// 需要将 PanelManager 传入 EditorLayoutManager
void EditorLayoutManager::ApplyDefaultLayout(uint32_t dockspaceID, PanelManager& panelManager)
{
    // 通过 PanelManager 获取面板名称...
}
```

**优点**：
- 面板名称与注册处保持同步

**缺点**：
- `EditorLayoutManager` 需要依赖 `PanelManager`，增加耦合
- `PanelData::Name` 是 `const char*`，通过 hash ID 查找再取名称，过于复杂
- 违反单一职责原则

#### 方案 C：定义全局常量（? 其次）

```cpp
// EditorPanelNames.h（新建文件）
namespace Lucky::PanelNames
{
    constexpr const char* Hierarchy = "Hierarchy";
    constexpr const char* Scene = "Scene";
    constexpr const char* Inspector = "Inspector";
    constexpr const char* Project = "Project";
    constexpr const char* RenderPipeline = "Render Pipeline";
}
```

**优点**：
- 面板名称集中管理，修改时只需改一处

**缺点**：
- 当前阶段过度设计，增加了一个文件
- 面板名称在 `EditorLayer::OnAttach()` 中已经通过 `AddPanel` 的参数定义，再定义一套常量是冗余的

#### 结论

使用**方案 A**。直接使用字符串字面量，简洁明了。面板名称变更频率极低，维护成本可控。

### 7.2 面板 IsOpen 状态与布局的关系

**重要说明**：`DockBuilderDockWindow()` 只是告诉 ImGui "当这个窗口出现时，应该停靠在哪个节点"。它**不会**自动打开窗口。窗口是否出现取决于是否调用了 `ImGui::Begin(name)`。

在本项目中，`PanelManager::OnImGuiRender()` 只会为 `IsOpen == true` 的面板调用 `ImGui::Begin()`：

```cpp
void PanelManager::OnImGuiRender()
{
    for (auto& [id, panelData] : m_Panels)
    {
        if (panelData.IsOpen)
        {
            panelData.Panel->OnImGuiRender(panelData.Name, panelData.IsOpen);
        }
    }
}
```

因此，默认布局中停靠的面板（Hierarchy、Scene、Inspector、Project、Render Pipeline）在 `EditorLayer::OnAttach()` 中注册时 `isOpenByDefault` 已经设置为 `true`，**无需额外处理 IsOpen 状态**。

但是，当用户通过菜单重置布局时，如果用户之前关闭了某些面板（如关闭了 Hierarchy），重置布局后这些面板仍然是关闭状态，`DockBuilderDockWindow` 的停靠指令不会生效（因为窗口不会被渲染）。

#### 方案 A：重置布局时同时重置面板的 IsOpen 状态（? 推荐，最优）

在 `EditorLayer` 中，调用 `ResetToDefaultLayout()` 时同时将默认面板的 `IsOpen` 设为 `true`：

```cpp
// EditorLayer.cpp - UI_DrawMenuBar() 中
if (ImGui::MenuItem("Default"))
{
    m_EditorDockSpace.ResetToDefaultLayout();

    // 确保默认布局中的面板都处于打开状态
    auto openPanel = [this](const char* panelID)
    {
        uint32_t id = Hash::GenerateFNVHash(panelID);
        PanelData* data = m_PanelManager->GetPanelData(id);
        data->IsOpen = true;
    };

    openPanel(SCENE_HIERARCHY_PANEL_ID);
    openPanel(SCENE_VIEWPORT_PANEL_ID);
    openPanel(INSPECTOR_PANEL_ID);
    openPanel(PROJECT_ASSETS_PANEL_ID);
    openPanel(RENDER_PIPELINE_PANEL_ID);
}
```

**优点**：
- 重置布局后所有默认面板都会正确显示和停靠
- 行为与 Unity 一致（重置布局 = 恢复到初始状态）

**缺点**：
- 菜单处理代码稍长（但逻辑清晰）

#### 方案 B：不处理 IsOpen 状态，仅重置停靠位置（? 不推荐）

```cpp
if (ImGui::MenuItem("Default"))
{
    m_EditorDockSpace.ResetToDefaultLayout();
    // 不处理 IsOpen
}
```

**优点**：
- 代码最简

**缺点**：
- 如果用户关闭了某些面板后重置布局，这些面板不会重新出现
- 用户体验不佳，与 Unity 行为不一致

#### 结论

使用**方案 A**。重置布局时同时确保默认面板处于打开状态。

---

## 八、完整代码变更清单

### 8.1 EditorLayoutManager.h（新建）

```cpp
#pragma once

namespace Lucky
{
    typedef unsigned int uint32_t;

    /// <summary>
    /// 编辑器布局管理器：负责管理编辑器面板的停靠布局
    /// </summary>
    class EditorLayoutManager
    {
    public:
        EditorLayoutManager() = default;

        /// <summary>
        /// 应用默认布局
        /// </summary>
        /// <param name="dockspaceID">DockSpace 节点 ID</param>
        void ApplyDefaultLayout(uint32_t dockspaceID);
    };
}
```

### 8.2 EditorLayoutManager.cpp（新建）

```cpp
#include "EditorLayoutManager.h"

#include <imgui/imgui.h>
#include <imgui/imgui_internal.h>

namespace Lucky
{
    void EditorLayoutManager::ApplyDefaultLayout(uint32_t dockspaceID)
    {
        ImGuiViewport* viewport = ImGui::GetMainViewport();

        // 清除旧布局
        ImGui::DockBuilderRemoveNode(dockspaceID);

        // 创建新的 DockSpace 节点
        ImGui::DockBuilderAddNode(dockspaceID, ImGuiDockNodeFlags_DockSpace);
        ImGui::DockBuilderSetNodeSize(dockspaceID, viewport->Size);

        ImGuiID dockMain = dockspaceID;

        // Step 1: 左侧分割出 Hierarchy（20%）
        ImGuiID dockLeft;
        ImGuiID dockRemaining;
        ImGui::DockBuilderSplitNode(dockMain, ImGuiDir_Left, 0.2f, &dockLeft, &dockRemaining);

        // Step 2: 右侧分割出 Inspector（25% of remaining）
        ImGuiID dockRight;
        ImGuiID dockCenter;
        ImGui::DockBuilderSplitNode(dockRemaining, ImGuiDir_Right, 0.25f, &dockRight, &dockCenter);

        // Step 3: 中间底部分割出 Bottom 区域（30% 高度）
        ImGuiID dockBottom;
        ImGuiID dockViewport;
        ImGui::DockBuilderSplitNode(dockCenter, ImGuiDir_Down, 0.3f, &dockBottom, &dockViewport);

        // 将窗口停靠到对应节点
        ImGui::DockBuilderDockWindow("Hierarchy", dockLeft);
        ImGui::DockBuilderDockWindow("Scene", dockViewport);
        ImGui::DockBuilderDockWindow("Inspector", dockRight);
        ImGui::DockBuilderDockWindow("Project", dockBottom);
        ImGui::DockBuilderDockWindow("Render Pipeline", dockBottom);    // 与 Project 共享 Tab

        // 完成布局构建
        ImGui::DockBuilderFinish(dockspaceID);
    }
}
```

### 8.3 EditorDockSpace.h（完整替换）

```cpp
#pragma once

#include "EditorLayoutManager.h"

namespace Lucky
{
    typedef unsigned int uint32_t;

    /// <summary>
    /// 编辑器停靠空间
    /// </summary>
    class EditorDockSpace
    {
    public:
        EditorDockSpace(bool fullScreen = true);

        /// <summary>
        /// 绘制 DockSpace
        /// </summary>
        void ImGuiRender();

        /// <summary>
        /// 请求重置为默认布局（下一帧生效）
        /// </summary>
        void ResetToDefaultLayout();
    private:
        bool m_IsFullScreen = true;             // 是否全屏
        uint32_t m_Flags;                       // DockSpace 标志（ImGuiDockNodeFlags）
        uint32_t m_WindowFlags;                 // DockSpace 窗口标志（ImGuiWindowFlags）
        bool m_ResetLayout = false;             // 是否需要重置布局
        EditorLayoutManager m_LayoutManager;    // 布局管理器
    };
}
```

### 8.4 EditorDockSpace.cpp（完整替换）

```cpp
#include "EditorDockSpace.h"

#include <imgui/imgui.h>
#include <imgui/imgui_internal.h>

namespace Lucky
{
    EditorDockSpace::EditorDockSpace(bool fullScreen)
        : m_IsFullScreen(fullScreen),
        m_Flags(ImGuiDockNodeFlags_None),
        m_WindowFlags(ImGuiWindowFlags_MenuBar | ImGuiWindowFlags_NoDocking)
    {

    }

    void EditorDockSpace::ImGuiRender()
    {
        if (m_IsFullScreen)
        {
            ImGuiViewport* viewport = ImGui::GetMainViewport();

            ImGui::SetNextWindowPos(viewport->Pos);
            ImGui::SetNextWindowSize(viewport->Size);
            ImGui::SetNextWindowViewport(viewport->ID);

            ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
            ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);

            m_WindowFlags |= ImGuiWindowFlags_NoTitleBar;
            m_WindowFlags |= ImGuiWindowFlags_NoCollapse;
            m_WindowFlags |= ImGuiWindowFlags_NoResize;
            m_WindowFlags |= ImGuiWindowFlags_NoMove;
            m_WindowFlags |= ImGuiWindowFlags_NoBringToFrontOnFocus;
            m_WindowFlags |= ImGuiWindowFlags_NoNavFocus;
        }

        if (m_Flags)
        {
            m_WindowFlags |= ImGuiWindowFlags_NoBackground;
        }

        static ImVec4 barColor = ImGui::GetStyle().Colors[ImGuiCol_MenuBarBg];
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));
        ImGui::PushStyleColor(ImGuiCol_WindowBg, barColor);
        ImGui::PushStyleColor(ImGuiCol_Border, barColor);
        // DockSpace 窗口
        ImGui::Begin("DockSpace", nullptr, m_WindowFlags);
        {
            ImGui::PopStyleColor(2);
            ImGui::PopStyleVar();

            if (m_IsFullScreen)
            {
                ImGui::PopStyleVar(2);
            }

            ImGuiIO& io = ImGui::GetIO();

            if (io.ConfigFlags & ImGuiConfigFlags_DockingEnable)
            {
                ImGuiID dockspaceID = ImGui::GetID("EditorDockSpace");
                ImGui::DockSpace(dockspaceID, ImVec2(0.0f, 0.0f),
                                 m_Flags | ImGuiDockNodeFlags_NoWindowMenuButton | ImGuiDockNodeFlags_NoCloseButton);

                // ---- 布局管理 ----

                // 首次启动检测：节点不存在或无子节点分割，说明没有已保存的布局
                ImGuiDockNode* node = ImGui::DockBuilderGetNode(dockspaceID);
                if (node == nullptr || !node->IsSplitNode())
                {
                    m_LayoutManager.ApplyDefaultLayout(dockspaceID);
                }

                // 用户请求重置布局
                if (m_ResetLayout)
                {
                    m_LayoutManager.ApplyDefaultLayout(dockspaceID);
                    m_ResetLayout = false;
                }
            }
        }
        ImGui::End();
    }

    void EditorDockSpace::ResetToDefaultLayout()
    {
        m_ResetLayout = true;
    }
}
```

### 8.5 EditorLayer.cpp 变更（仅 UI_DrawMenuBar 中 Window 菜单部分）

在 `if (ImGui::BeginMenu("Window"))` 内部，在 `Panels` 子菜单**之前**插入以下代码：

```cpp
if (ImGui::BeginMenu("Window"))
{
    // ---- 新增：Layouts 子菜单 ----
    if (ImGui::BeginMenu("Layouts"))
    {
        if (ImGui::MenuItem("Default"))
        {
            m_EditorDockSpace.ResetToDefaultLayout();

            // 确保默认布局中的面板都处于打开状态
            auto openPanel = [this](const char* panelID)
            {
                uint32_t id = Hash::GenerateFNVHash(panelID);
                PanelData* data = m_PanelManager->GetPanelData(id);
                data->IsOpen = true;
            };

            openPanel(SCENE_HIERARCHY_PANEL_ID);
            openPanel(SCENE_VIEWPORT_PANEL_ID);
            openPanel(INSPECTOR_PANEL_ID);
            openPanel(PROJECT_ASSETS_PANEL_ID);
            openPanel(RENDER_PIPELINE_PANEL_ID);
        }

        ImGui::EndMenu();
    }

    ImGui::Separator();

    // ---- 以下为现有代码（不变）----
    if (ImGui::BeginMenu("Panels"))
    {
        // ... 现有面板菜单项 ...
    }

    if (ImGui::BeginMenu("Rendering"))
    {
        // ... 现有渲染菜单项 ...
    }

    ImGui::EndMenu();
}
```

---

## 九、未来扩展预留

当前设计为未来扩展预留了清晰的路径：

### 9.1 新增预设布局

只需在 `EditorLayoutManager` 中新增对应的 `Apply*Layout` 方法：

```cpp
// EditorLayoutManager.h — 未来新增
void ApplyTwoByThreeLayout(uint32_t dockspaceID);
void ApplyWideLayout(uint32_t dockspaceID);
```

如果布局类型增多，可以引入枚举和统一入口：

```cpp
// EditorLayoutManager.h — 未来扩展
enum class EditorLayoutType
{
    Default,
    TwoByThree,
    Wide
};

class EditorLayoutManager
{
public:
    void ApplyLayout(EditorLayoutType type, uint32_t dockspaceID);
    // ...
};
```

### 9.2 自定义布局保存/加载

利用 ImGui 的 `SaveIniSettingsToMemory()` / `LoadIniSettingsFromMemory()` 实现：

```cpp
void EditorLayoutManager::SaveCustomLayout(const std::string& name)
{
    size_t size;
    const char* iniData = ImGui::SaveIniSettingsToMemory(&size);
    // 保存到文件：Layouts/<name>.ini
}

void EditorLayoutManager::LoadCustomLayout(const std::string& name)
{
    // 从文件读取：Layouts/<name>.ini
    ImGui::LoadIniSettingsFromMemory(iniData, size);
}
```

---

## 十、测试验证

### 10.1 首次启动测试

1. 删除 `imgui.ini` 文件（如果存在）
2. 启动编辑器
3. **预期结果**：自动应用默认布局，面板按照第四节定义的位置排列

### 10.2 布局持久化测试

1. 启动编辑器（默认布局已应用）
2. 手动拖拽面板到不同位置
3. 关闭编辑器
4. 重新启动编辑器
5. **预期结果**：面板保持上次关闭时的位置（ImGui 自动通过 `imgui.ini` 保存）

### 10.3 重置布局测试

1. 手动拖拽面板到不同位置
2. 点击 **Window → Layouts → Default**
3. **预期结果**：所有面板恢复到默认布局位置

### 10.4 关闭面板后重置测试

1. 右键关闭 Hierarchy 面板
2. 点击 **Window → Layouts → Default**
3. **预期结果**：Hierarchy 面板重新出现并停靠在左侧

---

## 十一、方案决策汇总

| 决策点 | 选定方案 | 理由 |
|--------|----------|------|
| 布局逻辑放置位置 | 方案 B：独立 EditorLayoutManager 类 | 职责分离清晰，为未来多布局扩展预留结构 |
| 首次启动检测 | 方案 A：检查 DockBuilder 节点 | 最可靠，不依赖外部文件，自动适应各种场景 |
| 布局应用时机 | 方案 A：DockSpace() 之后立即应用 | 与调用时序完美契合，保证封装性 |
| 菜单集成方式 | 方案 A：Layouts 子菜单 | 与 Unity 一致，为未来扩展预留空间 |
| DockSpace ID 获取 | 方案 A：ImGuiRender 内部获取 | 保证 ID 正确性，无需额外成员变量 |
| 窗口名称管理 | 方案 A：字符串字面量 | 简洁直观，维护成本可控 |
| 面板 IsOpen 状态 | 方案 A：重置时同步打开 | 与 Unity 行为一致，用户体验最佳 |
