# PhaseR17：渲染管线信息面板（Render Pipeline Panel）

> **版本**：1.0  
> **创建日期**：2026-04-28  
> **前置依赖**：PhaseR7（Multi-Pass Rendering）、PhaseR4（Shadow System）、PhaseR8（Selection Outline）

---

## 一、目标

在编辑器中新增一个 **Render Pipeline** 面板，实时显示渲染管线中所有 RenderPass 的信息，包括：

- 按**实际执行顺序**列出所有 Pass
- 按**分组（Group）**组织显示
- 每个 Pass 的**启用/禁用**状态（可交互切换）
- 持有 FBO 的 Pass 显示 **FBO 分辨率**
- 全局**渲染统计**信息（DrawCalls、三角形数、顶点数）

---

## 二、当前架构概述

### 2.1 渲染管线结构

```
RenderPipeline
  ├── m_Passes: vector<Ref<RenderPass>>  （按添加顺序 = 执行顺序）
  │
  ├── [0] ShadowPass           Group="Shadow"    FBO: 2048×2048 纯深度
  ├── [1] OpaquePass           Group="Main"      FBO: 无（使用外部主 FBO）
  ├── [2] PickingPass          Group="Main"      FBO: 无（复用主 FBO）
  ├── [3] SilhouettePass       Group="Outline"   FBO: 1280×720 RGBA8（随视口 Resize）
  └── [4] OutlineCompositePass Group="Outline"   FBO: 无（使用外部主 FBO）
```

### 2.2 执行流程

```
EndScene()
  → ExecuteGroup("Shadow")   → ShadowPass
  → ExecuteGroup("Main")     → OpaquePass → PickingPass

[Gizmo 渲染（独立于管线）]

RenderOutline()
  → ExecuteGroup("Outline")  → SilhouettePass → OutlineCompositePass
```

### 2.3 编辑器面板体系

```
EditorPanel（基类）
  ├── OnUpdate(DeltaTime dt)           — 每帧更新
  ├── OnImGuiRender(name, isOpen)      — ImGui 渲染入口（基类已实现：Begin + 右键关闭 + OnGUI + End）
  ├── OnGUI()                          — 子类实现：绘制面板内容
  └── OnEvent(Event&)                  — 事件处理

PanelManager
  ├── AddPanel<T>(strID, displayName, isOpenByDefault, args...)
  ├── GetPanel<T>(strID)
  ├── GetPanelData(panelID) → PanelData*
  ├── OnUpdate / OnImGuiRender / OnEvent
  └── m_Panels: unordered_map<uint32_t, PanelData>

PanelData { ID, Name, Panel, IsOpen }
```

### 2.4 现有面板注册方式（EditorLayer::OnAttach）

```cpp
m_PanelManager->AddPanel<SceneHierarchyPanel>(SCENE_HIERARCHY_PANEL_ID, "Hierarchy", true, m_Scene);
m_PanelManager->AddPanel<SceneViewportPanel>(SCENE_VIEWPORT_PANEL_ID, "Scene", true, m_Scene);
m_PanelManager->AddPanel<InspectorPanel>(INSPECTOR_PANEL_ID, "Inspector", true, m_Scene);
```

### 2.5 现有统计数据

```cpp
struct Renderer3D::Statistics {
    uint32_t DrawCalls = 0;
    uint32_t TriangleCount = 0;
    uint32_t GetTotalVertexCount() const { return TriangleCount * 3; }
};
// 通过 Renderer3D::GetStats() 获取，Renderer3D::ResetStats() 重置
// 目前仅在 OpaquePass::Execute() 中累加
```

---

## 三、涉及的文件清单

| 文件 | 操作 | 说明 |
|------|------|------|
| `Luck3DApp/Source/Panels/RenderPipelinePanel.h` | **新建** | 面板头文件 |
| `Luck3DApp/Source/Panels/RenderPipelinePanel.cpp` | **新建** | 面板实现 |
| `Luck3DApp/Source/EditorLayer.h` | **修改** | 添加面板 include 和宏定义 |
| `Luck3DApp/Source/EditorLayer.cpp` | **修改** | 注册面板 + Window 菜单项 |
| `Lucky/Source/Lucky/Renderer/RenderPipeline.h` | **修改** | 添加 `GetPasses()` 访问器 |
| `Lucky/Source/Lucky/Renderer/RenderPass.h` | **修改** | 添加可选的 `GetDescription()` 接口 |

---

## 四、RenderPipeline 扩展 — 暴露 Pass 列表

### 4.1 问题

当前 `RenderPipeline::m_Passes` 是 `private`，面板无法遍历 Pass 列表。

### 4.2 实现方案

#### 方案 A：添加 `GetPasses()` const 引用访问器（? 推荐，最优）

```cpp
// RenderPipeline.h
public:
    /// <summary>
    /// 获取所有 Pass 列表（只读，按执行顺序排列）
    /// </summary>
    const std::vector<Ref<RenderPass>>& GetPasses() const { return m_Passes; }
```

**优点**：
- 零拷贝，返回 const 引用，外部只能读取不能修改列表结构
- 简洁直观，一行代码
- 外部可以通过 `Ref<RenderPass>` 访问 `Enabled`（可写）、`GetName()`、`GetGroup()` 等

**缺点**：
- 暴露了内部容器类型（`vector<Ref<RenderPass>>`），如果未来容器类型变化需要修改接口

#### 方案 B：提供迭代器接口（其次）

```cpp
// RenderPipeline.h
public:
    auto begin() const { return m_Passes.begin(); }
    auto end() const { return m_Passes.end(); }
    size_t GetPassCount() const { return m_Passes.size(); }
```

**优点**：
- 不暴露具体容器类型，更符合 C++ 迭代器惯例
- 支持 range-based for

**缺点**：
- 需要额外的 `GetPassCount()` 方法
- 无法直接按索引访问（如果面板需要显示序号）
- 代码量稍多

#### 方案 C：回调遍历模式（不推荐）

```cpp
// RenderPipeline.h
public:
    void ForEachPass(const std::function<void(const Ref<RenderPass>&, size_t index)>& callback) const
    {
        for (size_t i = 0; i < m_Passes.size(); ++i)
            callback(m_Passes[i], i);
    }
```

**优点**：
- 完全隐藏内部实现

**缺点**：
- `std::function` 有堆分配开销（每帧调用）
- 使用不直观，过度设计

**结论**：使用**方案 A**，简洁高效，与项目现有风格一致（如 `Mesh::GetSubMeshes()` 返回 `const vector&`）。

---

## 五、RenderPass 扩展 — 可选描述信息接口

### 5.1 问题

面板需要显示每个 Pass 的附加信息（如 FBO 分辨率、使用的 Shader 名称等），但 `RenderPass` 基类目前只有 `GetName()` 和 `GetGroup()`。

### 5.2 实现方案

#### 方案 A：在基类添加 `GetDescription()` 虚方法（? 推荐，最优）

```cpp
// RenderPass.h
public:
    /// <summary>
    /// 获取 Pass 的描述信息（用于调试面板显示）
    /// 子类可重写以提供 FBO 分辨率、Shader 名称等附加信息
    /// 默认返回空字符串
    /// </summary>
    virtual std::string GetDescription() const { return ""; }
```

各子类重写：

```cpp
// ShadowPass.h
std::string GetDescription() const override
{
    return std::to_string(m_ShadowMapResolution) + "x" + std::to_string(m_ShadowMapResolution);
}

// SilhouettePass.h
std::string GetDescription() const override
{
    const auto& spec = m_SilhouetteFBO->GetSpecification();
    return std::to_string(spec.Width) + "x" + std::to_string(spec.Height);
}

// OpaquePass.h — 不重写，使用默认空字符串
// PickingPass.h — 不重写
// OutlineCompositePass.h — 不重写
```

**优点**：
- 基类提供默认实现，子类按需重写，零侵入
- 面板代码统一调用 `pass->GetDescription()`，无需 `dynamic_cast`
- 后续新增 Pass 自动兼容（默认空描述）

**缺点**：
- 返回 `std::string` 有堆分配（每帧调用），但面板刷新频率低，可忽略
- 描述格式由各子类自行决定，缺乏统一结构

#### 方案 B：在基类添加结构化的 `GetPassInfo()` 方法（其次）

```cpp
// RenderPass.h
public:
    /// <summary>
    /// Pass 信息结构体（用于调试面板）
    /// </summary>
    struct PassInfo
    {
        uint32_t FBOWidth = 0;          // FBO 宽度（0 表示无独立 FBO）
        uint32_t FBOHeight = 0;         // FBO 高度
        bool HasOwnFBO = false;         // 是否持有独立 FBO
        std::string ShaderName = "";    // 使用的 Shader 名称
    };
    
    /// <summary>
    /// 获取 Pass 信息（用于调试面板）
    /// 子类可重写以提供详细信息
    /// </summary>
    virtual PassInfo GetPassInfo() const { return {}; }
```

各子类重写：

```cpp
// ShadowPass.h
PassInfo GetPassInfo() const override
{
    PassInfo info;
    info.HasOwnFBO = true;
    info.FBOWidth = m_ShadowMapResolution;
    info.FBOHeight = m_ShadowMapResolution;
    info.ShaderName = "Shadow";
    return info;
}

// SilhouettePass.h
PassInfo GetPassInfo() const override
{
    PassInfo info;
    info.HasOwnFBO = true;
    const auto& spec = m_SilhouetteFBO->GetSpecification();
    info.FBOWidth = spec.Width;
    info.FBOHeight = spec.Height;
    info.ShaderName = "Silhouette";
    return info;
}
```

**优点**：
- 结构化数据，面板可以灵活格式化显示
- 类型安全，不依赖字符串解析

**缺点**：
- `PassInfo` 结构体需要随需求扩展，可能频繁修改
- 对于不需要额外信息的 Pass（OpaquePass、PickingPass），返回空结构体略显冗余
- 代码量更大

#### 方案 C：面板中使用 `dynamic_cast` 识别具体 Pass 类型（不推荐）

```cpp
// RenderPipelinePanel.cpp
for (const auto& pass : passes)
{
    if (auto shadow = std::dynamic_pointer_cast<ShadowPass>(pass))
    {
        // 显示 ShadowPass 特有信息
    }
    else if (auto silhouette = std::dynamic_pointer_cast<SilhouettePass>(pass))
    {
        // 显示 SilhouettePass 特有信息
    }
    // ...
}
```

**优点**：
- 不需要修改 RenderPass 基类和子类

**缺点**：
- 面板代码与具体 Pass 类型强耦合
- 每新增一个 Pass 都需要修改面板代码
- 违反开闭原则
- `dynamic_cast` 有运行时开销

**结论**：使用**方案 A**。简洁、低侵入、可扩展。如果后续面板需要更丰富的结构化数据，可以升级为方案 B。

---

## 六、RenderPipelinePanel 面板实现

### 6.1 面板类设计

#### 方案 A：继承 `EditorPanel`，通过 `PanelManager` 管理（? 推荐，最优）

```cpp
// RenderPipelinePanel.h
#pragma once

#include "Lucky/Editor/EditorPanel.h"

namespace Lucky
{
    class RenderPipelinePanel : public EditorPanel
    {
    public:
        RenderPipelinePanel() = default;
        ~RenderPipelinePanel() override = default;
        
        void OnUpdate(DeltaTime dt) override;
        void OnGUI() override;
    };
}
```

**注册方式**（EditorLayer::OnAttach）：

```cpp
#define RENDER_PIPELINE_PANEL_ID "RenderPipelinePanel"

m_PanelManager->AddPanel<RenderPipelinePanel>(RENDER_PIPELINE_PANEL_ID, "Render Pipeline", false);
// 注意：第三个参数 false 表示默认不打开，需要通过 Window 菜单手动打开
```

**优点**：
- 与现有面板体系完全一致（SceneHierarchyPanel、InspectorPanel 等）
- 自动获得 PanelManager 的生命周期管理、OnUpdate/OnImGuiRender/OnEvent 调度
- 自动获得 EditorPanel 基类的 ImGui::Begin/End、右键关闭菜单等功能
- 支持通过 Window 菜单切换显示/隐藏

**缺点**：
- 无（这是标准做法）

#### 方案 B：独立面板，不走 `PanelManager`（类似 `PreferencesPanel`）（不推荐）

```cpp
// RenderPipelinePanel.h
class RenderPipelinePanel
{
public:
    void OnImGuiRender();
    void Open() { m_IsOpen = true; }
    bool IsOpen() const { return m_IsOpen; }
private:
    bool m_IsOpen = false;
};
```

**使用方式**（EditorLayer）：

```cpp
// EditorLayer.h
RenderPipelinePanel m_RenderPipelinePanel;

// EditorLayer::OnImGuiRender()
m_RenderPipelinePanel.OnImGuiRender();
```

**优点**：
- 实现简单，不依赖 PanelManager

**缺点**：
- 不支持 PanelManager 的统一调度（OnUpdate、OnEvent）
- 不支持 EditorPanel 基类的右键关闭菜单等功能
- 与 PreferencesPanel 的独立方式一致，但 PreferencesPanel 是弹窗式面板（有特殊原因），RenderPipelinePanel 是常驻面板，应该走 PanelManager
- 需要在 EditorLayer 中手动管理生命周期

**结论**：使用**方案 A**。RenderPipelinePanel 是常驻调试面板，应该与其他面板一样通过 PanelManager 管理。

### 6.2 面板构造函数参数

#### 方案 A：无参构造，面板内部通过静态方法获取数据（? 推荐，最优）

```cpp
// RenderPipelinePanel.cpp
void RenderPipelinePanel::OnGUI()
{
    // 通过静态方法获取数据
    auto stats = Renderer3D::GetStats();
    auto& pipeline = Renderer3D::GetPipeline();
    const auto& passes = pipeline.GetPasses();
    // ...
}
```

**优点**：
- 构造函数无参数，注册时最简洁：`AddPanel<RenderPipelinePanel>(ID, "Render Pipeline", false)`
- 与 `Renderer3D::GetStats()`、`Renderer3D::GetPipeline()` 的静态访问模式一致
- 面板不持有任何外部引用，无生命周期问题

**缺点**：
- 面板与 `Renderer3D` 静态接口耦合（但这是引擎内部面板，耦合可接受）

#### 方案 B：构造函数接收 `RenderPipeline&` 引用（不推荐）

```cpp
class RenderPipelinePanel : public EditorPanel
{
public:
    RenderPipelinePanel(RenderPipeline& pipeline) : m_Pipeline(pipeline) {}
private:
    RenderPipeline& m_Pipeline;
};
```

**优点**：
- 显式依赖注入，更易测试

**缺点**：
- `RenderPipeline` 是 `Renderer3DData` 的成员（`static` 内部数据），通过引用传递不如静态方法直观
- 注册时需要额外传参：`AddPanel<RenderPipelinePanel>(ID, "Render Pipeline", false, Renderer3D::GetPipeline())`
- 面板还需要 `Renderer3D::GetStats()`，如果也通过构造函数传递会更复杂

**结论**：使用**方案 A**。面板内部通过 `Renderer3D` 的静态方法获取所有数据，简洁一致。

### 6.3 OnUpdate 实现

```cpp
void RenderPipelinePanel::OnUpdate(DeltaTime dt)
{
    // 面板是纯展示型，不需要每帧更新逻辑
    // OnGUI() 中直接读取实时数据即可
}
```

---

## 七、面板 UI 布局设计

### 7.1 整体布局

```
┌──────────────────────────────────────────────┐
│  Render Pipeline                         [×] │
├──────────────────────────────────────────────┤
│                                              │
│  ▼ Statistics                                │
│    Draw Calls:    12                         │
│    Triangles:     4,832                      │
│    Vertices:      14,496                     │
│                                              │
│  ──────────────────────────────────────────  │
│                                              │
│  ▼ Passes (5)                                │
│                                              │
│    Shadow                                    │
│    ┌──────────────────────────────────────┐  │
│    │ ? #0 ShadowPass         2048×2048   │  │
│    └──────────────────────────────────────┘  │
│                                              │
│    Main                                      │
│    ┌──────────────────────────────────────┐  │
│    │ ? #1 OpaquePass                     │  │
│    │ ? #2 PickingPass                    │  │
│    └──────────────────────────────────────┘  │
│                                              │
│    Outline                                   │
│    ┌──────────────────────────────────────┐  │
│    │ ? #3 SilhouettePass     1280×720    │  │
│    │ ? #4 OutlineCompositePass           │  │
│    └──────────────────────────────────────┘  │
│                                              │
└──────────────────────────────────────────────┘
```

### 7.2 显示顺序

**关键要求**：Passes 的显示顺序必须与 `m_Passes` 的注册顺序（= 执行顺序）一致。

由于 `RenderPipeline::GetPasses()` 返回的 `vector` 本身就是按注册顺序排列的，直接遍历即可保证顺序正确。

### 7.3 分组显示策略

#### 方案 A：遍历 Pass 列表，按顺序检测分组变化时插入分组标题（? 推荐，最优）

```cpp
std::string currentGroup = "";
int passIndex = 0;

for (const auto& pass : passes)
{
    const std::string& group = pass->GetGroup();
    
    // 分组变化时显示分组标题
    if (group != currentGroup)
    {
        currentGroup = group;
        // 非第一个分组前添加间距
        if (passIndex > 0)
            ImGui::Spacing();
        
        ImGui::TextColored(ImVec4(0.6f, 0.8f, 1.0f, 1.0f), "%s", group.c_str());
        ImGui::Separator();
    }
    
    // 显示 Pass 信息
    // ...
    
    passIndex++;
}
```

**优点**：
- 严格保持执行顺序
- 一次遍历，O(n) 复杂度
- 分组标题自然出现在该组第一个 Pass 之前
- 如果同一分组的 Pass 不连续（虽然当前不会），也能正确显示

**缺点**：
- 如果同一分组的 Pass 被其他分组的 Pass 隔开，会出现重复的分组标题（但当前架构中同组 Pass 总是连续的）

#### 方案 B：先收集分组信息，再按分组渲染（不推荐）

```cpp
// 第一遍：收集分组及其 Pass 索引
std::vector<std::pair<std::string, std::vector<size_t>>> groups;
// ...

// 第二遍：按分组渲染
for (const auto& [groupName, indices] : groups)
{
    ImGui::Text("%s", groupName.c_str());
    for (size_t idx : indices)
    {
        // 显示 passes[idx]
    }
}
```

**优点**：
- 分组标题不会重复

**缺点**：
- 两遍遍历，代码复杂
- 如果按分组重新排列，可能打乱实际执行顺序（违反核心需求）
- 需要额外的数据结构

**结论**：使用**方案 A**。一次遍历，严格保持执行顺序，简洁高效。

### 7.4 每个 Pass 的显示内容

每个 Pass 行显示以下信息：

```
? #0 ShadowPass         2048×2048
│  │  │                  │
│  │  │                  └── GetDescription()（右对齐，灰色，可选）
│  │  └── GetName()
│  └── 执行序号（从 0 开始，对应 m_Passes 中的索引）
└── Enabled Checkbox（可交互）
```

### 7.5 Pass 行的 ImGui 实现

#### 方案 A：使用 `ImGui::Checkbox` + `ImGui::SameLine` + `ImGui::Text` 组合（? 推荐，最优）

```cpp
// 每个 Pass 的渲染
bool enabled = pass->Enabled;
std::string label = "##pass_" + std::to_string(passIndex);

// Checkbox（启用/禁用）
if (ImGui::Checkbox(label.c_str(), &enabled))
{
    pass->Enabled = enabled;
}

ImGui::SameLine();

// 序号 + 名称
ImGui::Text("#%d %s", passIndex, pass->GetName().c_str());

// 描述信息（右对齐）
std::string desc = pass->GetDescription();
if (!desc.empty())
{
    float textWidth = ImGui::CalcTextSize(desc.c_str()).x;
    float availWidth = ImGui::GetContentRegionAvail().x;
    ImGui::SameLine(availWidth - textWidth);
    ImGui::TextDisabled("%s", desc.c_str());
}
```

**优点**：
- 简洁直观
- Checkbox 直接修改 `pass->Enabled`，实时生效
- 描述信息右对齐，不影响主要信息的阅读
- `ImGui::TextDisabled` 自动使用灰色，视觉层次清晰

**缺点**：
- 右对齐计算在窗口宽度变化时可能有微小偏移（可忽略）

#### 方案 B：使用 `ImGui::Columns` 或 `ImGui::Table` 实现表格布局（其次）

```cpp
if (ImGui::BeginTable("PassTable", 3, ImGuiTableFlags_SizingStretchProp))
{
    ImGui::TableSetupColumn("Enabled", ImGuiTableColumnFlags_WidthFixed, 30.0f);
    ImGui::TableSetupColumn("Name", ImGuiTableColumnFlags_WidthStretch);
    ImGui::TableSetupColumn("Info", ImGuiTableColumnFlags_WidthFixed, 100.0f);
    
    for (size_t i = 0; i < passes.size(); ++i)
    {
        ImGui::TableNextRow();
        
        ImGui::TableSetColumnIndex(0);
        bool enabled = passes[i]->Enabled;
        if (ImGui::Checkbox(("##pass_" + std::to_string(i)).c_str(), &enabled))
            passes[i]->Enabled = enabled;
        
        ImGui::TableSetColumnIndex(1);
        ImGui::Text("#%d %s", (int)i, passes[i]->GetName().c_str());
        
        ImGui::TableSetColumnIndex(2);
        std::string desc = passes[i]->GetDescription();
        if (!desc.empty())
            ImGui::TextDisabled("%s", desc.c_str());
    }
    
    ImGui::EndTable();
}
```

**优点**：
- 列对齐更精确
- 支持列排序（如果未来需要）

**缺点**：
- 表格布局与分组标题的结合比较复杂（分组标题需要跨列显示）
- 代码量更大
- 对于当前 5 个 Pass 的规模，表格布局过于正式

**结论**：使用**方案 A**。当前 Pass 数量少，简单的 Checkbox + Text 组合足够清晰。如果后续 Pass 数量增长到 10+ 且需要更多列信息，可以升级为方案 B。

---

## 八、Statistics 区域实现

### 8.1 数据来源

```cpp
auto stats = Renderer3D::GetStats();
// stats.DrawCalls      — OpaquePass 中累加
// stats.TriangleCount  — OpaquePass 中累加
// stats.GetTotalVertexCount() — TriangleCount * 3
```

### 8.2 ImGui 实现

```cpp
if (ImGui::TreeNodeEx("Statistics", ImGuiTreeNodeFlags_DefaultOpen))
{
    auto stats = Renderer3D::GetStats();
    
    ImGui::Text("Draw Calls:  %d", stats.DrawCalls);
    ImGui::Text("Triangles:   %d", stats.TriangleCount);
    ImGui::Text("Vertices:    %d", stats.GetTotalVertexCount());
    
    ImGui::TreePop();
}
```

> **注意**：`Renderer3D::ResetStats()` 在每帧 `BeginScene()` 之前由 `Scene::OnUpdate()` 调用。面板在 `OnGUI()` 中读取的是上一帧的统计数据，这是正确的行为。

---

## 九、EditorLayer 集成

### 9.1 头文件修改

```cpp
// EditorLayer.h — 无需修改（面板通过 PanelManager 管理，不需要额外成员变量）
```

### 9.2 实现文件修改

```cpp
// EditorLayer.cpp

// 1. 添加 include
#include "Panels/RenderPipelinePanel.h"

// 2. 添加面板 ID 宏
#define RENDER_PIPELINE_PANEL_ID "RenderPipelinePanel"

// 3. 在 OnAttach() 中注册面板（在现有面板注册之后）
m_PanelManager->AddPanel<RenderPipelinePanel>(RENDER_PIPELINE_PANEL_ID, "Render Pipeline", false);

// 4. 在 UI_DrawMenuBar() 的 Window 菜单中添加菜单项
if (ImGui::MenuItem("Render Pipeline"))
{
    uint32_t panelID = Hash::GenerateFNVHash(RENDER_PIPELINE_PANEL_ID);
    PanelData* panelData = m_PanelManager->GetPanelData(panelID);
    panelData->IsOpen = true;
}
```

### 9.3 Window 菜单项位置

在现有菜单项之后添加，用 `ImGui::Separator()` 分隔调试类面板：

```cpp
if (ImGui::BeginMenu("Window"))
{
    // 现有面板
    if (ImGui::MenuItem("Hierarchy")) { /* ... */ }
    if (ImGui::MenuItem("Scene")) { /* ... */ }
    if (ImGui::MenuItem("Inspector")) { /* ... */ }
    
    ImGui::Separator();  // 分隔线
    
    // 调试面板
    if (ImGui::MenuItem("Render Pipeline"))
    {
        uint32_t panelID = Hash::GenerateFNVHash(RENDER_PIPELINE_PANEL_ID);
        PanelData* panelData = m_PanelManager->GetPanelData(panelID);
        panelData->IsOpen = true;
    }
    
    ImGui::EndMenu();
}
```

---

## 十、完整代码参考

### 10.1 RenderPipeline.h 修改

```cpp
// 在 public 区域添加（Resize 方法之后）：

/// <summary>
/// 获取所有 Pass 列表（只读，按执行顺序排列）
/// 用于调试面板显示管线信息
/// </summary>
const std::vector<Ref<RenderPass>>& GetPasses() const { return m_Passes; }
```

### 10.2 RenderPass.h 修改

```cpp
// 在 GetGroup() 方法之后、Enabled 成员之前添加：

/// <summary>
/// 获取 Pass 的描述信息（用于调试面板显示）
/// 子类可重写以提供 FBO 分辨率、Shader 名称等附加信息
/// 默认返回空字符串
/// </summary>
virtual std::string GetDescription() const { return ""; }
```

### 10.3 ShadowPass.h 修改

```cpp
// 在 GetGroup() 之后添加：

std::string GetDescription() const override
{
    return std::to_string(m_ShadowMapResolution) + "x" + std::to_string(m_ShadowMapResolution);
}
```

### 10.4 SilhouettePass.h 修改

```cpp
// 在 GetGroup() 之后添加：

std::string GetDescription() const override
{
    if (m_SilhouetteFBO)
    {
        const auto& spec = m_SilhouetteFBO->GetSpecification();
        return std::to_string(spec.Width) + "x" + std::to_string(spec.Height);
    }
    return "";
}
```

### 10.5 RenderPipelinePanel.h

```cpp
#pragma once

#include "Lucky/Editor/EditorPanel.h"

namespace Lucky
{
    /// <summary>
    /// 渲染管线信息面板：显示所有 RenderPass 的执行顺序、启用状态和渲染统计
    /// 通过 PanelManager 管理，默认不打开，通过 Window 菜单开启
    /// </summary>
    class RenderPipelinePanel : public EditorPanel
    {
    public:
        RenderPipelinePanel() = default;
        ~RenderPipelinePanel() override = default;
        
        void OnUpdate(DeltaTime dt) override;
        void OnGUI() override;
    };
}
```

### 10.6 RenderPipelinePanel.cpp

```cpp
#include "RenderPipelinePanel.h"

#include "Lucky/Renderer/Renderer3D.h"
#include "Lucky/Renderer/RenderPipeline.h"
#include "Lucky/Renderer/RenderPass.h"

#include <imgui/imgui.h>

namespace Lucky
{
    void RenderPipelinePanel::OnUpdate(DeltaTime dt)
    {
        // 纯展示型面板，无需每帧更新逻辑
    }
    
    void RenderPipelinePanel::OnGUI()
    {
        // ======== Statistics 区域 ========
        if (ImGui::TreeNodeEx("Statistics", ImGuiTreeNodeFlags_DefaultOpen))
        {
            auto stats = Renderer3D::GetStats();
            
            ImGui::Text("Draw Calls:  %d", stats.DrawCalls);
            ImGui::Text("Triangles:   %d", stats.TriangleCount);
            ImGui::Text("Vertices:    %d", stats.GetTotalVertexCount());
            
            ImGui::TreePop();
        }
        
        ImGui::Separator();
        
        // ======== Passes 区域 ========
        auto& pipeline = Renderer3D::GetPipeline();
        const auto& passes = pipeline.GetPasses();
        
        std::string passesHeader = "Passes (" + std::to_string(passes.size()) + ")";
        
        if (ImGui::TreeNodeEx(passesHeader.c_str(), ImGuiTreeNodeFlags_DefaultOpen))
        {
            std::string currentGroup = "";
            
            for (size_t i = 0; i < passes.size(); ++i)
            {
                const auto& pass = passes[i];
                const std::string& group = pass->GetGroup();
                
                // ---- 分组标题（分组变化时显示） ----
                if (group != currentGroup)
                {
                    currentGroup = group;
                    
                    if (i > 0)
                        ImGui::Spacing();
                    
                    ImGui::TextColored(ImVec4(0.6f, 0.8f, 1.0f, 1.0f), "%s", group.c_str());
                    ImGui::Separator();
                }
                
                // ---- Pass 行 ----
                
                // Checkbox（启用/禁用）
                bool enabled = pass->Enabled;
                std::string checkboxLabel = "##pass_" + std::to_string(i);
                if (ImGui::Checkbox(checkboxLabel.c_str(), &enabled))
                {
                    pass->Enabled = enabled;
                }
                
                ImGui::SameLine();
                
                // 序号 + 名称
                ImGui::Text("#%d %s", (int)i, pass->GetName().c_str());
                
                // 描述信息（右对齐，灰色）
                std::string desc = pass->GetDescription();
                if (!desc.empty())
                {
                    ImGui::SameLine();
                    float textWidth = ImGui::CalcTextSize(desc.c_str()).x;
                    float availWidth = ImGui::GetContentRegionAvail().x;
                    if (availWidth > textWidth)
                    {
                        ImGui::SameLine(ImGui::GetWindowWidth() - textWidth - ImGui::GetStyle().WindowPadding.x);
                    }
                    ImGui::TextDisabled("%s", desc.c_str());
                }
            }
            
            ImGui::TreePop();
        }
    }
}
```

### 10.7 EditorLayer.cpp 修改

```cpp
// 1. 添加 include（在现有 Panels include 之后）
#include "Panels/RenderPipelinePanel.h"

// 2. 添加面板 ID 宏（在现有宏定义之后）
#define RENDER_PIPELINE_PANEL_ID "RenderPipelinePanel"

// 3. OnAttach() 中注册面板（在现有 AddPanel 之后）
m_PanelManager->AddPanel<RenderPipelinePanel>(RENDER_PIPELINE_PANEL_ID, "Render Pipeline", false);

// 4. UI_DrawMenuBar() 的 Window 菜单中添加（在 Inspector 菜单项之后）
ImGui::Separator();

if (ImGui::MenuItem("Render Pipeline"))
{
    uint32_t panelID = Hash::GenerateFNVHash(RENDER_PIPELINE_PANEL_ID);
    PanelData* panelData = m_PanelManager->GetPanelData(panelID);
    panelData->IsOpen = true;
}
```

---

## 十一、编译依赖分析

### 11.1 RenderPipelinePanel 的 include 依赖链

```
RenderPipelinePanel.cpp
  ├── RenderPipelinePanel.h
  │     └── Lucky/Editor/EditorPanel.h
  │           ├── Lucky/Core/DeltaTime.h
  │           └── Lucky/Core/Events/Event.h
  ├── Lucky/Renderer/Renderer3D.h        （GetStats, GetPipeline）
  ├── Lucky/Renderer/RenderPipeline.h    （GetPasses）
  ├── Lucky/Renderer/RenderPass.h        （GetName, GetGroup, GetDescription, Enabled）
  └── imgui/imgui.h
```

### 11.2 不需要 include 的文件

- 不需要 include 任何具体 Pass 的头文件（ShadowPass.h 等）
- 不需要 include RenderContext.h
- 不需要 include Framebuffer.h

这保证了面板与具体 Pass 实现的解耦。

---

## 十二、后续扩展方向（本期不实施）

以下功能可在后续版本中按需添加：

### 12.1 Per-Pass GPU 耗时统计

- 使用 OpenGL Timer Query（`GL_TIME_ELAPSED`）测量每个 Pass 的 GPU 执行时间
- 在 `RenderPass` 基类中添加 `float GetLastGPUTime() const`
- 面板中显示每个 Pass 的耗时（ms）

### 12.2 Shadow Map / Silhouette 纹理预览

- 在面板中点击 Pass 名称展开详情区域
- 使用 `ImGui::Image()` 显示 FBO 纹理的缩略图
- ShadowPass 的深度纹理需要线性化后显示

### 12.3 RenderContext 实时数据显示

- 显示当前帧的 `ShadowEnabled`、`LightSpaceMatrix`、`OutlineEntityIDs` 等
- 需要在 `Renderer3D` 中缓存最近一帧的 `RenderContext`

### 12.4 Pass 重排序

- 通过拖拽调整 Pass 的执行顺序
- 需要在 `RenderPipeline` 中添加 `ReorderPass()` 方法

---

## 十三、测试验证清单

| # | 测试项 | 预期结果 |
|---|--------|----------|
| 1 | 通过 Window → Render Pipeline 打开面板 | 面板正常显示 |
| 2 | 面板显示 5 个 Pass，顺序为 Shadow → Opaque → Picking → Silhouette → OutlineComposite | 与注册顺序一致 |
| 3 | 分组标题显示正确：Shadow、Main、Outline | 三个分组 |
| 4 | ShadowPass 描述显示 "2048x2048" | FBO 分辨率正确 |
| 5 | SilhouettePass 描述显示当前视口分辨率（如 "1280x720"） | 随视口 Resize 更新 |
| 6 | OpaquePass、PickingPass、OutlineCompositePass 无描述信息 | 不显示额外文本 |
| 7 | 取消勾选 ShadowPass 的 Checkbox | 场景中阴影消失 |
| 8 | 重新勾选 ShadowPass 的 Checkbox | 阴影恢复 |
| 9 | 取消勾选 OpaquePass 的 Checkbox | 场景中不透明物体消失（只剩 Gizmo） |
| 10 | Statistics 区域显示正确的 DrawCalls、Triangles、Vertices | 数值随场景物体数量变化 |
| 11 | 右键面板标签 → Close Tab | 面板关闭 |
| 12 | 再次通过 Window 菜单打开 | 面板恢复显示 |
| 13 | 调整视口大小后，SilhouettePass 的描述信息更新 | 分辨率跟随视口变化 |
