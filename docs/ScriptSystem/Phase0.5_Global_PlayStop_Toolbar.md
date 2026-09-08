# Phase 0.5：全局 Play/Pause 工具条

## 1. 概述

P0.5 目标：在 `EditorLayer` 顶部（`MainMenuBar` 之下、`DockSpace` 之上）新增一条**全局工具条**，把 **Play / Pause** 两个按钮以水平居中的方式集中呈现，作为整个编辑器切换 `SceneState::Edit / Play / Pause` 的唯一入口。按钮模型对齐 Unity 编辑器（合并 Play/Stop、Pause 支持"预暂停"）。

### 1.1 按钮模型（对齐 Unity）

工具条只有 **2 个按钮**，各自是一个独立的 Toggle：

- **Play 按钮**：切换"是否运行时"
  - Edit → Play：进入运行时，按钮变蓝
  - Play/Pause → Edit：退出运行时，按钮恢复常态（相当于 Stop）
- **Pause 按钮**：切换"暂停意图"（`PauseArmed` 位）
  - 编辑态下点 Pause：仅置蓝色高亮，不动 Scene（"预暂停"）；之后按 Play 会直接进入 Pause 态
  - 运行态下点 Pause：Scene 立即进入 Pause，按钮变蓝
  - 蓝色高亮下再点 Pause：清除预暂停 / 恢复运行，按钮恢复常态

按 (IsPlaying, PauseArmed) 两个正交布尔位组合出 4 种 UI 状态：

| # | IsPlaying | PauseArmed | SceneState | Play 按钮 | Pause 按钮 |
|---|:---:|:---:|:---:|:---:|:---:|
| 1 | false | false | Edit | 常态 | 常态 |
| 2 | false | true | Edit（**预暂停**） | 常态 | **蓝** |
| 3 | true | false | Play | **蓝** | 常态 |
| 4 | true | true | Pause | **蓝** | **蓝** |

**语义要点**：

- 状态 #2 下 `Scene` 层面仍是 `Edit`，`Scene::Copy` 尚未发生，`OnRuntimeStart` 未调用 ?? `PauseArmed` 是**纯 UI 意图位**
- Play 按钮从 false → true 时，若 `PauseArmed==true`，则在**同一帧内**先 `OnScenePlay` 再 `SetScenePaused(true)`，直接进 Pause 态
- **Stop 时清零 `PauseArmed`**：从 Play/Pause 退回 Edit 时，`PauseArmed` 一律清零；下次进入 Play 从干净状态开始

### 1.2 具体产出

1. `SceneManager` 新增 3 个运行态切换接口：`OnScenePlay / OnSceneStop / SetScenePaused`
2. `EditorIconManager` 扩展 2 个工具条图标获取接口：`GetPlayIcon / GetPauseIcon`
3. 新增 `EditorToolbar` 类（`Luck3DApp/Source/`），持有 `m_PauseArmed` 位，绘制 Play / Pause 两个 Toggle 按钮
4. `EditorLayer::OnImGuiRender` 集成 Toolbar：在 `UI_DrawMenuBar()` 之后调用 `m_EditorToolbar.ImGuiRender()`，并在 `viewport->WorkOffsetMin.y` 上累加 Toolbar 高度，让 DockSpace 自动下移
5. `EditorDockSpace::ImGuiRender` 把 `viewport->Pos / Size` 改为 `viewport->WorkPos / WorkSize`

### 1.3 前置依赖

- P0.1 完成（`SceneState::Edit/Play/Pause`、`Scene::OnRuntimeStart/Stop`、`OnUpdateEditor/Runtime` 已就位）
- P0.3 完成（`Scene::Copy` 深拷贝已就位）
- P0.4 完成（`ComponentRegistry` 已就位，`Scene::Copy` 走 Registry 遍历，本 Phase 不改动）

### 1.4 本 Phase **不做**的事

- **不做 Game 面板**（P0.6）?? 本 Phase Play 后 Scene 面板通过 EditorCamera 渲染副本
- **不接入脚本 / 物理 tick**（P1+）?? `OnRuntimeStart/Stop` 内部当前是空实现，本 Phase 保持不变
- **不做工具条快捷键**（如 Ctrl+P）?? 视觉入口先落地，快捷键留给后续可用性 Phase
- **不做工具条右侧按钮**（网格 / Snap / 坐标系 / Local-Global 切换）?? 只做居中的 Play / Pause 两个按钮，右侧留白
- **不改任何面板代码** ?? 面板通过 `SceneManager::Subscribe` 订阅切换事件的既有机制天然覆盖 Play 时的场景切换

---

## 2. 涉及的文件

### 需要修改

| 文件 | 说明 |
|------|------|
| `Lucky/Source/Lucky/Editor/EditorIconManager.h` | 新增 `GetPlayIcon / GetPauseIcon` 2 个静态接口声明 |
| `Lucky/Source/Lucky/Editor/EditorIconManager.cpp` | `EditorIconData` 新增 2 个 `Ref<Texture2D>` 字段；`Init` 加载 ToolBar 图标；`Shutdown` 释放；实现 2 个 Get 接口 |
| `Lucky/Source/Lucky/Scene/SceneManager.h` | 新增 `OnScenePlay / OnSceneStop / SetScenePaused` 3 个静态接口声明 |
| `Lucky/Source/Lucky/Scene/SceneManager.cpp` | 实现上述 3 个接口；匿名命名空间新增 `s_EditorScene`（进入 Play 前的编辑态快照） |
| `Luck3DApp/Source/EditorDockSpace.cpp` | 把 `viewport->Pos / Size` 改为 `viewport->WorkPos / WorkSize` |
| `Luck3DApp/Source/EditorLayer.h` | 新增成员 `EditorToolbar m_EditorToolbar;`；`#include "EditorToolbar.h"` |
| `Luck3DApp/Source/EditorLayer.cpp` | `OnImGuiRender` 中在 `UI_DrawMenuBar()` 之后追加 `m_EditorToolbar.ImGuiRender()`，并累加 `viewport->WorkOffsetMin.y` |

### 需要新建

| 文件 | 说明 |
|------|------|
| `Luck3DApp/Source/EditorToolbar.h` | `EditorToolbar` 类声明 |
| `Luck3DApp/Source/EditorToolbar.cpp` | `EditorToolbar` 类实现 |

### 无需修改

- 所有面板（`SceneViewportPanel / SceneHierarchyPanel / InspectorPanel / ProjectAssetsPanel / RenderPipelinePanel / LightingPanel`）：面板 ctor 已通过 `SceneManager::Subscribe` 订阅切换事件，Play/Stop 时的 ActiveScene 切换会自动广播
- `Scene::OnRuntimeStart / OnRuntimeStop`：状态切换语义已在 P0.1 落地
- `Scene::Copy`：本 Phase 直接复用

### 资源清单

图标已全部就位，本 Phase **不需要**任何新资源：

```
Luck3DApp/Resources/Icons/ToolBar/
├── Play.png     （已就位）
└── Pause.png    （已就位）
```

采用"合并 Play/Stop"模型后，**不需要 Stop.png**。

---

## 3. 现状回顾

### 3.1 EditorLayer 的 ImGui 渲染顺序

对照 [EditorLayer.cpp](../../Luck3DApp/Source/EditorLayer.cpp) `OnImGuiRender()`：

```cpp
void EditorLayer::OnImGuiRender()
{
    m_EditorDockSpace.ImGuiRender();   // Begin/End "DockSpace" 窗口 + DockSpace()
    UI_DrawMenuBar();                  // BeginMainMenuBar / EndMainMenuBar
    m_PanelManager->OnImGuiRender();   // 各面板
}
```

三者是**平级**的，彼此独立：

- `EditorDockSpace` 内部自己 `Begin("DockSpace")` 一个覆盖整个主 viewport 的窗口
- `UI_DrawMenuBar()` 用 `BeginMainMenuBar()`，这是 ImGui 的 **viewport 级主菜单栏**，不依附于 "DockSpace" 窗口的 `WindowFlags_MenuBar` flag（DockSpace 窗口虽带该 flag，但从未 `BeginMenuBar`，flag 实际上没用）
- `PanelManager` 里每个面板都是独立的 dockable 窗口

### 3.2 EditorDockSpace 当前定位方式

对照 [EditorDockSpace.cpp](../../Luck3DApp/Source/EditorDockSpace.cpp)：

```cpp
ImGui::SetNextWindowPos(viewport->Pos);
ImGui::SetNextWindowSize(viewport->Size);
```

用的是 `Pos / Size` ?? 覆盖 **整个 viewport**，不扣 MainMenuBar 高度。

ImGui 里 `MainMenuBar` 会自动累加 `viewport->WorkOffsetMin.y`，也就是说 `WorkPos / WorkSize` 会**自动**扣掉 MainMenuBar 已占用的顶部区域。当前 `Pos / Size` 之所以视觉上无异常，是因为 `DockSpace()` 内部会做溢出裁剪 ?? DockSpace 被 MainMenuBar 压在下面，只是没有溢出到屏幕外。

必须切到 `WorkPos / WorkSize`，因为 P0.5 之后 MainMenuBar 之下会插入一条新的 Toolbar，DockSpace 必须"看见"这段新增的顶部占用才能正确下移。

### 3.3 SceneManager 现状

对照 [SceneManager.h](../../Lucky/Source/Lucky/Scene/SceneManager.h)：

- `GetActiveScene / SetActiveScene`：ActiveScene 的读写唯一入口
- `OpenScene(handle) / OpenScene(path) / NewSceneWithDialog / SaveSceneAs`：场景生命周期入口
- `Subscribe / Unsubscribe`：所有面板通过订阅 `SceneChangedCallback` 自动同步

已经具备"广播 ActiveScene 切换"的机制，本 Phase 只需在 `SceneManager` 里加**运行态切换入口**，内部完成"深拷贝 → SetActiveScene 广播"两步，面板自动同步到副本 Scene。

### 3.4 Scene 运行状态相关接口

对照 [Scene.h](../../Lucky/Source/Lucky/Scene/Scene.h)：

```cpp
SceneState GetState() const;
void SetState(SceneState state);       // 不触发任何回调，仅赋值
void OnRuntimeStart();                 // 内部把 State 置为 Play
void OnRuntimeStop();                  // 内部把 State 置为 Edit
```

Pause / Resume 走 `SetState(Pause) / SetState(Play)`：目前无独立事件钩子，`OnUpdateRuntime` 内部会根据 `m_State` 跳过脚本/物理 tick，天然覆盖 Pause 语义。

### 3.5 EditorIconManager 现状

对照 [EditorIconManager.cpp](../../Lucky/Source/Lucky/Editor/EditorIconManager.cpp)：

- 有 `s_IconRootPath = "Resources/Icons"`（当前工作目录为 `Luck3DApp/`，拼出的相对路径正确）
- 有 `LoadIcon(relativePath)` 内部 helper
- 每种类别的图标都有独立字段和独立 `Get*Icon()` 接口
- 图标接口一律返回 `const Ref<Texture2D>&` [[memory:njikb5xe]]

新增 2 个 Toolbar 图标只需照抄 `SettingsIcon` 的模式。

---

## 4. 详细设计

### 4.1 SceneManager 新增运行态切换接口

#### 4.1.1 接口声明（SceneManager.h）

在 `// ---- 场景打开 / 新建 ----` 段落**之后**、`// ---- 事件订阅 ----` **之前**新增一段 `// ---- 运行态切换 ----`：

```cpp
// ---- 运行态切换 ----

/// <summary>
/// 进入运行态：Edit → Play（Toolbar Play 按钮从常态切到高亮时调用）
/// 
/// 内部流程：
/// 1. 若当前 ActiveScene 不处于 Edit，直接返回（幂等）
/// 2. 保存当前 ActiveScene 为编辑态快照 s_EditorScene
/// 3. 调用 Scene::Copy 深拷贝出 runtimeScene
/// 4. runtimeScene->OnRuntimeStart() 切换状态到 Play
/// 5. SetActiveScene(runtimeScene) 广播到所有订阅方
/// </summary>
static void OnScenePlay();

/// <summary>
/// 退出运行态：Play/Pause → Edit（Toolbar Play 按钮从高亮切回常态时调用）
/// 
/// 内部流程：
/// 1. 若当前 ActiveScene 处于 Edit，直接返回（幂等）
/// 2. 当前 runtimeScene->OnRuntimeStop() 切换状态到 Edit
/// 3. SetActiveScene(s_EditorScene) 还原到编辑态快照，广播到所有订阅方
/// 4. 清空 s_EditorScene
/// </summary>
static void OnSceneStop();

/// <summary>
/// 设置暂停位（Toolbar Pause 按钮在运行时点击时调用）
/// 
/// - 仅在 ActiveScene 处于 Play/Pause 时生效；Edit 态下为 no-op（预暂停由 Toolbar 内部维护）
/// - 直接 SetState(Pause / Play)，不切换 ActiveScene，不广播
/// </summary>
/// <param name="paused">true = 进入 Pause；false = 恢复 Play</param>
static void SetScenePaused(bool paused);
```

#### 4.1.2 接口实现（SceneManager.cpp）

在 `SceneManager.cpp` 匿名命名空间新增：

```cpp
namespace
{
    // ...（现有其他静态字段）...

    // 进入 Play 前保存的编辑态原始 Scene；Stop 时用来还原 ActiveScene
    // Edit 状态下始终为 null；Play/Pause 状态下持有原始编辑态 Scene 的 Ref
    Ref<Scene> s_EditorScene;
}
```

三个接口实现：

```cpp
void SceneManager::OnScenePlay()
{
    const Ref<Scene>& active = GetActiveScene();
    if (!active || active->GetState() != SceneState::Edit)
    {
        return;  // 无场景或已在运行态：幂等
    }

    s_EditorScene = active;

    Ref<Scene> runtimeScene = Scene::Copy(active);
    runtimeScene->OnRuntimeStart();

    SetActiveScene(runtimeScene);
}

void SceneManager::OnSceneStop()
{
    const Ref<Scene>& active = GetActiveScene();
    if (!active || active->GetState() == SceneState::Edit)
    {
        return;
    }

    active->OnRuntimeStop();

    Ref<Scene> editorScene = s_EditorScene;
    s_EditorScene.reset();

    SetActiveScene(editorScene);
}

void SceneManager::SetScenePaused(bool paused)
{
    const Ref<Scene>& active = GetActiveScene();
    if (!active || active->GetState() == SceneState::Edit)
    {
        return;  // Edit 态下不动 Scene：预暂停位由 Toolbar 内部持有
    }

    active->SetState(paused ? SceneState::Pause : SceneState::Play);
}
```

`Shutdown()` 内需追加 `s_EditorScene.reset();`，避免退出程序时残留引用。

### 4.2 EditorIconManager 扩展 Toolbar 图标

#### 4.2.1 EditorIconData 新增字段（EditorIconManager.cpp）

在 `EditorIconData` 结构体末尾新增：

```cpp
// ---- 工具条图标 ----
Ref<Texture2D> PlayIcon;            // 播放按钮图标
Ref<Texture2D> PauseIcon;           // 暂停按钮图标
```

#### 4.2.2 Init 内加载

在 `Init()` 内 `SettingsIcon` 加载之后、`AssetTypeIcons` 之前追加：

```cpp
// ---- 加载工具条图标 ----
s_IconData.PlayIcon  = LoadIcon("ToolBar/Play.png");
s_IconData.PauseIcon = LoadIcon("ToolBar/Pause.png");
```

#### 4.2.3 Shutdown 内释放

```cpp
s_IconData.PlayIcon.reset();
s_IconData.PauseIcon.reset();
```

#### 4.2.4 EditorIconManager.h 新增接口

在 `GetSettingsIcon()` 声明**之后**追加：

```cpp
// ======== 工具条图标 ========

/// <summary>
/// 获取播放按钮图标
/// </summary>
static const Ref<Texture2D>& GetPlayIcon();

/// <summary>
/// 获取暂停按钮图标
/// </summary>
static const Ref<Texture2D>& GetPauseIcon();
```

#### 4.2.5 实现

```cpp
const Ref<Texture2D>& EditorIconManager::GetPlayIcon()
{
    return s_IconData.PlayIcon;
}

const Ref<Texture2D>& EditorIconManager::GetPauseIcon()
{
    return s_IconData.PauseIcon;
}
```

### 4.3 EditorToolbar 新类

#### 4.3.1 EditorToolbar.h

放在 `Luck3DApp/Source/EditorToolbar.h`：

```cpp
#pragma once

namespace Lucky
{
    /// <summary>
    /// 编辑器全局工具条：MainMenuBar 之下、DockSpace 之上，贴顶显示 Play / Pause 两个 Toggle 按钮
    /// 
    /// 按钮模型（对齐 Unity）：
    /// - Play：切换"是否运行时"，蓝色高亮表示已进入运行时
    /// - Pause：切换"暂停意图"，蓝色高亮表示装载了暂停位（PauseArmed）
    ///   - 编辑态下点 Pause：仅高亮，不动 Scene（"预暂停"）；下次 Play 会直接进 Pause 态
    ///   - 运行态下点 Pause：Scene 立即 Pause / 恢复
    /// - Stop：以"再次点击 Play"实现，无独立按钮
    /// </summary>
    class EditorToolbar
    {
    public:
        /// <summary>
        /// 绘制工具条
        /// 内部自己 Begin 一个不可停靠、不可移动、贴顶宽度铺满的独立窗口
        /// </summary>
        void ImGuiRender();

        /// <summary>
        /// 工具条固定高度，DockSpace 会在其下方渲染
        /// </summary>
        static float GetHeight() { return s_ToolbarHeight; }
    private:
        bool m_PauseArmed = false;                          // 暂停意图位：Edit 态下 Pause 高亮但不影响 Scene

        static constexpr float s_ToolbarHeight = 34.0f;     // 工具条整体高度
        static constexpr float s_ButtonSize    = 24.0f;     // 单个图标按钮边长
        static constexpr float s_ButtonSpacing = 6.0f;      // 相邻按钮的水平间距
    };
}
```

#### 4.3.2 EditorToolbar.cpp

```cpp
#include "EditorToolbar.h"

#include "Lucky/Editor/EditorIconManager.h"
#include "Lucky/Scene/Scene.h"
#include "Lucky/Scene/SceneManager.h"

#include <imgui/imgui.h>
#include <imgui/imgui_internal.h>

namespace Lucky
{
    void EditorToolbar::ImGuiRender()
    {
        const Ref<Scene>& scene = SceneManager::GetActiveScene();
        SceneState state = scene ? scene->GetState() : SceneState::Edit;
        bool isPlaying = (state != SceneState::Edit);

        // ---- 定位：MainMenuBar 之下，宽度铺满 viewport ----
        ImGuiViewport* viewport = ImGui::GetMainViewport();
        ImVec2 pos  = viewport->WorkPos;                                    // 已自动扣掉 MainMenuBar 高度
        ImVec2 size = ImVec2(viewport->WorkSize.x, s_ToolbarHeight);

        ImGui::SetNextWindowPos(pos);
        ImGui::SetNextWindowSize(size);
        ImGui::SetNextWindowViewport(viewport->ID);

        ImGuiWindowFlags flags =
            ImGuiWindowFlags_NoDecoration       |
            ImGuiWindowFlags_NoMove             |
            ImGuiWindowFlags_NoDocking          |
            ImGuiWindowFlags_NoSavedSettings    |
            ImGuiWindowFlags_NoBringToFrontOnFocus;

        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(6.0f, 4.0f));
        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.0f, 0.0f, 0.0f, 0.0f));

        ImGui::Begin("##EditorToolbar", nullptr, flags);

        // ---- 水平居中 2 个按钮 ----
        const float buttonsTotalWidth = s_ButtonSize * 2.0f + s_ButtonSpacing;
        const float availWidth = ImGui::GetContentRegionAvail().x;
        ImGui::SetCursorPosX((availWidth - buttonsTotalWidth) * 0.5f);

        auto DrawIconButton = [](const Ref<Texture2D>& icon, const char* id, bool active) -> bool
        {
            ImVec4 tint = active ? ImVec4(0.4f, 0.8f, 1.0f, 1.0f)   // 高亮：浅蓝
                                 : ImVec4(1.0f, 1.0f, 1.0f, 1.0f);  // 常态：白

            ImTextureID tex = icon ? (ImTextureID)(uintptr_t)icon->GetRendererID() : 0;
            return ImGui::ImageButton(id, tex,
                ImVec2(s_ButtonSize, s_ButtonSize),
                ImVec2(0, 0), ImVec2(1, 1),
                ImVec4(0, 0, 0, 0), tint);
        };

        // ---- Play 按钮：Toggle IsPlaying ----
        if (DrawIconButton(EditorIconManager::GetPlayIcon(), "##Play", isPlaying))
        {
            if (!isPlaying)
            {
                // Edit → Play：如果预暂停位打开，进入后立刻切 Pause
                SceneManager::OnScenePlay();
                if (m_PauseArmed)
                {
                    SceneManager::SetScenePaused(true);
                }
            }
            else
            {
                // Play/Pause → Edit：Stop 时清零预暂停位
                SceneManager::OnSceneStop();
                m_PauseArmed = false;
            }
        }

        ImGui::SameLine(0.0f, s_ButtonSpacing);

        // ---- Pause 按钮：Toggle PauseArmed ----
        if (DrawIconButton(EditorIconManager::GetPauseIcon(), "##Pause", m_PauseArmed))
        {
            m_PauseArmed = !m_PauseArmed;
            if (isPlaying)
            {
                SceneManager::SetScenePaused(m_PauseArmed);
            }
            // 非运行态：仅翻转 UI 位（预暂停），不动 Scene
        }

        ImGui::End();

        ImGui::PopStyleColor();
        ImGui::PopStyleVar();
    }
}
```

#### 4.3.3 关键设计约束

1. **Play 按钮永远可点、判定高亮 = `state != Edit`**：无需 `BeginDisabled`
2. **Pause 按钮永远可点、判定高亮 = `m_PauseArmed`**：Edit 态也能按（用于预暂停）
3. **`m_PauseArmed` 与 `Scene` 状态解耦**：Edit 态下 `m_PauseArmed` 单独存在于 Toolbar，不广播、不影响 Scene；只有运行态下才 `SetScenePaused` 联动 Scene
4. **Stop 时清零 `m_PauseArmed`**：从 Play/Pause 退回 Edit 时一律清零，下次 Play 从干净状态开始（对齐 Unity 实测行为）
5. **进入 Play 时应用 `m_PauseArmed`**：`OnScenePlay` 之后若 `m_PauseArmed==true`，紧跟一个 `SetScenePaused(true)`，同一帧内直接进 Pause 态
6. **`WorkPos`**：MainMenuBar 已经把自身高度累加到 `WorkOffsetMin`，Toolbar 用 `WorkPos` 就能自动贴在 MainMenuBar 下方
7. **窗口 flag**：`NoDecoration`（含 `NoTitleBar / NoResize / NoScrollbar / NoCollapse`）+ `NoMove / NoDocking / NoSavedSettings / NoBringToFrontOnFocus`，确保不可拖动、不可停靠、不记忆位置、不抢焦点

#### 4.3.4 状态迁移速查表

| 当前 (IsPlaying, PauseArmed) | 点 Play → | 点 Pause → |
|:---:|:---:|:---:|
| (F, F) Edit | (T, F)：`OnScenePlay` | (F, T)：仅置 UI 位 |
| (F, T) Edit + 预暂停 | (T, T)：`OnScenePlay` **并**同帧 `SetScenePaused(true)` | (F, F)：仅清 UI 位 |
| (T, F) Play | (F, F)：`OnSceneStop` + 清 UI 位 | (T, T)：`SetScenePaused(true)` |
| (T, T) Pause | (F, F)：`OnSceneStop` + 清 UI 位 | (T, F)：`SetScenePaused(false)` |

### 4.4 EditorDockSpace 视口偏移修正

修改 [EditorDockSpace.cpp](../../Luck3DApp/Source/EditorDockSpace.cpp) 中两行：

```diff
- ImGui::SetNextWindowPos(viewport->Pos);
- ImGui::SetNextWindowSize(viewport->Size);
+ ImGui::SetNextWindowPos(viewport->WorkPos);
+ ImGui::SetNextWindowSize(viewport->WorkSize);
```

**为什么必须切**：ImGui 内部对于**通过 `SetNextWindowPos` 显式定位的窗口**，是否需要下移**必须靠调用方主动读取 `WorkPos`**。

**还有一个坑**：`viewport->WorkOffsetMin` 只有 `MainMenuBar` 会**自动**追加。工具条自己 `Begin` 的窗口**不会自动**更新 `WorkOffsetMin`。所以只切 `WorkPos / WorkSize` 是不够的 ?? DockSpace 会顶到 MainMenuBar 下面、和 Toolbar 重叠。

修正方式见【决策点 1】，推荐在 `EditorLayer::OnImGuiRender` 中手动累加 `viewport->WorkOffsetMin.y += EditorToolbar::GetHeight()`。

### 4.5 EditorLayer 集成

#### 4.5.1 EditorLayer.h

```diff
  #include "EditorDockSpace.h"
+ #include "EditorToolbar.h"
  #include "Lucky/Editor/PanelManager.h"
  ...

  private:
      EditorDockSpace m_EditorDockSpace;
+     EditorToolbar   m_EditorToolbar;
      Scope<PanelManager> m_PanelManager;
```

#### 4.5.2 EditorLayer.cpp OnImGuiRender

采用【决策点 1】方案 A：

```cpp
void EditorLayer::OnImGuiRender()
{
    // ---- 主菜单栏 ----
    UI_DrawMenuBar();

    // ---- 顶部工具条：贴在 MainMenuBar 下方 ----
    ImGuiViewport* viewport = ImGui::GetMainViewport();
    const float toolbarHeight = EditorToolbar::GetHeight();

    m_EditorToolbar.ImGuiRender();

    // 手动把 Toolbar 高度累加到 WorkOffsetMin，DockSpace 用 WorkPos/WorkSize 就会自动下移
    viewport->WorkOffsetMin.y += toolbarHeight;

    // ---- DockSpace ----
    m_EditorDockSpace.ImGuiRender();

    // 复位：不影响下一帧
    viewport->WorkOffsetMin.y -= toolbarHeight;

    // ---- 各面板 ----
    m_PanelManager->OnImGuiRender();
}
```

顺序上先 MainMenuBar → 再 Toolbar → 再 DockSpace，从上到下读起来自然；复位是防御性写法（`WorkOffsetMin` 每帧会被 ImGui 内部重置，理论上不写也可以）。

---

## 5. 决策点

### 决策点 1：Toolbar 与 DockSpace 的布局衔接

Toolbar 是独立 Begin 的窗口，需要让 DockSpace 主动"知道 Toolbar 占了一段顶部空间"。

- **方案 A：在 `EditorLayer::OnImGuiRender` 内手动累加 `viewport->WorkOffsetMin.y`**（★ 推荐）
  - 优点：符合 ImGui `WorkOffset` 的原本语义（"当前 viewport 顶部/底部已经被占用多少像素"）；DockSpace 与 EditorToolbar 解耦；未来加底部状态栏是同样方式（累加 `WorkOffsetMax`）
  - 缺点：`WorkOffsetMin` 属于 `imgui_internal.h` 范围，需要引入内部头；Hazel 就是这么做的，社区默认这条路
- **方案 B：`EditorDockSpace::ImGuiRender` 内部手动补偿 y 偏移**
  - 优点：不需要碰 `imgui_internal.h`
  - 缺点：DockSpace 反向依赖 EditorToolbar 的高度常量；未来再加底部状态栏还得补偿一次；改动源头分散
- **方案 C：把 Toolbar 塞进 DockSpace 窗口的 MenuBar 内**（放弃独立窗口）
  - 优点：零布局衔接问题
  - 缺点：语义混淆（MenuBar 是菜单，不是命令按钮）；图标按钮居中在 MenuBar 内更麻烦；未来 Toolbar 分组扩展（左中右）会撞在一起

**推荐方案 A**：Hazel / Unreal Slate 等主流引擎 UI 层的通行做法，未来扩展一致。

### 决策点 2：Play/Stop 状态切换的调用归属

由 Toolbar 直接调 `Scene::OnRuntimeStart` + `SceneManager::SetActiveScene`，还是抽一层 `SceneManager::OnScenePlay / OnSceneStop`？

- **方案 A：抽一层 SceneManager 静态接口**（★ 推荐）
  - 优点：`SceneManager` 已是 ActiveScene 的唯一真源，编辑态 Scene 备份 `s_EditorScene` 天然归属它；未来快捷键 / 命令行 / 脚本触发 Play 时共享同一份接口；Toolbar 极薄，只关心 UI
  - 缺点：SceneManager 多 3 个静态方法（可接受）
- **方案 B：Toolbar 自己持有编辑态 Scene 备份**
  - 优点：SceneManager 保持不变
  - 缺点：把"编辑态 Scene 备份"放到 UI 层不合适；未来任何非 Toolbar 入口触发 Play 都要重复实现；Toolbar 一旦被换掉备份就丢

**推荐方案 A**：与现有 `SceneManager::OpenScene / NewSceneWithDialog / SaveSceneAs` 保持同一层次。

### 决策点 3：Play/Stop 是否合并为单个按钮

- **方案 A：合并 Play/Stop 为单个 Toggle 按钮**（★ 推荐，已采用）
  - 优点：对齐 Unity 2019.3+ 的实际做法；只需 Play/Pause 两个图标，`Stop.png` 不需要；界面更紧凑
  - 缺点：Stop 语义融合进 Play 按钮的"再次点击"里，需要通过蓝色高亮表明当前是运行态
- **方案 B：保留独立 Stop 按钮**（旧方案）
  - 优点：Stop 入口显式可见
  - 缺点：需要额外准备 Stop.png；三按钮布局占位更宽

**采用方案 A**：用户已明确采用合并模型；同时避免额外图标资源依赖。

### 决策点 4：Pause 按钮的"预暂停"行为（Edit 态可点击）

- **方案 A：Edit 态下 Pause 可点击，仅置蓝色高亮位，不影响 Scene**（★ 推荐，已采用）
  - 优点：对齐 Unity 的实测行为；提供"进入运行的瞬间就暂停"的能力，非常适合逐帧调试运行开头
  - 缺点：需要 Toolbar 内部持有一个额外的 UI 位（`m_PauseArmed`），并在 Play 时应用
- **方案 B：Edit 态下 Pause 灰显禁用**
  - 优点：状态最简，Pause 严格依附 Scene 状态
  - 缺点：无法在进入 Play 之前预先装载暂停位，运行开头一帧无法暂停

**采用方案 A**：用户明确要求这个行为，且贴合 Unity 实际体验。

### 决策点 5：Stop 时 `m_PauseArmed` 是否清零

- **方案 A：Stop 时清零 `m_PauseArmed`**（★ 推荐，已采用）
  - 优点：对齐 Unity 实测行为（Unity 停止时清零）；每次进入 Play 从干净状态开始，行为可预期
  - 缺点：如果用户在 Play 中想"停下来但保留下次直接进 Pause 的意图"，需要停止后再手动点一次 Pause
- **方案 B：Stop 时保留 `m_PauseArmed`**
  - 优点：进 Play 前的预暂停设置可持久；连续多次 Play/Stop 循环调试时省一次点击
  - 缺点：与 Unity 行为不一致；`m_PauseArmed` 的语义变得"跨越 Play/Stop 周期"，隐含了长期状态，容易迷惑

**采用方案 A**：与 Unity 停止时清零行为一致，用户已在实测中确认。

### 决策点 6：`m_PauseArmed` 位的存放位置

- **方案 A：`EditorToolbar` 内部成员**（★ 推荐，已采用）
  - 优点：预暂停本质是 UI 意图，Scene 层完全无感知；与 `EditorToolbar` 的生命周期天然绑定；未来快捷键触发时可以通过 `EditorLayer::m_EditorToolbar` 公开一个成员方法即可
  - 缺点：非 UI 入口若要触发预暂停，需要拿到 Toolbar 引用
- **方案 B：`SceneManager` 内部静态字段**
  - 优点：任何位置都能读写
  - 缺点：把 UI 状态泄漏进 SceneManager，语义不干净；SceneManager 的其他消费者不该关心这个位

**采用方案 A**：预暂停是 UI 层的意图位，不应侵入 SceneManager。

### 决策点 7：`s_EditorScene` 存放位置

- **方案 A：`SceneManager.cpp` 匿名命名空间**（★ 推荐，已采用）
  - 优点：只对 SceneManager 内部函数可见，封装最严
  - 缺点：无
- **方案 B：`SceneManager` 私有静态成员**
  - 优点：可通过 friend 或 getter 让测试访问
  - 缺点：需要在头文件暴露一个前向声明 `Ref<Scene>`；当前 SceneManager 是纯静态 API（无实例），加私有成员在语义上也是文件级静态

**采用方案 A**：与现有 SceneManager.cpp 内其他状态字段（订阅表、ActiveScene）的存放方式保持一致。

---

## 6. 出口标准

按 4 种 UI 状态逐一验证：

1. **初始 Edit 态 (F, F)**：编辑器启动后，MainMenuBar 之下、DockSpace 之上出现一条 34 像素高的工具条，工具条内水平居中排列 Play / Pause 两个图标按钮，两者均为常态（白色）
2. **点击 Pause → Edit + 预暂停 (F, T)**：Pause 按钮变蓝，Play 按钮保持常态；Scene 仍是编辑态（Hierarchy / Inspector / Scene 面板行为不变，`Scene::Copy` 未发生）
3. **预暂停下点击 Play → Pause (T, T)**：Play 按钮变蓝，Pause 按钮保持蓝色；ActiveScene 切换到深拷贝副本，副本 `SceneState = Pause`；Scene 面板从副本渲染；Hierarchy / Inspector 自动同步到副本；`OnUpdateRuntime` 每帧继续跑但内部跳过脚本/物理 tick
4. **Pause 态点击 Pause → Play (T, F)**：Pause 按钮恢复常态，Play 按钮保持蓝色；副本 Scene 从 Pause 切到 Play；ActiveScene 不变
5. **Play 态点击 Pause → Pause (T, T)**：Pause 按钮变蓝，Play 按钮保持蓝色；副本 Scene 从 Play 切到 Pause；ActiveScene 不变
6. **Play/Pause 态点击 Play → Edit (F, F)**：Play 按钮恢复常态，**Pause 按钮同步恢复常态**（`m_PauseArmed` 清零）；ActiveScene 还原为进入 Play 前保存的编辑态 Scene；所有面板通过订阅机制自动同步
7. **Edit 态下 Pause 反复切换**：点 Pause / 再点 Pause，Pause 按钮蓝 / 白切换；Scene 全程处于 Edit，无深拷贝、无广播
8. **DockSpace 布局**：DockSpace 及所有面板在 Toolbar 之下正确渲染，不与 Toolbar 重叠、不与 MainMenuBar 重叠

---

## 7. 实施顺序建议

按下面顺序落地，每一步都可独立验证：

1. **EditorIconManager 扩展**：加 2 个字段 + 2 个 Get 接口 + Init/Shutdown 联动；跑一次编辑器验证 warn 无遗漏
2. **SceneManager 运行态接口**：新增 3 个静态方法；不接 UI，先在 `EditorLayer::OnUpdate` 里临时用键盘（如按 F5）触发 `OnScenePlay/Stop` 验证 Scene 切换 + 广播工作
3. **EditorToolbar 类**：新建两个文件，实现渲染 + 事件转发；在 `EditorLayer::OnImGuiRender` 里挂上；DockSpace 暂时保持 `Pos/Size`，此时 Toolbar 会盖在 DockSpace 上方
4. **DockSpace 视口偏移修正**：改 `viewport->Pos / Size → WorkPos / WorkSize`，并在 `EditorLayer::OnImGuiRender` 内累加 `WorkOffsetMin.y`；验证 DockSpace 正确下移
5. **手工回归**：按第 6 节 8 条出口标准逐一走一遍

---

## 8. 与后续 Phase 的衔接

- **P0.6 GameViewportPanel**：Game 面板会订阅同一个 `SceneChangedCallback`，Play 时自动切到副本 Scene 上，用副本内的 Primary CameraComponent 渲染；Toolbar 无需再改
- **Phase 1 脚本 MVP**：`Scene::OnRuntimeStart / OnRuntimeStop / OnUpdateRuntime` 内部补上 `ScriptEngine` 调用；Toolbar 也无需再改
- **未来快捷键 Ctrl+P / Ctrl+Shift+P**：只需在 `EditorLayer::OnEvent` 中拦截键盘事件，转到 `SceneManager::OnScenePlay/Stop`；若快捷键需要联动预暂停，通过 `EditorLayer::m_EditorToolbar` 暴露一个 `TogglePauseArmed()` 方法即可
- **未来右侧工具条按钮**（Snap / Local-Global / Gizmo 模式）：直接在 `EditorToolbar::ImGuiRender` 内部按 `SameLine + SetCursorPosX(availWidth - 右侧组件宽度)` 追加
