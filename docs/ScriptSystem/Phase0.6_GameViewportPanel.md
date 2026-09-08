# Phase 0.6：GameViewportPanel（Game 面板）

## 1. 概述

P0.6 目标：在编辑器中新增一个独立的 **Game 面板**，与 `Scene` 面板并列存在。该面板使用**场景内 Primary CameraComponent** 作为视图 / 投影来源渲染场景，用于预览"玩家看到的画面"。面板本身没有 Gizmo / Grid / Outline / 拾取等编辑器 Overlay，是纯粹的"游戏画面输出窗口"，行为对齐 Unity 编辑器的 Game View。

### 1.1 行为规格

- **渲染源**：调用 `Scene::OnRenderRuntime()`（P0.2 已实装），由 `Scene` 内部按 `GetPrimaryCameraEntity()` 取 Primary `CameraComponent`
- **Overlay**：**不绘制**任何编辑器 Overlay（Grid、Gizmo、ImGuizmo、描边、Frustum、View Orientation Gizmo）
- **交互**：**不响应**任何输入事件（不做鼠标拾取、不消费快捷键、不做拖放接收）
- **状态无感**：在 `SceneState::Edit / Play / Pause` 三种状态下行为完全一致??`OnUpdateEditor / OnUpdateRuntime` 由 `EditorLayer` 每帧统一驱动，Game 面板只在 `OnUpdate` 里做一次相机渲染
- **无主相机时**：Game 面板显示**纯黑**，不打日志、不加提示文字（对齐用户明确需求）

### 1.2 具体产出

1. 新增 `GameViewportPanel` 类（`Luck3DApp/Source/Panels/`），继承 `EditorPanel`
2. `EditorLayer` 注册面板，加入 `Window → Panels` 子菜单和 `Default Layout` 重置列表
3. `EditorLayoutManager` 把 `Game` 与 `Scene` 停靠到同一 Dock 节点，默认布局下形成 Tab 共享（`Scene` 在前 tab、`Game` 在后 tab）

### 1.3 前置依赖

- P0.1 完成（`Scene::OnUpdateEditor / OnUpdateRuntime`、`OnRenderEditor / OnRenderRuntime` 已拆分）
- P0.2 完成（`CameraComponent + SceneCamera`、`Scene::GetPrimaryCameraEntity`、`Scene::OnRenderRuntime()` 已实装）
- P0.3 完成（Play 时 `SceneManager` 会切换到深拷贝副本，Game 面板通过 `SceneManager::Subscribe` 天然同步）
- P0.5 完成（Play/Pause 工具条已就位，本 Phase 依赖它切换 SceneState，但不与其发生代码耦合）

### 1.4 本 Phase **不做**的事

- **不做 "No cameras rendering" 提示文字**：无主相机时直接黑屏（明确的产品决策）
- **不做**在 Game 面板内响应快捷键、拾取或 Focus 时的输入捕获
- **不做**多相机分屏 / 多 Game 面板
- **不做**独立 pipeline：`Renderer3D` 仍是全局单例，`ResizePipeline` 仍由 Scene 面板负责调用（详见 §5 坑点 2）
- **不做**独立于 `ViewportClearColor` 的清屏色偏好项：Game 面板与 Scene 面板复用同一 `ColorSettings::ViewportClearColor`

---

## 2. 涉及的文件

### 需要修改

| 文件 | 说明 |
|------|------|
| `Luck3DApp/Source/EditorLayer.cpp` | 新增 `GAME_VIEWPORT_PANEL_ID` 宏；`OnAttach` 注册面板；`UI_DrawMenuBar` 的 `Window → Panels` 子菜单追加 `Game`；`Window → Layouts → Default` 中把 Game 面板加入 `OpenPanel` 列表 |
| `Luck3DApp/Source/EditorLayoutManager.cpp` | `ApplyDefaultLayout` 将 `Game` 窗口与 `Scene` 停到同一 Dock 节点 |

### 需要新建

| 文件 | 说明 |
|------|------|
| `Luck3DApp/Source/Panels/GameViewportPanel.h` | `GameViewportPanel` 类声明 |
| `Luck3DApp/Source/Panels/GameViewportPanel.cpp` | `GameViewportPanel` 类实现 |

### 无需修改

- `Lucky/Source/Lucky/Scene/Scene.h/.cpp`：`OnRenderRuntime()` 已在 P0.2 就位
- `Lucky/Source/Lucky/Renderer/Renderer3D.*`：现有 `SetTargetFramebuffer / SetClearColor` 全局单例接口足以支撑
- `Lucky/Source/Lucky/Editor/EditorPanel.*`：基类接口已足够
- 其他所有面板

---

## 3. 关键设计决策（方案对比）

### 3.1 Scene 引用同步机制

**方案 A（推荐）：`SceneManager::Subscribe` 订阅切换事件**

```cpp
GameViewportPanel::GameViewportPanel(const Ref<Scene>& scene)
    : m_Scene(scene)
{
    m_SceneChangedSub = SceneManager::Subscribe([this](const Ref<Scene>& newScene)
    {
        m_Scene = newScene;
    });
}

GameViewportPanel::~GameViewportPanel()
{
    SceneManager::Unsubscribe(m_SceneChangedSub);
}
```

- **优点**：完全对齐 `SceneViewportPanel` 的既有模式；Play/Stop 时 `SceneManager` 广播的副本切换会自动同步；无需在 Update 阶段做额外查询
- **缺点**：需要多写一个订阅句柄成员

**方案 B：每帧 `SceneManager::GetActiveScene()` 查询**

```cpp
void GameViewportPanel::OnUpdate(DeltaTime dt)
{
    const Ref<Scene>& scene = SceneManager::GetActiveScene();
    if (!scene) return;
    // ...
}
```

- **优点**：无订阅句柄，代码更短
- **缺点**：与 `SceneViewportPanel` 风格不一致；面板"当前 Scene"隐式，未来做多面板 / 多 Scene 时不好扩展

**结论**：采用 **方案 A**（订阅模式），与 `SceneViewportPanel` 保持一致的架构风格。

### 3.2 无主相机时的处理

**方案 A（推荐）：不判断，直接调 `Scene::OnRenderRuntime()`，靠 Scene 内部 early-return**

```cpp
m_Framebuffer->Bind();
RenderCommand::SetClearColor(glm::vec4(0.0f, 0.0f, 0.0f, 1.0f));   // 无 Primary 时看到的黑
RenderCommand::Clear();
Renderer3D::SetTargetFramebuffer(m_Framebuffer);
Renderer3D::SetClearColor(glm::vec4(0.0f, 0.0f, 0.0f, 1.0f));
m_Scene->OnRenderRuntime();   // 内部若无 Primary Camera 则直接 return
m_Framebuffer->Unbind();
```

- **优点**：面板不感知 Scene 内部实现；`OnRenderRuntime` 已经在 P0.2 处理了 early-return 分支，面板只做 Clear
- **缺点**：无

**方案 B：面板自己调用 `Scene::GetPrimaryCameraEntity()` 判空**

```cpp
Entity cam = m_Scene->GetPrimaryCameraEntity();
if (!cam)
{
    // 只 Clear 不渲染
}
else
{
    m_Scene->OnRenderRuntime();
}
```

- **优点**：面板本地可读性略高
- **缺点**：面板越权知道 Scene 的选相机策略；Scene 内部逻辑一旦改动（例如未来支持 CameraStack），面板要同步改

**结论**：采用 **方案 A**。清屏色强制写死 `(0, 0, 0, 1)`，不使用 `EditorPreferences::ViewportClearColor`??因为用户明确要求"无 MainCamera 时纯黑"，而 Scene 面板的清屏色是可配置的，为避免两者耦合，Game 面板独立使用固定黑色。

### 3.3 清屏色策略

**方案 A（推荐）：Game 面板固定 `(0, 0, 0, 1)` 黑色**

- **优点**：符合用户明确要求；无主相机时纯黑一目了然；实现最简
- **缺点**：不可配置

**方案 B：使用 `ColorSettings::ViewportClearColor`（与 Scene 面板一致）**

- **优点**：与 Scene 面板统一，用户改颜色两个面板同步生效
- **缺点**：违反用户"纯黑"的明确诉求

**方案 C：`ColorSettings` 新增 `GameViewportClearColor`**

- **优点**：可配置且独立于 Scene 面板
- **缺点**：P0.6 出口标准没有这条要求，属于超前设计；`EditorPreferences` 的序列化字段也要相应扩展

**结论**：采用 **方案 A**。等未来 `CameraComponent` 有 `ClearFlags`（Skybox / SolidColor / DontClear）后，再让 `Scene::OnRenderRuntime()` 内部根据主相机 ClearColor 覆盖，Game 面板依旧只负责"外层容器清屏"。

### 3.4 Framebuffer 附件规格

**方案 A（推荐）：`{ RGBA8, Depth }`**

- **优点**：Game 面板不做拾取，`RED_INTEGER` 附件是死重量；两个附件比三个附件省显存、省一次 clear
- **缺点**：与 `SceneViewportPanel` 的 FBO 规格不同??但两者本就语义不同，不必强行统一

**方案 B：与 `SceneViewportPanel` 一致 `{ RGBA8, RED_INTEGER, Depth }`**

- **优点**：两个视口 FBO 规格一致，未来复用简单
- **缺点**：多一个整型附件浪费显存 / clear 开销；对 Renderer3D pipeline 里"是否需要写入 Entity ID"的分支也无意义（Game 面板不需要拾取）

**结论**：采用 **方案 A**。

### 3.5 Renderer3D pipeline 尺寸同步

Renderer3D 是全局单例，`ResizePipeline(w, h)` 会把 ShadowPass、OpaquePass、PostProcessPass 等所有 Pass 的内部 FBO 都设为传入尺寸。Scene 面板已在 Resize 时调用它，Game 面板若也调用会与 Scene 面板互相踩踏。

**方案 A（推荐）：Game 面板 **不调用** `Renderer3D::ResizePipeline`**

- **优点**：避免与 Scene 面板争抢 pipeline 尺寸；Scene 面板尺寸决定 pipeline 内部 Pass 尺寸，Game 面板通过 `Renderer3D::SetTargetFramebuffer(m_Framebuffer)` 让最终 Blit 写到自己的 FBO 里，`ImGui::Image` 显示时按 `m_ViewportSize` 采样，尺寸差异由采样自动处理（会缩放，可接受）
- **缺点**：如果 Game 面板显著大于 Scene 面板，最终会看到低分辨率放大后的画面（属于当前架构下的已知限制，P0.6 阶段不解决）

**方案 B：Game 面板每帧根据自身尺寸调用 `Renderer3D::ResizePipeline`**

- **优点**：Game 面板画面清晰
- **缺点**：两个面板同帧内交替 resize pipeline，代价极高（每帧 destroy/recreate 一堆 FBO）；Scene 面板拿到的其实是被 Game 面板改过的尺寸

**方案 C：面板各自持有独立 pipeline 实例**

- **优点**：真正隔离
- **缺点**：Renderer3D 目前是单例架构，改造成本大，超出 P0.6 范围

**结论**：采用 **方案 A**（本 Phase 出口标准明确"Renderer3D 是全局单例"这一限制被接受）。Scene 面板依旧负责 pipeline 尺寸；Game 面板只做"外层 FBO Resize + Scene 视口 Resize"。

### 3.6 `Scene::OnViewportResize` 的调用时机

Scene 面板已经在自身 Resize 时调 `Scene::OnViewportResize`，这会更新场景内所有非 `FixedAspectRatio` 的 `CameraComponent`。

**方案 A（推荐）：Game 面板 **也调用** `Scene::OnViewportResize`**

- **优点**：Game 面板尺寸变化时主相机的 aspect 立即跟随更新，画面构图正确
- **缺点**：与 Scene 面板存在写竞争??最后 resize 的面板决定 CameraComponent 的 aspect

**方案 B：Game 面板不调用**

- **优点**：无写竞争
- **缺点**：Scene 面板尺寸变化会驱动 aspect，Game 面板独立浮动时看到的是"按 Scene 面板 aspect 算出的主相机画面"，构图与 Game 面板实际宽高比不匹配

**结论**：采用 **方案 A**。理由：Game 面板才是"以主相机 aspect 为准"的视图，Scene 面板本质上只是把它当作参考。写竞争在两个视口 aspect 不同时会产生"以最后 resize 者胜"的现象，属于当前架构下的已知限制。未来做 Free Aspect / Fixed Aspect 时再重构。

### 3.7 默认布局：Game 与 Scene 停靠关系

**方案 A（推荐）：同一 Dock 节点 Tab 共享**

```cpp
ImGui::DockBuilderDockWindow("Scene", dockViewport);
ImGui::DockBuilderDockWindow("Game",  dockViewport);   // 同一节点
```

- **优点**：对齐 Unity 默认布局（Scene / Game 在同位置以 Tab 切换）；不占用额外屏幕空间
- **缺点**：初次打开只能看到一个 tab；用户如需同时看到需手动拖出

**方案 B：Scene 在中间、Game 单独占屏幕右侧或下方**

- **优点**：两个视口同时可见
- **缺点**：屏幕空间紧张；与 Unity 默认布局不一致，用户认知负担

**结论**：采用 **方案 A**（Tab 共享）。

### 3.8 Game 面板是否响应事件

**方案 A（推荐）：完全不重写 `OnEvent`**

- **优点**：`EditorPanel::OnEvent` 的默认实现是 no-op；Game 面板对键盘 / 鼠标事件完全透明，不与 Scene 面板抢焦点
- **缺点**：未来接 `Input` 系统时，"运行时按键要从哪个面板消费"需要重新设计（P1+ 的事)

**方案 B：Game 面板消费 Input 事件（Focused 时）**

- **优点**：为未来 Play 模式下从 Game 面板接管输入做准备
- **缺点**：本 Phase 无需求；`Input` 系统还未事件化（Phase 3 才做）

**结论**：采用 **方案 A**。

---

## 4. 实现步骤

按下列顺序落地，每一步落地后编辑器都能编译运行（渐进可验证）。

### Step 1：新建 `GameViewportPanel` 类

**文件**：`Luck3DApp/Source/Panels/GameViewportPanel.h`

```cpp
#pragma once

#include "Lucky/Editor/EditorPanel.h"
#include "Lucky/Renderer/Framebuffer.h"
#include "Lucky/Scene/Scene.h"
#include "Lucky/Scene/SceneManager.h"

#include <glm/glm.hpp>

namespace Lucky
{
    /// <summary>
    /// Game 面板：使用场景内 Primary CameraComponent 渲染游戏视角
    /// 不绘制任何编辑器 Overlay（Grid / Gizmo / Outline / Frustum）
    /// 无主相机时显示纯黑
    /// </summary>
    class GameViewportPanel : public EditorPanel
    {
    public:
        GameViewportPanel() = default;
        GameViewportPanel(const Ref<Scene>& scene);
        ~GameViewportPanel() override;

        void OnUpdate(DeltaTime dt) override;
        void OnGUI() override;
    private:
        Ref<Scene> m_Scene;
        Ref<Framebuffer> m_Framebuffer;

        glm::vec2 m_ViewportSize = { 0.0f, 0.0f };

        SceneManager::SubscriptionHandle m_SceneChangedSub = 0;
    };
}
```

**文件**：`Luck3DApp/Source/Panels/GameViewportPanel.cpp`

```cpp
#include "GameViewportPanel.h"

#include "Lucky/Renderer/RenderCommand.h"
#include "Lucky/Renderer/Renderer3D.h"

#include "imgui/imgui.h"

namespace Lucky
{
    GameViewportPanel::GameViewportPanel(const Ref<Scene>& scene)
        : m_Scene(scene)
    {
        SetFlags(ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);

        FramebufferSpecification fbSpec;
        fbSpec.Attachments =
        {
            FramebufferTextureFormat::RGBA8,
            FramebufferTextureFormat::Depth
        };
        fbSpec.Width = 1280;
        fbSpec.Height = 720;

        m_Framebuffer = Framebuffer::Create(fbSpec);

        m_SceneChangedSub = SceneManager::Subscribe([this](const Ref<Scene>& newScene)
        {
            m_Scene = newScene;
        });
    }

    GameViewportPanel::~GameViewportPanel()
    {
        SceneManager::Unsubscribe(m_SceneChangedSub);
    }

    void GameViewportPanel::OnUpdate(DeltaTime dt)
    {
        if (FramebufferSpecification spec = m_Framebuffer->GetSpecification();
            m_ViewportSize.x > 0.0f && m_ViewportSize.y > 0.0f &&
            (spec.Width != m_ViewportSize.x || spec.Height != m_ViewportSize.y))
        {
            m_Framebuffer->Resize(static_cast<uint32_t>(m_ViewportSize.x), static_cast<uint32_t>(m_ViewportSize.y));

            if (m_Scene)
            {
                m_Scene->OnViewportResize(static_cast<uint32_t>(m_ViewportSize.x), static_cast<uint32_t>(m_ViewportSize.y));
            }
        }

        if (!m_Scene)
        {
            return;
        }

        m_Framebuffer->Bind();

        constexpr glm::vec4 blackClear{ 0.0f, 0.0f, 0.0f, 1.0f };
        RenderCommand::SetClearColor(blackClear);
        RenderCommand::Clear();

        Renderer3D::SetTargetFramebuffer(m_Framebuffer);
        Renderer3D::SetClearColor(blackClear);

        // 关键：清空描边集合，避免 Scene 面板上一帧留下的选中集合污染 Game 面板
        Renderer3D::SetOutlineEntities({});

        m_Scene->OnRenderRuntime();

        m_Framebuffer->Unbind();
    }

    void GameViewportPanel::OnGUI()
    {
        ImVec2 viewportPanelSize = ImGui::GetContentRegionAvail();
        m_ViewportSize = { viewportPanelSize.x, viewportPanelSize.y };

        uint32_t textureID = m_Framebuffer->GetColorAttachmentRendererID();
        ImGui::Image(reinterpret_cast<void*>(static_cast<uintptr_t>(textureID)),
                     ImVec2{ m_ViewportSize.x, m_ViewportSize.y },
                     ImVec2{ 0, 1 }, ImVec2{ 1, 0 });
    }
}
```

**关键点解释**：

1. `SetFlags` 关滚动条：`ImGui::Image` 遇到面板缩放的一瞬间可能撑出滚动条，与 Scene 面板处理方式一致
2. `Renderer3D::SetOutlineEntities({})` 清空：由于 `Renderer3D` 是全局单例，Scene 面板上一帧调用 `SetOutlineEntities(selectedIDs)` 后没有主动清空。若 Game 面板在 Scene 面板 `OnUpdate` 之后执行，其提交的 Draw 会带着选中集合走一遍 Silhouette 逻辑（无害，但会写额外 FBO），显式清空更干净
3. `m_Scene == nullptr` 时直接 return：`SceneManager` 极端情况下（如 `Shutdown` 期间）可能没有 ActiveScene；这种保护性判空对齐 `SceneViewportPanel` 的健壮性

### Step 2：EditorLayer 注册面板

**文件**：`Luck3DApp/Source/EditorLayer.cpp`

**改动 A**：文件头 `include`

```cpp
#include "Panels/GameViewportPanel.h"
```

**改动 B**：PANEL_ID 宏区（对齐现有格式）

```cpp
#define SCENE_HIERARCHY_PANEL_ID "SceneHierarchyPanel"
#define SCENE_VIEWPORT_PANEL_ID "SceneViewportPanel"
#define GAME_VIEWPORT_PANEL_ID "GameViewportPanel"    // 新增
#define INSPECTOR_PANEL_ID "InspectorPanel"
// ...
```

**改动 C**：`OnAttach` 注册（放在 SceneViewportPanel 注册之后）

```cpp
m_PanelManager->AddPanel<SceneViewportPanel>(SCENE_VIEWPORT_PANEL_ID, "Scene", true, placeholder);
m_PanelManager->AddPanel<GameViewportPanel>(GAME_VIEWPORT_PANEL_ID, "Game", true, placeholder);    // 新增
m_PanelManager->AddPanel<InspectorPanel>(INSPECTOR_PANEL_ID, "Inspector", true, placeholder);
```

`displayName` 用 `"Game"`??必须与 `EditorLayoutManager::ApplyDefaultLayout` 中 `DockBuilderDockWindow("Game", ...)` 的字符串一致。

**改动 D**：`Window → Panels` 菜单追加一项（放在 `Scene` 项之后）

```cpp
if (ImGui::MenuItem("Scene"))
{
    uint32_t panelID = Hash::GenerateFNVHash(SCENE_VIEWPORT_PANEL_ID);
    PanelData* panelData = m_PanelManager->GetPanelData(panelID);
    panelData->IsOpen = true;
}

if (ImGui::MenuItem("Game"))    // 新增
{
    uint32_t panelID = Hash::GenerateFNVHash(GAME_VIEWPORT_PANEL_ID);
    PanelData* panelData = m_PanelManager->GetPanelData(panelID);
    panelData->IsOpen = true;
}
```

**改动 E**：`Window → Layouts → Default` 重置布局时把 Game 面板加入 `OpenPanel` 列表

```cpp
OpenPanel(SCENE_HIERARCHY_PANEL_ID);
OpenPanel(SCENE_VIEWPORT_PANEL_ID);
OpenPanel(GAME_VIEWPORT_PANEL_ID);    // 新增
OpenPanel(INSPECTOR_PANEL_ID);
OpenPanel(PROJECT_ASSETS_PANEL_ID);
OpenPanel(RENDER_PIPELINE_PANEL_ID);
```

### Step 3：默认布局停靠

**文件**：`Luck3DApp/Source/EditorLayoutManager.cpp`

在 `ApplyDefaultLayout` 中 `DockBuilderDockWindow("Scene", dockViewport)` 之后立即追加一行：

```cpp
ImGui::DockBuilderDockWindow("Scene", dockViewport);
ImGui::DockBuilderDockWindow("Game",  dockViewport);   // ← 新增，与 Scene 同节点形成 Tab
ImGui::DockBuilderDockWindow("Project", dockBottom);
```

顶部 ASCII 布局图注释可保持不变（Tab 关系在图里不体现）。

---

## 5. 坑点提醒

1. **Renderer3D 状态在 Scene / Game 面板间的顺序耦合**
   `Renderer3D::SetTargetFramebuffer / SetClearColor / SetOutlineEntities` 是全局状态。面板执行顺序由 `PanelManager` 内注册顺序决定（当前 Scene 先注册、Game 后注册）。Game 面板在自身 `OnUpdate` 起始处必须显式调用 `SetTargetFramebuffer(m_Framebuffer) + SetOutlineEntities({})`，覆盖 Scene 面板留下的状态。反过来，Scene 面板每帧自身也会重新 `SetOutlineEntities`，不会被 Game 面板影响。

2. **Pipeline FBO 尺寸争抢（已知限制）**
   `Renderer3D::ResizePipeline` 会把 ShadowPass、OpaquePass 等内部 Pass FBO 全部改成传入尺寸。Scene 面板在自身 Resize 时会调用它，Game 面板**不会**。当两个面板尺寸不同时，pipeline 内部 Pass 走 Scene 面板尺寸，Game 面板的 FBO 尺寸独立，最终 Blit 会经过一次拉伸采样??画质略有损失但不影响功能。当前 Renderer3D 单例架构下这是可接受的。

3. **Scene 面板与 Game 面板 aspect 写竞争**
   两者都会调用 `Scene::OnViewportResize`，导致场景内非 `FixedAspectRatio` 的 CameraComponent aspect 被"最后 resize 的面板"覆盖。绝大多数情况下 Game 面板尺寸变化频率较低（用户很少同时缩放两个面板），最后 resize 者是 Scene 面板；主相机 aspect 略微偏离 Game 面板的正确值，肉眼几乎不可见。未来支持 `CameraComponent.FixedAspectRatio = true` 时用户可以强制主相机使用固定 aspect 规避此问题。

4. **`OnRenderRuntime` early-return 场景**
   `Scene::OnRenderRuntime()` 内部若 `GetPrimaryCameraEntity()` 返回无效 Entity 则直接 return。Game 面板已在此之前完成 `RenderCommand::Clear`（黑色），因此显示效果就是纯黑，符合需求。

5. **无 Scene（`m_Scene == nullptr`）保护**
   编辑器 `Shutdown` 阶段 `SceneManager::Shutdown()` 会先于 `PanelManager` 释放，如果面板 dtor 期间还有 `OnUpdate` 被调用（罕见但存在于层级复杂时），`m_Scene` 可能已为空。`OnUpdate` 首部判空 return 是必要的健壮性保护。

6. **面板显示名 vs PanelID 的关系**
   - `PanelID`（`GAME_VIEWPORT_PANEL_ID = "GameViewportPanel"`）：用于 `PanelManager` 内部哈希索引，`Hash::GenerateFNVHash` 计算
   - `displayName`（`"Game"`）：ImGui 窗口标题，`DockBuilderDockWindow` 匹配的也是这个字符串
   - 两者必须严格一致，尤其是 `EditorLayoutManager::ApplyDefaultLayout` 中的 `"Game"` 字面量与 `AddPanel<GameViewportPanel>(...., "Game", ...)` 的第二参数必须逐字符相同（区分大小写）

---

## 6. 验收标准

对应 Roadmap 中 P0.6 出口标准 + 用户明确诉求：

### 6.1 面板可见性

- [x] 首次启动编辑器：Default Layout 中 `Scene` / `Game` 以 Tab 形式共享同一 Dock 节点，`Scene` 是激活 tab
- [x] 点击 `Window → Panels → Game` 可打开 / 保持打开状态
- [x] 关闭 Game 面板后点击 `Window → Layouts → Default` 可以重新展开
- [x] 拖动 Game tab 可以拉出成独立浮动 / 拖到其他 Dock 节点

### 6.2 渲染行为

**Edit 态（Play 前）**：

- [x] 场景中**存在** Primary CameraComponent：Game 面板显示主相机视角画面
- [x] 场景中**不存在** Primary CameraComponent（或删除了主相机）：Game 面板显示**纯黑**（`(0, 0, 0, 1)`）
- [x] Game 面板中**看不到** Grid、Gizmo（位移/旋转/缩放手柄）、View Orientation Gizmo、选中描边、Light Gizmo、Camera Frustum
- [x] Scene 面板行为完全不变（拾取、Gizmo、拖 .luck3d 均正常）

**Play 态（点击 Play 按钮后）**：

- [x] `Scene::OnUpdateRuntime` 每帧唯一一次调用（未来接脚本后要能验证钩子只跑一次）
- [x] Game 面板显示主相机视角、Scene 面板依旧显示 EditorCamera 视角
- [x] `Scene::Copy` 生效??Game 面板显示的是副本，编辑源场景不受影响
- [x] 点击 Stop：Game 面板回到编辑态源场景的主相机画面，与 Play 前一致

**Pause 态**：

- [x] Scene / Game 面板画面停留在暂停帧，但相机操作（Scene 面板 EditorCamera 拖动）仍可用

### 6.3 交互行为

- [x] 鼠标点击 Game 面板：**不触发**实体拾取（Scene 面板才拾取）
- [x] 键盘按 Q / W / E / R（Gizmo 类型切换）：Game 面板不响应，Scene 面板正常响应
- [x] 拖 `.luck3d` 到 Game 面板：无反应（Scene 面板才响应）

### 6.4 性能 / 无回归

- [x] 每帧 `Renderer3D::ResetStats()` 只清零一次（EditorLayer::OnUpdate 已做），Draw Call 统计包括 Scene + Game 两个面板的贡献
- [x] 关闭 Game 面板后，其 `OnUpdate` 由 `PanelManager` 跳过（依据 `PanelData::IsOpen`），不产生任何 GPU 开销

---

## 7. 后续 Phase 的扩展点（仅作说明，本 Phase 不实现）

- **`CameraComponent.ClearFlags`**（Skybox / SolidColor / DontClear / DepthOnly）：由 `Scene::OnRenderRuntime` 内部按主相机 ClearFlags 决定清屏行为，Game 面板依旧只负责外层清屏
- **多相机 CameraStack**：`Scene::OnRenderRuntime` 按 Depth 排序遍历所有 CameraComponent，Game 面板依旧只调用一次
- **Play 模式下 Input 从 Game 面板消费**：Phase 3 事件化 Input 时补 `OnEvent` 与 Focus 检测
- **Free Aspect / Fixed Aspect / Resolution 下拉**：工具栏样式，本 Phase 无此需求（Roadmap P0.6 未列入）
- **"No cameras rendering" 提示文字**：如需，可在 `OnGUI` 中判空后叠一个 `ImGui::TextUnformatted` 居中显示；本 Phase 明确不做

---

## 8. 参考

- `SceneViewportPanel`（`Luck3DApp/Source/Panels/SceneViewportPanel.*`）：作为 Game 面板的骨架来源
- `Scene::OnRenderRuntime` / `Scene::GetPrimaryCameraEntity`（Phase 0.2）
- Unity Editor Game View：布局与交互模型参考
