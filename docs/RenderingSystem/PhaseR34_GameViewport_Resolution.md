# PhaseR34：Game 面板分辨率控制 ?? 正式设计文档

> **文档性质**：正式设计文档（Design）。可直接指导 AI 或人类工程师完成编码。
> **前置依赖**：R33 SceneRenderer 抽象已落地（Framebuffer / Pipeline / State 完全实例化）。
> **代码风格**：全文所有代码片段严格遵循 [Coding_Style_Guide.md](../Coding_Style_Guide.md)。
> **代码注释规范**：不写"阶段性注释"、不写"文档式注释"、不写"废话注释"。
> **改动范围**：局部功能扩展，仅涉及 `Luck3DApp/Source/Panels/GameViewportPanel.h/.cpp` 两个文件，**不动**任何底层渲染代码。

---

## 1. 概述

### 1.1 目标

为 Game 面板增加类 Unity Game View 的分辨率控制能力：

- **Free Aspect**：默认模式，RT 与相机 aspect 跟随面板尺寸变化（等价当前行为）
- **Aspect Ratio 固定**：`16:9 / 16:10 / 4:3 / 5:4 / 9:16` 五个预设。相机 aspect 固定为该比值；面板过宽/过高时用**黑边**（letterbox / pillarbox）填充，画面居中
- **Fixed Resolution**：`1920x1080 / 1280x720 / 1080x1920` 三个预设。RT 尺寸固定，面板显示时等比缩放贴到中央

顶部工具栏对齐 `SceneViewportPanel` 的既有风格（背景色 `{0.235, 0.235, 0.235, 1.0}`、高度 34、左对齐分辨率下拉框），面板外围空白区域填充色为 `#282828`。

### 1.2 非目标

- **不实现** Unity 的 "+" 号自定义分辨率（预设列表足够覆盖 90% 需求）
- **不实现** Unity 的顶部 Scale 滑块（Fixed Resolution 模式统一等比缩放）
- **不实现** "Low Resolution Aspect Ratios" 开关
- **不改造** `CameraComponent` / `Camera::SetViewportSize` / `Scene::OnViewportResize` 的现有职责
- **不引入** 序列化（面板选择不持久化到磁盘；重启回到 Free Aspect）

### 1.3 与 R33 架构的关系

R33 完成后，`SceneRenderer` 已经具备"RT 尺寸独立于面板尺寸"的所有前置条件：

| R33 能力 | 本 Phase 使用方式 |
|---|---|
| `SceneRenderer::OnViewportResize(w, h)` 接受任意尺寸 | 面板传入的是**计算后的 RT 尺寸**，而非 ImGui 面板尺寸 |
| Framebuffer 内聚 | 面板不感知 FBO；只从 `GetFinalColorAttachmentID()` 取纹理句柄 |
| `SceneRenderer::BeginScene/EndScene` 内部完成 Bind/Clear/Unbind | 面板层完全不动 GL 状态 |
| `Scene::OnRenderRuntime` 用 `CameraComponent.Camera.GetAspectRatio()` 决定投影 | 面板需要保证在调 `OnRenderRuntime` 之前，Primary Camera 的 aspect 已经被设成 RT aspect |

**关键洞察**：本 Phase 是**应用层**功能，底层无需任何改动。

---

## 2. 术语与概念模型

### 2.1 三个尺寸的区分

编码时**必须严格区分**下面三个尺寸，混用是常见 bug 来源：

| 术语 | 含义 | 变量命名 |
|---|---|---|
| **Panel Size**（面板尺寸） | ImGui 面板 Content Region 大小，随窗口拖动而变 | `m_ViewportSize` / `panelSize` |
| **Render Target Size**（RT 尺寸） | `SceneRenderer::m_Framebuffer` 的宽高，也是相机 aspect 的分母 | `m_RTSize` / `rtW`, `rtH` |
| **Display Size**（显示尺寸） | 面板上实际用来贴 RT 的矩形大小（居中显示，其余为黑边） | `displaySize` / `displayW`, `displayH` |

三者关系随模式而变：

```
Free Aspect：       RT Size = Panel Size = Display Size
Aspect Ratio：      RT Size = Display Size（按 aspect 收缩后的矩形），Panel Size 外围为黑边
Fixed Resolution：  RT Size = 固定值（如 1920x1080），Display Size 等比缩放到面板，Panel Size 外围为黑边
```

### 2.2 相机 aspect 的耦合

`Scene::OnRenderRuntime` 会读 `CameraComponent.Camera.GetAspectRatio()` 计算投影矩阵。因此本 Phase 必须保证：

> 每帧调用 `Scene::OnRenderRuntime` 之前，Primary Camera 的 aspect 已经被强制设为 `RT Size` 的 aspect。

这个动作在 Game 面板的 `OnUpdate` 内完成，覆盖 Scene / SceneViewportPanel 可能已经写入的 aspect（这是 R33 遗留的 "aspect 争抢" 问题的短期解法）。

---

## 3. 数据模型

### 3.1 模式枚举

```cpp
namespace Lucky
{
    /// <summary>
    /// Game 视口分辨率模式
    /// </summary>
    enum class GameViewResolutionMode : uint8_t
    {
        FreeAspect = 0,     // RT 尺寸 = 面板尺寸
        AspectRatio,        // 相机 aspect 固定；RT 按 aspect 收缩到面板短边
        FixedResolution     // RT 尺寸固定；面板显示时等比缩放
    };
}
```

### 3.2 预设条目

预设列表由一个 `constexpr` 数组承载。每个条目自描述其模式与参数：

```cpp
struct GameViewResolutionPreset
{
    const char*             Label;      // 下拉框显示文本
    GameViewResolutionMode  Mode;
    uint32_t                Width;      // Mode = FixedResolution 时的固定宽（AspectRatio 用作 aspect 分子）
    uint32_t                Height;     // Mode = FixedResolution 时的固定高（AspectRatio 用作 aspect 分母）
};

static constexpr GameViewResolutionPreset s_ResolutionPresets[] =
{
    { "Free Aspect",        GameViewResolutionMode::FreeAspect,       0,    0    },
    { "16:9",               GameViewResolutionMode::AspectRatio,      16,   9    },
    { "16:10",              GameViewResolutionMode::AspectRatio,      16,   10   },
    { "4:3",                GameViewResolutionMode::AspectRatio,      4,    3    },
    { "5:4",                GameViewResolutionMode::AspectRatio,      5,    4    },
    { "9:16 (Portrait)",    GameViewResolutionMode::AspectRatio,      9,    16   },
    { "1920x1080",          GameViewResolutionMode::FixedResolution,  1920, 1080 },
    { "1280x720",           GameViewResolutionMode::FixedResolution,  1280, 720  },
    { "1080x1920",          GameViewResolutionMode::FixedResolution,  1080, 1920 },
};
```

> **注意**：
> - `AspectRatio` 模式下 Width / Height 只用作"比例"两个整数，不代表实际像素；
> - `FreeAspect` 的 Width / Height 填 0，仅用于占位，运行时判断 Mode 时会跳过；
> - 列表**不搞可扩展性接口**（无 "+" 号自定义），YAGNI。

### 3.3 面板持有的状态

`GameViewportPanel` 新增两个字段：

```cpp
int         m_ResolutionIndex = 0;                    // 当前选中的预设索引（默认 0 = Free Aspect）
glm::uvec2  m_LastRTSize      = { 0, 0 };             // 上一帧的 RT 尺寸（用于判断是否需要 OnViewportResize）
```

**为什么不持有 `GameViewResolutionMode` 副本？**??预设索引唯一决定模式，避免两处状态失同步。

**为什么需要 `m_LastRTSize`？**??避免每帧都调 `OnViewportResize`（内部会重建 FBO 附件，昂贵）。只在 RT 尺寸变化时触发。

---

## 4. 关键流程

### 4.1 数据流总览

```
每帧 GameViewportPanel：

  OnGUI (先执行)：
    1. ImGui::Image 绘制之前先绘制 ToolBar（BeginChild / EndChild）
       └─ DropdownList 更新 m_ResolutionIndex
    2. avail = ImGui::GetContentRegionAvail()   （去掉 ToolBar 后的剩余区域）
    3. m_ViewportSize = avail
    4. rtSize    = ComputeRTSize(preset, m_ViewportSize)
    5. displaySz = ComputeDisplaySize(rtSize, m_ViewportSize)
    6. 计算居中偏移 → SetCursorScreenPos → ImGui::Image(displaySz)
       （周围空白区域由父窗口背景色 #282828 自然填充）

  OnUpdate (每帧调用一次，位于 EditorLayer 主循环)：
    1. rtSize = ComputeRTSize(preset, m_ViewportSize)
    2. 若 rtSize != m_LastRTSize：
         m_SceneRenderer->OnViewportResize(rtSize)
         m_Scene       ->OnViewportResize(rtSize)      // 让 Primary Camera aspect 跟上
         m_LastRTSize = rtSize
    3. 强制覆盖 Primary Camera aspect（防 Scene 面板污染）：
         Entity primary = m_Scene->GetPrimaryCameraEntity();
         if (primary) primary.GetComponent<CameraComponent>().Camera.SetViewportSize(rtSize.x, rtSize.y);
    4. m_SceneRenderer->SetClearColor(blackClear)
    5. m_Scene->OnRenderRuntime(*m_SceneRenderer)
```

**顺序说明**：ImGui 面板的 `OnGUI` 在 EditorLayer 里可能早于 `OnUpdate` 也可能晚于。当前 Luck3D 的 EditorLayer 是 `OnUpdate → OnGUI` 顺序。因此 `OnGUI` 里拿到的 `m_ViewportSize` 是**上一帧的面板大小**，`OnUpdate` 里用它计算 RT 尺寸??第一帧会有 1 帧的延迟（`m_ViewportSize = 0`），跳过渲染即可，与当前 Panel 的既有行为一致。

### 4.2 RT 尺寸计算

```cpp
static glm::uvec2 ComputeRTSize(const GameViewResolutionPreset& preset, const glm::vec2& panelSize)
{
    if (panelSize.x <= 0.0f || panelSize.y <= 0.0f)
    {
        return { 0, 0 };
    }

    switch (preset.Mode)
    {
        case GameViewResolutionMode::FreeAspect:
        {
            return { static_cast<uint32_t>(panelSize.x), static_cast<uint32_t>(panelSize.y) };
        }
        case GameViewResolutionMode::AspectRatio:
        {
            float targetAspect = static_cast<float>(preset.Width) / static_cast<float>(preset.Height);
            float panelAspect  = panelSize.x / panelSize.y;

            if (panelAspect > targetAspect)
            {
                // 面板过宽 → RT 高度取满，宽度按比例
                uint32_t h = static_cast<uint32_t>(panelSize.y);
                uint32_t w = static_cast<uint32_t>(panelSize.y * targetAspect);
                return { w, h };
            }
            else
            {
                // 面板过高 → RT 宽度取满，高度按比例
                uint32_t w = static_cast<uint32_t>(panelSize.x);
                uint32_t h = static_cast<uint32_t>(panelSize.x / targetAspect);
                return { w, h };
            }
        }
        case GameViewResolutionMode::FixedResolution:
        {
            return { preset.Width, preset.Height };
        }
    }
    return { 0, 0 };
}
```

### 4.3 显示尺寸计算（Fixed Resolution 专用）

对 `FreeAspect` / `AspectRatio` 而言 Display Size = RT Size；对 `FixedResolution` 而言需要把 RT 尺寸等比缩放到面板范围内。统一函数：

```cpp
static glm::vec2 ComputeDisplaySize(const glm::uvec2& rtSize, const glm::vec2& panelSize)
{
    if (rtSize.x == 0 || rtSize.y == 0 || panelSize.x <= 0.0f || panelSize.y <= 0.0f)
    {
        return { 0.0f, 0.0f };
    }

    float rtAspect    = static_cast<float>(rtSize.x) / static_cast<float>(rtSize.y);
    float panelAspect = panelSize.x / panelSize.y;

    if (panelAspect > rtAspect)
    {
        // 面板过宽 → 显示高度取满，宽度按 RT aspect
        return { panelSize.y * rtAspect, panelSize.y };
    }
    else
    {
        // 面板过高 → 显示宽度取满，高度按 RT aspect
        return { panelSize.x, panelSize.x / rtAspect };
    }
}
```

对 `FreeAspect` 模式，rtSize == panelSize，函数天然返回 panelSize；对 `AspectRatio` 模式，rtSize 已经是按 aspect 收缩后的矩形，Display Size 等于 rtSize（面板剩余区域天然黑边）。所以**三种模式统一走这一个函数**，无需分支。

---

## 5. 关键设计决策

### 5.1 ToolBar 结构风格

**要求**：与 `SceneViewportPanel` 顶部工具栏保持一致??34px 高、背景色 `{0.235f, 0.235f, 0.235f, 1.0f}`、左对齐、内部用 `UI::DropdownList`。

#### 方案 A（推荐）：完全复刻 SceneViewportPanel 的 BeginChild ToolBar

```cpp
void GameViewportPanel::OnGUI()
{
    float toolBarHeight = 34.0f;

    {
        UI::ScopedColor bgColor(ImGuiCol_ChildBg, { 0.235f, 0.235f, 0.235f, 1.0f });
        ImGui::BeginChild("ToolBar", { 0, toolBarHeight }, false,
                          ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);
        {
            UI::ShiftCursor(4.0f, 4.0f);
            ImGui::SetNextItemWidth(140.0f);    // 分辨率名较长，给宽一点

            const int presetCount = IM_ARRAYSIZE(s_ResolutionPresets);
            const char* labels[presetCount];
            for (int i = 0; i < presetCount; ++i)
            {
                labels[i] = s_ResolutionPresets[i].Label;
            }
            UI::DropdownList(m_ResolutionIndex, labels, presetCount);
        }
        ImGui::EndChild();
    }

    // ... 后续绘制 RT 图像
}
```

- **优点**：与 Scene 面板视觉完全一致，读者一眼理解；未来加更多控件（比如 Stats 显示、Mute 按钮）位置模式已就位
- **缺点**：需要每帧构造一次 `labels[]` 数组（可用 `static` 缓存优化，但没必要）

#### 方案 B：不加 ToolBar，把下拉框放在 Panel 头部的普通位置

```cpp
UI::DropdownList(m_ResolutionIndex, labels, presetCount);
ImGui::Image(...);
```

- **优点**：代码短
- **缺点**：与 Scene 面板视觉不统一；用户不清楚控件属于"工具"还是"内容"；未来扩展困难

**推荐：方案 A**。用户要求"顶部 ToolBar 和 Scene 窗口一样"，直接复刻风格是唯一正解。

### 5.2 RT 尺寸变化的检测时机

**问题**：什么时候调 `m_SceneRenderer->OnViewportResize`？

#### 方案 A（推荐）：`OnUpdate` 内比较 `m_LastRTSize`，仅在变化时调用

```cpp
void GameViewportPanel::OnUpdate(DeltaTime dt)
{
    glm::uvec2 rtSize = ComputeRTSize(s_ResolutionPresets[m_ResolutionIndex], m_ViewportSize);

    if (rtSize.x > 0 && rtSize.y > 0 && rtSize != m_LastRTSize)
    {
        m_SceneRenderer->OnViewportResize(rtSize.x, rtSize.y);
        if (m_Scene)
        {
            m_Scene->OnViewportResize(rtSize.x, rtSize.y);
        }
        m_LastRTSize = rtSize;
    }

    // ... 后续渲染
}
```

- **优点**：避免每帧重建 FBO；语义清晰；与现有 Game/Scene 面板 resize 逻辑同构
- **缺点**：需要维护 `m_LastRTSize` 状态（1 个字段，成本极低）

#### 方案 B：每帧无脑调 `OnViewportResize`

- **优点**：无状态、少一个字段
- **缺点**：`Framebuffer::Resize` 内部会重建 GL 附件；Fixed Resolution 模式下 RT 尺寸不变，每帧无谓重建，浪费显存分配

**推荐：方案 A**。方案 B 的性能倒退不可接受。

### 5.3 Primary Camera aspect 的覆盖时机

**问题**：Game 面板需要让 Primary Camera 的 aspect 等于 RT aspect，但 Scene 面板也在写它??如何保证 Game 渲染时读到正确值？

#### 方案 A（推荐）：Game 面板 `OnUpdate` 每帧在 `Scene::OnRenderRuntime` 前强制覆盖

```cpp
Entity primary = m_Scene->GetPrimaryCameraEntity();
if (primary)
{
    auto& cameraComp = primary.GetComponent<CameraComponent>();
    cameraComp.Camera.SetViewportSize(rtSize.x, rtSize.y);
}

m_Scene->OnRenderRuntime(*m_SceneRenderer);
```

- **优点**：改动最小；不侵入 Scene / CameraComponent；一行代码解决问题
- **缺点**：本质是 workaround；如果未来加入第三个面板（如 Preview）同样争抢，需要每个面板重复这段代码

#### 方案 B：把 aspect 从 CameraComponent 状态里剥离，作为渲染时参数传入

将 `Scene::OnRenderRuntime` 改为直接接受一个 aspect 参数，或让 `CameraRenderData` 携带 aspect 由渲染器覆盖投影矩阵。

- **优点**：从根本上解决"aspect 争抢"问题；语义正确
- **缺点**：涉及 `CameraComponent` / `Camera` / `Scene::OnRenderRuntime` / `RenderSceneImpl` 联动修改；超出本 Phase 的应用层范围

#### 方案 C：Scene 面板不再调 `Scene::OnViewportResize` 更新 Primary Camera aspect

- **优点**：从源头切断污染
- **缺点**：Game 面板未运行时 Primary Camera 的 aspect 值就无人维护，暴露序列化/初始化路径问题

**推荐：方案 A**。方案 B 是终极方案，但作为独立 Phase（R33 遗留问题清单里已列出）处理更合理；本 Phase 只做局部功能，坚持不侵入。

### 5.4 面板外围空白色 `#282828` 的实现

用户要求：面板中 RT 图像之外的区域填充 `#282828`（十进制 `0.157, 0.157, 0.157, 1.0`）。

#### 方案 A（推荐）：整个 Game 面板的 `WindowBg` 通过 `UI::ScopedColor` 覆盖

```cpp
void GameViewportPanel::OnGUI()
{
    UI::ScopedColor windowBg(ImGuiCol_WindowBg, ImVec4{ 0.157f, 0.157f, 0.157f, 1.0f });
    // ... 后续绘制 ToolBar + Image
}
```

- **优点**：一行搞定；面板任何空白（含 ToolBar 之外的 padding 缝隙）都自动填充
- **缺点**：需要注意 `ScopedColor` 的作用域正确覆盖到 `Image` 之外的空白绘制

#### 方案 B：只对 Image 之外的区域用 `ImGui::GetWindowDrawList()->AddRectFilled` 手动填充

- **优点**：只影响 RT 周围的黑边，不影响面板其他潜在装饰
- **缺点**：多写 20 行；需要计算 4 块空白矩形；padding 缝隙依然是默认色

**推荐：方案 A**。Game 面板本就不放其他东西，整个面板背景就是 Game 视图的"外壳"，统一 `#282828` 语义清晰。

### 5.5 Panel Flags

`GameViewportPanel::GameViewportPanel` 里已经设置 `ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse`。加上 ToolBar 后，父窗口依然禁用滚动条，但 `Image` 的子窗口不需要独立 `BeginChild` 包裹（对比 SceneViewportPanel 是为了拖拽目标 / Gizmo 绘制才用 BeginChild "Viewport"，Game 面板没有这些需求）。

**决策**：不加独立 `BeginChild "Viewport"`，直接在父窗口里 `SetCursorScreenPos + Image`。

---

## 6. 完整代码结构（可编码级别）

### 6.1 [GameViewportPanel.h](file:D:/Projects/C++/Luck3D/Luck3DApp/Source/Panels/GameViewportPanel.h)

```cpp
#pragma once

#include "Lucky/Editor/EditorPanel.h"
#include "Lucky/Renderer/SceneRenderer.h"
#include "Lucky/Scene/Scene.h"
#include "Lucky/Scene/SceneManager.h"

#include <glm/glm.hpp>

namespace Lucky
{
    /// <summary>
    /// Game 面板：使用场景内 Primary CameraComponent 渲染游戏视角
    /// 顶部工具栏提供分辨率模式选择（Free Aspect / Aspect Ratio / Fixed Resolution）
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
        Ref<SceneRenderer> m_SceneRenderer;                 // 场景渲染器（持有 FBO / 状态）

        glm::vec2 m_ViewportSize = { 0.0f, 0.0f };          // ToolBar 下方可用区域大小
        glm::uvec2 m_LastRTSize = { 0, 0 };                 // 上一帧的 RT 尺寸（用于变更检测）

        int m_ResolutionIndex = 0;                          // 当前选中的分辨率预设索引

        SceneManager::SubscriptionHandle m_SceneChangedSub = 0;
    };
}
```

**要点**：
- 唯一新增字段：`m_LastRTSize`、`m_ResolutionIndex`
- `m_ViewportSize` 语义**变化**：从"整个 Panel 尺寸"变为"ToolBar 下方剩余区域尺寸"
- 头文件不需要暴露 `GameViewResolutionMode` / `GameViewResolutionPreset` ?? 它们是实现细节，放 `.cpp` 里的匿名 namespace

### 6.2 [GameViewportPanel.cpp](file:D:/Projects/C++/Luck3D/Luck3DApp/Source/Panels/GameViewportPanel.cpp)

**顶部匿名 namespace**：

```cpp
#include "GameViewportPanel.h"

#include "Lucky/Renderer/Renderer3D.h"
#include "Lucky/Renderer/Framebuffer.h"
#include "Lucky/Scene/Entity.h"
#include "Lucky/Scene/Components/Components.h"

#include "Lucky/UI/ScopedGuards.h"
#include "Lucky/UI/Widgets.h"
#include "Lucky/UI/UICore.h"

#include "imgui/imgui.h"

namespace Lucky
{
    namespace
    {
        enum class GameViewResolutionMode : uint8_t
        {
            FreeAspect = 0,
            AspectRatio,
            FixedResolution
        };

        struct GameViewResolutionPreset
        {
            const char*             Label;
            GameViewResolutionMode  Mode;
            uint32_t                Width;
            uint32_t                Height;
        };

        constexpr GameViewResolutionPreset s_ResolutionPresets[] =
        {
            { "Free Aspect",        GameViewResolutionMode::FreeAspect,       0,    0    },
            { "16:9",               GameViewResolutionMode::AspectRatio,      16,   9    },
            { "16:10",              GameViewResolutionMode::AspectRatio,      16,   10   },
            { "4:3",                GameViewResolutionMode::AspectRatio,      4,    3    },
            { "5:4",                GameViewResolutionMode::AspectRatio,      5,    4    },
            { "9:16 (Portrait)",    GameViewResolutionMode::AspectRatio,      9,    16   },
            { "1920x1080",          GameViewResolutionMode::FixedResolution,  1920, 1080 },
            { "1280x720",           GameViewResolutionMode::FixedResolution,  1280, 720  },
            { "1080x1920",          GameViewResolutionMode::FixedResolution,  1080, 1920 },
        };

        glm::uvec2 ComputeRTSize(const GameViewResolutionPreset& preset, const glm::vec2& panelSize)
        {
            if (panelSize.x <= 0.0f || panelSize.y <= 0.0f)
            {
                return { 0, 0 };
            }

            switch (preset.Mode)
            {
                case GameViewResolutionMode::FreeAspect:
                    return { static_cast<uint32_t>(panelSize.x), static_cast<uint32_t>(panelSize.y) };

                case GameViewResolutionMode::AspectRatio:
                {
                    float targetAspect = static_cast<float>(preset.Width) / static_cast<float>(preset.Height);
                    float panelAspect  = panelSize.x / panelSize.y;

                    if (panelAspect > targetAspect)
                    {
                        uint32_t h = static_cast<uint32_t>(panelSize.y);
                        uint32_t w = static_cast<uint32_t>(panelSize.y * targetAspect);
                        return { w, h };
                    }
                    uint32_t w = static_cast<uint32_t>(panelSize.x);
                    uint32_t h = static_cast<uint32_t>(panelSize.x / targetAspect);
                    return { w, h };
                }

                case GameViewResolutionMode::FixedResolution:
                    return { preset.Width, preset.Height };
            }
            return { 0, 0 };
        }

        glm::vec2 ComputeDisplaySize(const glm::uvec2& rtSize, const glm::vec2& panelSize)
        {
            if (rtSize.x == 0 || rtSize.y == 0 || panelSize.x <= 0.0f || panelSize.y <= 0.0f)
            {
                return { 0.0f, 0.0f };
            }

            float rtAspect    = static_cast<float>(rtSize.x) / static_cast<float>(rtSize.y);
            float panelAspect = panelSize.x / panelSize.y;

            if (panelAspect > rtAspect)
            {
                return { panelSize.y * rtAspect, panelSize.y };
            }
            return { panelSize.x, panelSize.x / rtAspect };
        }
    }
}
```

**ctor / dtor**（保持现状不变）：

```cpp
GameViewportPanel::GameViewportPanel(const Ref<Scene>& scene)
    : m_Scene(scene)
{
    SetFlags(ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);

    SceneRendererSpec spec;
    spec.Width = 1280;
    spec.Height = 720;
    spec.EnableShadow = true;
    spec.EnablePicking = false;
    spec.EnableOutline = false;
    spec.EnableDebugVisualize = false;
    spec.EnablePostProcess = true;

    m_SceneRenderer = CreateRef<SceneRenderer>();
    m_SceneRenderer->Init(spec);

    m_SceneChangedSub = SceneManager::Subscribe([this](const Ref<Scene>& newScene)
    {
        m_Scene = newScene;
    });
}

GameViewportPanel::~GameViewportPanel()
{
    SceneManager::Unsubscribe(m_SceneChangedSub);
    if (m_SceneRenderer)
    {
        m_SceneRenderer->Shutdown();
    }
}
```

**`OnUpdate` 新版**：

```cpp
void GameViewportPanel::OnUpdate(DeltaTime dt)
{
    const GameViewResolutionPreset& preset = s_ResolutionPresets[m_ResolutionIndex];
    glm::uvec2 rtSize = ComputeRTSize(preset, m_ViewportSize);

    // ---- RT 尺寸变化时触发 resize ----
    if (rtSize.x > 0 && rtSize.y > 0 && rtSize != m_LastRTSize)
    {
        m_SceneRenderer->OnViewportResize(rtSize.x, rtSize.y);
        if (m_Scene)
        {
            m_Scene->OnViewportResize(rtSize.x, rtSize.y);
        }
        m_LastRTSize = rtSize;
    }

    if (!m_Scene)
    {
        return;
    }

    // ---- 覆盖 Primary Camera aspect（避免被 Scene 面板 resize 污染） ----
    if (rtSize.x > 0 && rtSize.y > 0)
    {
        Entity primary = m_Scene->GetPrimaryCameraEntity();
        if (primary)
        {
            primary.GetComponent<CameraComponent>().Camera.SetViewportSize(rtSize.x, rtSize.y);
        }
    }

    constexpr glm::vec4 blackClear{ 0.0f, 0.0f, 0.0f, 1.0f };
    m_SceneRenderer->SetClearColor(blackClear);

    m_Scene->OnRenderRuntime(*m_SceneRenderer);
}
```

**`OnGUI` 新版**：

```cpp
void GameViewportPanel::OnGUI()
{
    // 面板外围空白填充色 #282828
    UI::ScopedColor windowBg(ImGuiCol_WindowBg, ImVec4{ 0.157f, 0.157f, 0.157f, 1.0f });

    // ---- 顶部 ToolBar（对齐 SceneViewportPanel 风格） ----
    float toolBarHeight = 34.0f;
    {
        UI::ScopedColor bgColor(ImGuiCol_ChildBg, { 0.235f, 0.235f, 0.235f, 1.0f });
        ImGui::BeginChild("ToolBar", { 0, toolBarHeight }, false,
                          ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);
        {
            UI::ShiftCursor(4.0f, 4.0f);
            ImGui::SetNextItemWidth(140.0f);

            constexpr int presetCount = IM_ARRAYSIZE(s_ResolutionPresets);
            const char* labels[presetCount];
            for (int i = 0; i < presetCount; ++i)
            {
                labels[i] = s_ResolutionPresets[i].Label;
            }
            UI::DropdownList(m_ResolutionIndex, labels, presetCount);
        }
        ImGui::EndChild();
    }

    // ---- RT 图像区域 ----
    ImVec2 avail = ImGui::GetContentRegionAvail();
    m_ViewportSize = { avail.x, avail.y };

    const GameViewResolutionPreset& preset = s_ResolutionPresets[m_ResolutionIndex];
    glm::uvec2 rtSize    = ComputeRTSize(preset, m_ViewportSize);
    glm::vec2  displaySz = ComputeDisplaySize(rtSize, m_ViewportSize);

    if (displaySz.x <= 0.0f || displaySz.y <= 0.0f)
    {
        return;
    }

    // 居中显示
    ImVec2 cursorStart = ImGui::GetCursorScreenPos();
    ImVec2 centerOffset = {
        (avail.x - displaySz.x) * 0.5f,
        (avail.y - displaySz.y) * 0.5f
    };
    ImGui::SetCursorScreenPos({ cursorStart.x + centerOffset.x, cursorStart.y + centerOffset.y });

    uint32_t textureID = m_SceneRenderer->GetFinalColorAttachmentID();
    ImGui::Image(reinterpret_cast<void*>(static_cast<uintptr_t>(textureID)),
                 ImVec2{ displaySz.x, displaySz.y },
                 ImVec2{ 0, 1 }, ImVec2{ 1, 0 });
}
```

---

## 7. 潜在坑与解决方案

### 7.1 `m_ViewportSize` 语义变化

**变化点**：从"整个 Panel 尺寸"变为"ToolBar 下方剩余尺寸"。

**影响**：由于 `OnUpdate` 里用的 `m_ViewportSize` 是上一帧 `OnGUI` 写入的，从"整个面板"变为"剩余区域"会让 RT 尺寸比之前少 34px 高。这是**正确的行为**（RT 本就不该占用 ToolBar 区域），无需回滚。

**唯一副作用**：面板首次显示的第一帧，`m_ViewportSize == (0, 0)`，`ComputeRTSize` 返回 (0, 0)，触发 resize 分支的 `rtSize.x > 0` 判断为 false，跳过 resize。第二帧 `m_ViewportSize` 已经正确 → 正常触发。**这与当前 Panel 的第一帧行为一致，不算回归**。

### 7.2 面板极窄时 RT 尺寸退化到 0

**场景**：用户把 Game 面板拖到高度 < 34px，`avail.y` 变成负数或 0。

**处理**：`ComputeRTSize` 已经用 `panelSize.x <= 0.0f || panelSize.y <= 0.0f` 判断返回 `(0, 0)`；`OnUpdate` 里 `rtSize.x > 0 && rtSize.y > 0` 分支跳过 resize；`OnGUI` 里 `displaySz` 也是 0，`return` 提前退出。**逻辑完整**。

### 7.3 Fixed Resolution 尺寸过大导致显存溢出

**场景**：用户选 `1080x1920`，但显示器实际是 4K，用户拖大面板到 3840x2160。RT 是 1080x1920（约 8 MB），Display 面板贴图缩放显示。**RT 尺寸由预设固定，与面板大小无关**，因此显存占用可控。

**注意**：切换 Fixed Resolution 会立即触发一次 `OnViewportResize(1920, 1080)`（如果面板初始是 1280x720，RT 会先从 1280x720 变到 1920x1080），FBO 附件重建。这是**预期行为**。

### 7.4 Game 面板未获焦时是否要停止渲染

**当前状态**：`EditorLayer::OnUpdate` 每帧都会调用所有 Panel 的 `OnUpdate`。Game 面板即便未显示，也会渲染。

**本 Phase 决策**：**不做**"未显示时暂停渲染"的优化，与现有行为一致。这属于独立的性能优化 Phase。

### 7.5 SceneRenderer::OnViewportResize 传入 0 值

`Framebuffer::Resize` 对 (0, 0) 有内部保护（当前 `SceneRenderer::OnViewportResize` 已经在 R33 里做了 `if (width == 0 || height == 0) return;`）。**无需额外处理**。

---

## 8. 验收标准

### 8.1 功能验收

| 场景 | 期望表现 |
|---|---|
| 首次打开 Game 面板 | 下拉框默认显示 "Free Aspect"，RT 填满面板 |
| 切换 "16:9" | 面板过宽时左右出现 `#282828` 黑边；面板过高时上下出现黑边；画面居中 |
| 切换 "9:16 (Portrait)" | 面板过宽时左右巨大黑边；渲染画面为竖长条 |
| 切换 "1920x1080" | RT 固定 1920x1080；面板缩小时画面自动缩放；面板放大到 4K 时画面依然是 FullHD 缩放显示（不模糊） |
| 拖动面板尺寸 | Free Aspect 下 RT 平滑跟随；Aspect Ratio 下黑边动态调整；Fixed Resolution 下 Display 平滑缩放而 RT 不变 |
| ToolBar 视觉 | 高度 34、背景色 `{0.235, 0.235, 0.235}`、左对齐、宽度 140px 的下拉框??与 Scene 面板对齐 |
| 面板外围空白 | 全部填充 `#282828`（十进制 `{0.157, 0.157, 0.157, 1.0}`） |
| 无 Primary Camera | ToolBar 正常显示；下方全部 `#282828`（因为渲染直接 return，Framebuffer 保留清屏色黑，但被面板背景覆盖不到 RT 图像位置） |

### 8.2 回归验收（不应破坏）

- Scene 面板拖动尺寸时相机 aspect 依然与 Scene 面板一致
- Game / Scene 面板同时打开时，两者互不影响画面比例
- 切换 Play / Stop 状态后 RT 尺寸行为一致
- Statistics 面板读取 Primary SceneRenderer 的统计数据不受影响

### 8.3 性能验收

- Fixed Resolution 模式下，拖动面板尺寸时**不应**触发 `Framebuffer::Resize`（用 GL Debug 层或加临时日志验证 `OnViewportResize` 未被反复调用）
- Aspect Ratio 模式下，仅当 RT 尺寸变化跨越像素边界时才触发 `Framebuffer::Resize`

---

## 9. 实施步骤（Todo 顺序）

1. 在 [GameViewportPanel.h](file:D:/Projects/C++/Luck3D/Luck3DApp/Source/Panels/GameViewportPanel.h) 增加两个字段：`m_ResolutionIndex`、`m_LastRTSize`
2. 在 [GameViewportPanel.cpp](file:D:/Projects/C++/Luck3D/Luck3DApp/Source/Panels/GameViewportPanel.cpp) 顶部匿名 namespace 加入枚举、预设表、两个辅助函数
3. 重写 `OnUpdate` 加入 RT 尺寸变化检测 + Primary Camera aspect 覆盖
4. 重写 `OnGUI`：先绘 ToolBar，再计算 Display 居中偏移绘 Image；用 `UI::ScopedColor` 设置面板背景
5. 补充 include：`Entity.h`、`Components.h`、`Widgets.h`、`UICore.h`、`ScopedGuards.h`
6. 编译，按 8.1 / 8.2 逐项验证

---

## 10. 未来扩展点（**本 Phase 不做**）

- **自定义分辨率**：加 `+` 按钮，弹窗输入宽/高，追加到预设列表末尾
- **Scale 滑块**：Fixed Resolution 模式下，允许用户 1x / 2x / 0.5x 查看
- **序列化**：把 `m_ResolutionIndex` 存到 `EditorPreferences`
- **CameraComponent aspect 剥离**：让 aspect 不再是 CameraComponent 的持久字段，由渲染时参数传入（对应 §5.3 方案 B）
- **每面板独立的清屏色**：目前 Game 面板黑清屏是硬编码，可考虑接入 `EnvironmentSettings::BackgroundColor`

---

## 11. 变更清单

| 文件 | 改动 |
|---|---|
| [GameViewportPanel.h](file:D:/Projects/C++/Luck3D/Luck3DApp/Source/Panels/GameViewportPanel.h) | 新增 `m_ResolutionIndex` / `m_LastRTSize` 字段；`m_ViewportSize` 语义注释更新 |
| [GameViewportPanel.cpp](file:D:/Projects/C++/Luck3D/Luck3DApp/Source/Panels/GameViewportPanel.cpp) | 顶部匿名 namespace 引入枚举/预设表/辅助函数；重写 `OnUpdate` / `OnGUI` |

**估计行数**：+180 行 / -20 行，净增 ~160 行；不涉及任何底层文件。

