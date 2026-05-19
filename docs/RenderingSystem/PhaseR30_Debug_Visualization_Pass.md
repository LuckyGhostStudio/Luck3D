# PhaseR30 调试可视化 Pass 化（CSM 级联可视化解耦）

> 版本：v1.1
> 创建日期：2026-05-18
> 最后更新：2026-05-19（决策落地：A→A2、D→D3）
> 依赖前置：R7（Multi-Pass 架构）、R17（RenderPipelinePanel）、R18（CSM）

## 决策落地说明（v1.1）

本次实施确认采用以下方案，文档中标注的

## 目录

- [一、背景与目标](#一背景与目标)
- [二、当前现状分析](#二当前现状分析)
- [三、设计原则](#三设计原则)
- [四、整体架构](#四整体架构)
- [五、详细设计](#五详细设计)
  - [5.1 新增 DebugVisualizePass](#51-新增-debugvisualizepass)
  - [5.2 新增 DebugCSMVisualize.frag](#52-新增-debugcsmvisualizefrag)
  - [5.3 RenderContext 扩展](#53-rendercontext-扩展)
  - [5.4 ShadowPass 改造](#54-shadowpass-改造)
  - [5.5 OpaquePass / TransparentPass 清理](#55-opaquepass--transparentpass-清理)
  - [5.6 Standard.frag 清理](#56-standardfrag-清理)
  - [5.7 Lucky/Shadow.glsl 清理](#57-luckyshadowglsl-清理)
  - [5.8 Material.cpp 白名单清理](#58-materialcpp-白名单清理)
  - [5.9 Renderer3D 注册与上下文构建](#59-renderer3d-注册与上下文构建)
- [六、关键决策点（多方案对比）](#六关键决策点多方案对比)
  - [6.1 决策点 A：世界坐标重建方式](#61-决策点-a世界坐标重建方式)
  - [6.2 决策点 B：调试 Pass 的渲染目标](#62-决策点-b调试-pass-的渲染目标)
  - [6.3 决策点 C：调试开关存储位置](#63-决策点-c调试开关存储位置)
  - [6.4 决策点 D：调试开关 UI 入口](#64-决策点-d调试开关-ui-入口)
  - [6.5 决策点 E：Pass 分组归属](#65-决策点-e-pass-分组归属)
- [七、Pass DebugUI 方法移除分析](#七pass-debugui-方法移除分析)
- [八、文件清单与改动汇总](#八文件清单与改动汇总)
- [九、实施步骤](#九实施步骤)
- [十、风险与回滚](#十风险与回滚)

---

## 一、背景与目标

### 1.1 问题陈述

当前 `Standard.frag` 末尾保留有 CSM 级联调试可视化代码：

```glsl
if (u_DebugCSMVisualize != 0 && u_ShadowEnabled != 0)
{
    int cascadeIndex = SelectCascadeIndex(v_Input.WorldPos);
    vec3 debugColor = GetCascadeDebugColor(cascadeIndex);
    color = mix(color, debugColor, 0.3);
}
```

这导致两个抽象层级问题：

1. **用户层 Shader 承担引擎调试职责**：用户编写自定义 Shader（如 `Toon.frag`、`Hair.frag`）必须复制这段代码，否则该 Shader 在调试模式下"失踪"
2. **横切关注点泄漏到材质层**：调试可视化在性质上等同 Bloom / FXAA / Tonemapping —— 都是管线级后处理，不应混入材质着色

### 1.2 目标

| 目标 | 说明 |
|------|------|
| **用户 Shader 零感知** | 用户 `.frag` 不写任何调试相关代码，未来新增调试模式不影响用户文件 |
| **架构对齐主流引擎** | 调试可视化作为独立 Pass 注入管线（对齐 Unity URP DebugFullScreenPass、UE View Mode、Filament DebugPass） |
| **可扩展** | 未来新增"法线可视化""UV 可视化""Overdraw"等调试模式，仅需新增 Pass，不改任何现有文件 |
| **零开销** | 不开调试时，新增 Pass 完全跳过，对主渲染管线无影响 |

### 1.3 非目标

- 本期**不**实现法线 / UV / Overdraw 等其他调试可视化（仅迁移 CSM）
- 本期**不**改造 R7 引入的 `RenderPass::OnDebugGUI` 接口本身（仅改变其使用方式，详见 [第七章](#七pass-debugui-方法移除分析)）

---

## 二、当前现状分析

### 2.1 CSM 调试可视化数据流（迁移前）

```
┌────────────────┐                                        ┌──────────────────────┐
│ ShadowPass     │  m_DebugCSMVisualize (bool)            │ RenderPipelinePanel  │
│  - OnDebugGUI()│ ?───── PropertyCheckbox ─────────────  │  for each Pass:      │
└────────┬───────┘                                        │    pass.OnDebugGUI() │
         │ IsDebugCSMVisualize()                          └──────────────────────┘
         ▼
┌─────────────────────────────────┐
│ Renderer3D::EndScene            │
│   context.DebugCSMVisualize =   │
│     shadowPass->IsDebugCSM()    │
└────────┬────────────────────────┘
         ▼
┌─────────────────────────────────┐       ┌─────────────────────────────────┐
│ OpaquePass::Execute             │       │ TransparentPass::Execute        │
│   shader->SetInt(               │       │   （类似设置）                    │
│     "u_DebugCSMVisualize", ...) │       │                                 │
└────────┬────────────────────────┘       └─────────────────────────────────┘
         ▼
┌─────────────────────────────────┐
│ Standard.frag                   │
│   if (u_DebugCSMVisualize != 0) │
│     mix(color, cascadeColor)    │ ?──── ★ 用户层泄漏点
└─────────────────────────────────┘
```

### 2.2 涉及文件清单（迁移前）

| 文件 | 涉及内容 |
|------|---------|
| `Luck3DApp/Assets/Shaders/Standard.frag` | 末尾 5 行 CSM 调试逻辑 |
| `Luck3DApp/Assets/Shaders/Lucky/Shadow.glsl` | `uniform u_DebugCSMVisualize`、`GetCascadeDebugColor()` |
| `Lucky/Source/Lucky/Renderer/Material.cpp` | `s_InternalUniforms` 白名单含 `u_DebugCSMVisualize` |
| `Lucky/Source/Lucky/Renderer/RenderContext.h` | `bool DebugCSMVisualize` 字段 |
| `Lucky/Source/Lucky/Renderer/Passes/ShadowPass.h/cpp` | `m_DebugCSMVisualize` + `IsDebugCSMVisualize()` + `OnDebugGUI()` |
| `Lucky/Source/Lucky/Renderer/Passes/OpaquePass.cpp` | `SetInt("u_DebugCSMVisualize", ...)` |
| `Lucky/Source/Lucky/Renderer/Passes/TransparentPass.cpp` | （同 OpaquePass） |
| `Lucky/Source/Lucky/Renderer/Renderer3D.cpp` | `context.DebugCSMVisualize = shadowPass->IsDebugCSMVisualize()` |

### 2.3 关键基础设施盘点（已具备）

| 基础设施 | 位置 | 用途 |
|---------|------|------|
| `RenderPipeline::ExecuteGroup(group, ctx)` | `RenderPipeline.cpp` | 按分组执行 Pass，自然支持新增分组 |
| `RenderCommand::ResetDefaultRenderState()` | `RenderCommand.cpp` | 每个 Pass 执行后自动恢复默认状态，新 Pass 无需考虑状态干扰 |
| `ScreenQuad::Draw()` | `ScreenQuad.h` | 全屏 Quad 绘制（FXAA / Tonemapping / Vignette 已使用） |
| `RenderContext::HDR_FBO` / `TargetFramebuffer` | `RenderContext.h` | HDR FBO（场景颜色 + 深度）、主 LDR FBO 已挂载 |
| `Framebuffer::GetDepthAttachmentRendererID()` | `Framebuffer.h` | 已有接口可获取深度纹理 ID |
| `Lucky/Shadow.glsl::SelectCascadeIndex()` | `Shadow.glsl` | 选级联函数已存在，可被新 Pass 复用 |
| HDR FBO 深度附件类型 | `PostProcessPass::Init()` | `DEFPTH24STENCIL8`，是纹理附件，可作为 Sampler 输入 ? |
| Camera UBO 字段 | `Common.glsl` | 当前仅 `ViewProjectionMatrix` + `Position`，**无逆矩阵** ??（重建世界坐标需考虑） |

---

## 三、设计原则

1. **用户 Shader 不感知**：用户 `.frag` 中不出现任何调试相关 uniform / 函数调用 / 条件分支
2. **复用现有基础设施**：调试 Pass 完全套用 `RenderPass` + `Pass 分组` + `ResetDefaultRenderState` 既有机制，不引入新概念
3. **代码组织对齐 PostProcess**：调试 Pass 的代码结构、Shader 组织、命名风格与 `PostProcessPass` / `VignetteEffect` 一致
4. **零开销路径**：调试关闭时，Pass 在 `Execute()` 入口立即 `return`，单次分支判断
5. **可扩展占位**：本次仅实现 CSM 调试，但接口和文件位置预留新增 Mode 的扩展点

---

## 四、整体架构

### 4.1 迁移后数据流

```
┌────────────────────────┐            ┌──────────────────────┐
│ DebugVisualizePass     │  m_Mode    │ RenderPipelinePanel  │
│  - OnDebugGUI()★       │ ?─ ImGui ─ │  for each Pass:      │
│  - Execute(context)    │            │    pass.OnDebugGUI() │
└──────────┬─────────────┘            └──────────────────────┘
           │ 读取 context.HDR_FBO 颜色与深度纹理
           │ 读取 context.CascadeFarPlanes / CameraViewMatrix
           ▼
┌──────────────────────────────────────────────────────────────┐
│ DebugCSMVisualize.frag (Internal)                            │
│   1. 采样 SceneColor (HDR)                                   │
│   2. 采样 SceneDepth, 重建 ViewSpace Z                       │
│   3. 用 ViewSpace Z 选择级联（与 Shadow.glsl 同算法）         │
│   4. mix(SceneColor, CascadeColor, 0.3)                      │
│   5. 输出回 HDR_FBO（覆盖原颜色）                              │
└──────────────────────────────────────────────────────────────┘

★ 同时：清理 Standard.frag / Shadow.glsl / OpaquePass / TransparentPass / RenderContext
   中所有旧的 u_DebugCSMVisualize 相关代码。
```

### 4.2 执行时机

```
EndScene()
  ├─ ExecuteGroup("Shadow")        // ShadowPass
  ├─ ExecuteGroup("Main")          // OpaquePass + SkyboxPass + TransparentPass + PickingPass
  ├─ ExecuteGroup("Debug")    ★ 新增分组（仅当存在启用的 DebugVisualizePass 时执行）
  └─ ExecuteGroup("PostProcess")   // PostProcessPass（Tonemapping → 主 FBO）
```

> **关键**：放在 `Main` 之后、`PostProcess`（含 Tonemapping）之前，意味着调试可视化在 **HDR 空间** 与场景颜色混合，与现有后处理一致；Tonemapping 会自然地把调试色和场景色一起做色调映射，避免调试色穿透 Tonemapping 显得突兀。

---

## 五、详细设计

### 5.1 新增 DebugVisualizePass

#### 5.1.1 文件路径

```
Lucky/Source/Lucky/Renderer/Passes/DebugVisualizePass.h
Lucky/Source/Lucky/Renderer/Passes/DebugVisualizePass.cpp
```

#### 5.1.2 头文件设计（v1.1 终版）

```cpp
// Lucky/Source/Lucky/Renderer/Passes/DebugVisualizePass.h
#pragma once

#include "Lucky/Renderer/RenderPass.h"
#include "Lucky/Renderer/Shader.h"
#include "Lucky/Renderer/Framebuffer.h"

namespace Lucky
{
    /// <summary>
    /// 调试可视化模式枚举
    /// 当前仅支持 CSM 级联可视化，未来可扩展（Normal / UV / Overdraw 等）
    /// </summary>
    enum class DebugVisualizeMode
    {
        None = 0,           // 关闭调试
        CSMCascades = 1,    // CSM 级联颜色叠加
        // 预留扩展：Normal、UV、Overdraw、LightingOnly...
    };

    /// <summary>
    /// 调试可视化 Pass：在 HDR 空间叠加调试信息（级联颜色等）
    /// 属于 "Debug" 分组，在 Main 之后、PostProcess 之前执行
    /// 用户层 Shader 完全不感知调试逻辑
    /// 
    /// 关闭时（Mode == None）Execute 立即返回，零开销
    /// UI 入口：SceneViewportPanel 工具栏（决策 D3）
    /// </summary>
    class DebugVisualizePass : public RenderPass
    {
    public:
        void Init() override;
        void Execute(const RenderContext& context) override;
        void Resize(uint32_t width, uint32_t height) override;

        const std::string& GetName() const override
        {
            static std::string name = "DebugVisualizePass";
            return name;
        }

        const std::string& GetGroup() const override
        {
            static std::string group = "Debug";
            return group;
        }

        /// <summary>
        /// 当前调试模式（外部可查询/设置）
        /// 由 SceneViewportPanel 工具栏读写
        /// </summary>
        DebugVisualizeMode GetMode() const { return m_Mode; }
        void SetMode(DebugVisualizeMode mode) { m_Mode = mode; }

    private:
        /// <summary>
        /// 执行 CSM 级联可视化（输入 HDR_FBO，经 m_PingFBO 中转后写回 HDR_FBO）
        /// </summary>
        void ExecuteCSMCascades(const RenderContext& context);

        DebugVisualizeMode m_Mode = DebugVisualizeMode::None;   // 当前模式
        Ref<Shader> m_CSMVisualizeShader;                       // CSM 级联可视化 Shader
        Ref<Framebuffer> m_PingFBO;                             // PingPong 中转 FBO（避免同纹理读写）
    };
}
```

#### 5.1.3 实现文件（v1.1 终版）

```cpp
// Lucky/Source/Lucky/Renderer/Passes/DebugVisualizePass.cpp
#include "lcpch.h"
#include "DebugVisualizePass.h"

#include "Lucky/Renderer/RenderContext.h"
#include "Lucky/Renderer/RenderCommand.h"
#include "Lucky/Renderer/ScreenQuad.h"
#include "Lucky/Renderer/Renderer3D.h"

namespace Lucky
{
    void DebugVisualizePass::Init()
    {
        m_CSMVisualizeShader = Renderer3D::GetShaderLibrary()->Get("DebugCSMVisualize");

        // 创建 PingPong FBO（同 HDR_FBO 尺寸 + RGBA16F），用于 Pass 中转
        FramebufferSpecification spec;
        spec.Width = 1280;
        spec.Height = 720;
        spec.Attachments = { FramebufferTextureFormat::RGBA16F };
        m_PingFBO = Framebuffer::Create(spec);
    }

    void DebugVisualizePass::Execute(const RenderContext& context)
    {
        if (m_Mode == DebugVisualizeMode::None) return;
        if (!context.HDR_FBO) return;

        switch (m_Mode)
        {
            case DebugVisualizeMode::CSMCascades:
                if (context.ShadowEnabled)
                {
                    ExecuteCSMCascades(context);
                }
                break;
            default:
                break;
        }
    }

    void DebugVisualizePass::Resize(uint32_t width, uint32_t height)
    {
        if (m_PingFBO)
        {
            m_PingFBO->Resize(width, height);
        }
    }

    void DebugVisualizePass::ExecuteCSMCascades(const RenderContext& context)
    {
        const Ref<Framebuffer>& hdrFBO = context.HDR_FBO;
        uint32_t width  = hdrFBO->GetSpecification().Width;
        uint32_t height = hdrFBO->GetSpecification().Height;

        // ---- 阶段 1：HDR -> PingFBO（叠加级联颜色） ----
        m_PingFBO->Bind();
        RenderCommand::SetViewport(0, 0, width, height);
        RenderCommand::SetDepthTest(false);
        RenderCommand::SetDepthWrite(false);

        m_CSMVisualizeShader->Bind();

        RenderCommand::BindTextureUnit(0, hdrFBO->GetColorAttachmentRendererID(0));
        m_CSMVisualizeShader->SetInt("u_SceneColor", 0);

        RenderCommand::BindTextureUnit(1, hdrFBO->GetDepthAttachmentRendererID());
        m_CSMVisualizeShader->SetInt("u_SceneDepth", 1);

        // 决策 A2：InvProjection 已通过 Camera UBO 自动可用，无需在此 SetMat4

        // 上传级联参数（与 Lucky/Shadow.glsl::SelectCascadeIndex 同算法）
        m_CSMVisualizeShader->SetInt("u_CascadeCount", context.CascadeCount);
        for (int i = 0; i < context.CascadeCount; ++i)
        {
            std::string idx = std::to_string(i);
            m_CSMVisualizeShader->SetFloat("u_DebugCascadeFarPlanes[" + idx + "]",
                context.CascadeFarPlanes[i]);
        }

        ScreenQuad::Draw();
        m_PingFBO->Unbind();

        // ---- 阶段 2：PingFBO 颜色 -> HDR_FBO 颜色（GPU Native Blit） ----
        m_PingFBO->BlitColorTo(hdrFBO);
    }
}
```

> **重要**：
> - 决策 D3 落地后，**该 Pass 不再需要 `OnDebugGUI`**。UI 由 `SceneViewportPanel` 提供。
> - `BlitColorTo` 是项目已有的 Framebuffer 接口（参照 `BlitDepthTo`）；若不存在，可改为再绑回 HDR_FBO 用一次直通 Blit Shader 写回。
> - frag shader 端使用 `u_DebugCascadeFarPlanes` 作为 uniform 名（避免与 `Lucky/Shadow.glsl` 中的 `u_CascadeFarPlanes` 同名冲突——本 Pass 不 include Shadow.glsl，但 uniform 名空间共享，使用前缀避免歧义）。

---

### 5.2 新增 DebugCSMVisualize.frag

#### 5.2.1 文件路径

```
Luck3DApp/Assets/Shaders/Internal/Debug/DebugCSMVisualize.frag
Luck3DApp/Assets/Shaders/Internal/Debug/DebugCSMVisualize.vert  （可选，复用 PostProcess/Fullscreen.vert）
```

> **建议复用现有全屏 vert**：项目中 FXAA / Tonemapping / Vignette 都有共用的全屏 vert（通过 `ScreenQuad::Draw` 间接使用），实施时确认 `ScreenQuad` 内部的 vert，新建的 frag 使用同一组 vert 即可。如果 ScreenQuad 内部已绑定 vert，则只需新建 frag。

#### 5.2.2 Frag Shader 设计

```glsl
// Luck3DApp/Assets/Shaders/Internal/Debug/DebugCSMVisualize.frag
// 调试可视化：CSM 级联颜色叠加（v1.1 - 决策 A2 已落地）
// 在 HDR 空间执行，输入 SceneColor + SceneDepth，输出叠加后的颜色

#version 450 core

#include "Lucky/Common.glsl"     // 引入 Camera UBO（含 InvProjectionMatrix）

layout(location = 0) out vec4 o_Color;

in vec2 v_TexCoord;

#define MAX_CASCADE_COUNT 4

uniform sampler2D u_SceneColor;     // HDR 场景颜色
uniform sampler2D u_SceneDepth;     // 场景深度（HDR FBO 深度附件）
uniform int   u_CascadeCount;
uniform float u_DebugCascadeFarPlanes[MAX_CASCADE_COUNT];   // 每级远平面距离（视图空间）

/// <summary>
/// 由屏幕空间 UV 与深度重建视图空间 Z（绝对值，远处更大）
/// 决策 A2：InvProjectionMatrix 直接来自 Camera UBO
/// </summary>
float ReconstructViewDepth(vec2 uv, float depth)
{
    vec4 ndc = vec4(uv * 2.0 - 1.0, depth * 2.0 - 1.0, 1.0);
    vec4 viewPos = u_Camera.InvProjectionMatrix * ndc;
    viewPos.xyz /= viewPos.w;
    return -viewPos.z;  // OpenGL 视图空间 z 为负，取反得正深度
}

int SelectCascadeIndex(float viewDepth)
{
    for (int i = 0; i < u_CascadeCount; ++i)
    {
        if (viewDepth < u_DebugCascadeFarPlanes[i])
        {
            return i;
        }
    }
    return u_CascadeCount - 1;
}

vec3 GetCascadeDebugColor(int cascadeIndex)
{
    if (cascadeIndex == 0) return vec3(1.0, 0.0, 0.0);
    if (cascadeIndex == 1) return vec3(0.0, 1.0, 0.0);
    if (cascadeIndex == 2) return vec3(0.0, 0.0, 1.0);
    return vec3(1.0, 1.0, 0.0);
}

void main()
{
    vec3 sceneColor = texture(u_SceneColor, v_TexCoord).rgb;
    float depth = texture(u_SceneDepth, v_TexCoord).r;

    if (depth >= 1.0)   // 跳过远平面（天空盒等）
    {
        o_Color = vec4(sceneColor, 1.0);
        return;
    }

    float viewDepth = ReconstructViewDepth(v_TexCoord, depth);
    int cascadeIndex = SelectCascadeIndex(viewDepth);
    vec3 cascadeColor = GetCascadeDebugColor(cascadeIndex);

    o_Color = vec4(mix(sceneColor, cascadeColor, 0.3), 1.0);
}
```

#### 5.2.3 注册到 ShaderLibrary

`Renderer3D::Init()` 中已有的 Internal Shader 加载序列后追加：

```cpp
s_Data.ShaderLib->Load("Assets/Shaders/Internal/Debug/DebugCSMVisualize");
```

---

### 5.3 RenderContext 扩展

**目标**：移除 `bool DebugCSMVisualize`，因为这个开关现在归 `DebugVisualizePass` 自己管理，不再需要在 RenderContext 中传递。

```cpp
// RenderContext.h
struct RenderContext
{
    // ... 其他字段保持 ...

    // ---- 调试标志 ----
    // bool DebugCSMVisualize = false;     ← 删除该字段
};
```

> **说明**：原本 `DebugCSMVisualize` 之所以放进 `RenderContext`，是因为 `OpaquePass` / `TransparentPass` 需要把它转发给 Shader。改造后**没有任何 Pass 需要这个标志**（DebugVisualizePass 直接读自己的 `m_Mode`），字段可以彻底删除。

---

### 5.4 ShadowPass 改造

#### 5.4.1 移除 CSM 调试相关成员

```cpp
// ShadowPass.h
class ShadowPass : public RenderPass
{
    // ...

    // ---- 删除这些成员 ----
    // void OnDebugGUI() override;                         ← 删除
    // bool IsDebugCSMVisualize() const { ... }            ← 删除
    // bool m_DebugCSMVisualize = false;                   ← 删除
};
```

#### 5.4.2 移除 OnDebugGUI 实现

`ShadowPass.cpp` 中删除整个 `void ShadowPass::OnDebugGUI()` 函数体。  
（Shadow Map 分辨率 / 级联数量等只读信息**不再展示**——它们属于 ShadowPass 的运行参数，与 RenderPipelinePanel 的"Pass 调试"职责不同；如需展示这些信息，应放到 LightingPanel 或专门的 ShadowSettingsPanel，详见第七章。）

---

### 5.5 OpaquePass / TransparentPass 清理

```cpp
// OpaquePass.cpp 内删除以下行（约第 212 行）：
cmd.MaterialData->GetShader()->SetInt("u_DebugCSMVisualize", context.DebugCSMVisualize ? 1 : 0);

// TransparentPass.cpp 同样位置同样删除。
```

---

### 5.6 Standard.frag 清理

删除末尾整段 CSM 调试代码：

```glsl
// 删除这一整段：
// ---- CSM 调试可视化（级联颜色叠加） ----
if (u_DebugCSMVisualize != 0 && u_ShadowEnabled != 0)
{
    int cascadeIndex = SelectCascadeIndex(v_Input.WorldPos);
    vec3 debugColor = GetCascadeDebugColor(cascadeIndex);
    color = mix(color, debugColor, 0.3);
}
```

最终 `main()` 末尾仅保留：
```glsl
o_Color = vec4(color, alpha);
```

---

### 5.7 Lucky/Shadow.glsl 清理

```glsl
// 删除：
uniform int u_DebugCSMVisualize;    // CSM 级联颜色可视化（0 = 关闭, 1 = 开启）

// 删除整个函数：
vec3 GetCascadeDebugColor(int cascadeIndex) { ... }
```

`SelectCascadeIndex` **保留**（阴影计算还在用）。

> **重要**：`GetCascadeDebugColor` 已被搬到 `DebugCSMVisualize.frag` 内部（与 `SelectCascadeIndex` 拷贝逻辑一致），保持单源真理；`Lucky/Shadow.glsl` 里只留阴影计算所需。

---

### 5.8 Material.cpp 白名单清理

```cpp
// Material.cpp 中 s_InternalUniforms 集合：
"u_TranslucentShadowEnabled",
"u_DebugCSMVisualize",          ← 删除这一行
// ---- 天空盒系统 ----
```

---

### 5.9 Renderer3D 注册与上下文构建

#### 5.9.1 Init() 中注册新 Pass

```cpp
// Renderer3D::Init()  （位于 Pass 创建部分）

auto shadowPass = CreateRef<ShadowPass>();
auto opaquePass = CreateRef<OpaquePass>();
auto skyboxPass = CreateRef<SkyboxPass>();
auto transparentPass = CreateRef<TransparentPass>();
auto pickingPass = CreateRef<PickingPass>();
auto debugVisualizePass = CreateRef<DebugVisualizePass>();   // ★ 新增
auto postProcessPass = CreateRef<PostProcessPass>();
auto silhouettePass = CreateRef<SilhouettePass>();
auto outlineCompositePass = CreateRef<OutlineCompositePass>();

// 设置依赖
outlineCompositePass->SetSilhouettePass(silhouettePass);

// 按执行顺序注册（Shadow -> Main -> Debug -> PostProcess -> Outline）
s_Data.Pipeline.AddPass(shadowPass);
s_Data.Pipeline.AddPass(opaquePass);
s_Data.Pipeline.AddPass(skyboxPass);
s_Data.Pipeline.AddPass(transparentPass);
s_Data.Pipeline.AddPass(pickingPass);
s_Data.Pipeline.AddPass(debugVisualizePass);   // ★ 新增
s_Data.Pipeline.AddPass(postProcessPass);
s_Data.Pipeline.AddPass(silhouettePass);
s_Data.Pipeline.AddPass(outlineCompositePass);
```

#### 5.9.2 EndScene() 中调用新分组

```cpp
// Renderer3D::EndScene() 中：

// ---- 删除这一行（不再需要） ----
// context.DebugCSMVisualize = shadowPass->IsDebugCSMVisualize();

// ---- 在 Main 之后、PostProcess 之前插入 Debug 分组 ----
s_Data.Pipeline.ExecuteGroup("Shadow", context);
s_Data.Pipeline.ExecuteGroup("Main", context);
s_Data.Pipeline.ExecuteGroup("Debug", context);     // ★ 新增
s_Data.Pipeline.ExecuteGroup("PostProcess", context);
```

#### 5.9.3 Shader 加载

```cpp
// Renderer3D::Init() 内 ShaderLib->Load 序列追加：
s_Data.ShaderLib->Load("Assets/Shaders/Internal/Debug/DebugCSMVisualize");
```

---

## 六、关键决策点（多方案对比）

> 实施前请逐个确认以下决策点。每个决策都标注了**推荐方案**与**次优方案**。

### 6.1 决策点 A：世界坐标重建方式

调试 Pass 需要由屏幕 UV + 深度计算视图空间深度，用于选级联。有三种方案：

#### 方案 A1：传 InvProjection，shader 内重建（每帧 SetMat4）

```glsl
vec4 ndc = vec4(uv * 2 - 1, depth * 2 - 1, 1);
vec4 viewPos = u_InvProjection * ndc;
viewPos /= viewPos.w;
float viewDepth = -viewPos.z;
```

| 维度 | 评价 |
|------|------|
| **CPU 开销** | C++ 端 1 次 `glm::inverse()`（每帧一次，可忽略） |
| **GPU 开销** | 1 次 mat4·vec4 + 1 次除法（极低） |
| **修改面** | 仅 DebugVisualizePass 与 frag |
| **侵入性** | 零（不影响 Camera UBO、不影响其他 Shader） |

#### 方案 A2：扩展 Camera UBO 增加 InvProjectionMatrix 字段（★★★★★ 已采用）

```glsl
// Common.glsl
layout(std140, binding = 0) uniform Camera
{
    mat4 ViewProjectionMatrix;
    mat4 InvProjectionMatrix;   // 新增（投影矩阵的逆，用于屏幕空间效果重建视图坐标）
    vec3 Position;
    float _padding;             // 16 字节对齐
} u_Camera;
```

```cpp
// Renderer3D::CameraUBOData
struct CameraUBOData
{
    glm::mat4 ViewProjectionMatrix;
    glm::mat4 InvProjectionMatrix;   // 新增
    glm::vec3 Position;
    char padding[4];
};

// BeginScene 中填充：
s_Data.CameraBuffer.ViewProjectionMatrix = camera.GetViewProjectionMatrix();
s_Data.CameraBuffer.InvProjectionMatrix  = glm::inverse(camera.GetProjectionMatrix());
s_Data.CameraBuffer.Position             = camera.GetPosition();
```

| 优点 | 缺点 |
|------|------|
| 多个 Pass 都需要时可复用（未来 SSR / SSAO / DOF） | 改动 UBO 结构，需同步 C++ 端结构体大小 |
| 不需要每个 Pass 单独 SetMat4 | UBO 体积小幅增加（64 B） |
| Shader 端语义统一（都从 u_Camera 读） | |

#### 方案 A3：直接传 ViewSpace 远平面 + NDC 线性化公式

利用 `viewZ = farPlane * nearPlane / (farPlane - depth * (farPlane - nearPlane))` 的简化公式。

| 优点 | 缺点 |
|------|------|
| 无需 mat4 计算 | 需要传 nearPlane / farPlane，且公式与投影矩阵假设耦合 |
| 性能略好（mat4 → 几个标量） | 只对透视投影正确；非标准投影会出错 |

**评价**：性能优势忽略不计，正确性更脆弱。

#### 决策（v1.1 已确认）

- ★★★★★ **方案 A2（已采用）** —— 一次性扩展，未来屏幕空间效果可直接复用
- ★★★★ 方案 A1
- ★★ 方案 A3（不推荐）

---

### 6.2 决策点 B：调试 Pass 的渲染目标

调试可视化要把级联色叠加到哪个 Buffer？

#### 方案 B1：写回 HDR_FBO（覆盖叠加）（★★★★★ 推荐）

直接把 HDR_FBO 颜色采样后混合，再写回 HDR_FBO（注意需要 Ping-Pong，因为 OpenGL 不允许同时读写同一纹理）。

> **修正**：HDR_FBO 是同一个纹理，shader 同时采样 + 写入会 UB。两种应对：
> 1. **方案 B1-a（推荐）**：使用 `PostProcessStack` 的 PingPong FBO 做中转
> 2. **方案 B1-b**：让 DebugVisualizePass 自己持有一个 PingPong FBO

#### 方案 B1-a：通过 PostProcessStack 的 PingPong 中转（★★★★★）

```cpp
// Execute 时：
// 1. 把 HDR_FBO 颜色采样并叠加级联色，写到 PostProcessPass.PingFBO
// 2. 把 PingFBO 颜色用一个简单的 Blit Shader 写回 HDR_FBO
```

| 优点 | 缺点 |
|------|------|
| 复用现有 PingPong FBO | 需要 PostProcessPass 暴露 PingFBO 访问接口 |
| 不增加显存 | 比 B1-b 多 1 次 Blit |

#### 方案 B1-b：DebugVisualizePass 自持 PingPong FBO（★★★★）

```cpp
class DebugVisualizePass {
    Ref<Framebuffer> m_PingFBO;     // 与 HDR_FBO 同尺寸 + RGBA16F
};
```

| 优点 | 缺点 |
|------|------|
| 完全自包含 | 多占用 1 张 RGBA16F（约 8MB @ 1080p），仅调试使用 |
| 与 PostProcessPass 解耦 | |

#### 方案 B2：Glsl `imageLoad` + `imageStore` 同纹理读写

| 优点 | 缺点 |
|------|------|
| 无需中转 FBO | 需要 GL 4.2+ + 显式 barrier，复杂度高 |
| | 性能未必更好 |

#### 方案 B3：写到独立 Debug FBO，最后再 Blit

类似 B1-b 但用一个独立的 RGBA8 FBO（不需要 HDR）。

#### 决策

- ★★★★★ **方案 B1-b** —— 自包含、改动最小（PostProcessPass 不需暴露内部 FBO）
- ★★★★ 方案 B1-a（如果不希望多占显存）
- ★★ 方案 B2 / B3

> **实施细节**（推荐 B1-b）：
> - `DebugVisualizePass::Init()` 中创建 `m_PingFBO`（RGBA16F + 1280×720 默认）
> - `DebugVisualizePass::Resize()` 同步调整 `m_PingFBO`
> - `Execute()`：先写 PingFBO，然后用 Tonemapping 的"直通模式"或新增简单 Blit shader 把结果回写 HDR_FBO
> - **更简单的做法**：直接 `glBlitFramebuffer` 从 PingFBO 拷贝到 HDR_FBO（GPU Native blit，无需 shader）—— 这是最简形式

---

### 6.3 决策点 C：调试开关存储位置

#### 方案 C1：DebugVisualizePass 自己持有 m_Mode（★★★★★ 推荐）

```cpp
class DebugVisualizePass {
    DebugVisualizeMode m_Mode = DebugVisualizeMode::None;
};
```

#### 方案 C2：放到 RenderContext

需要 Renderer3D 在 EndScene 中再次同步该字段，与"用户 Shader 不感知"的目标一致，但增加一次同步路径。

#### 方案 C3：放到全局单例（如 RenderSettings）

过度抽象。

**决策**：★★★★★ **C1**。Pass 自管自己的状态，是 R7 架构的本意。

---

### 6.4 决策点 D：调试开关 UI 入口

#### 方案 D1：保留 `OnDebugGUI()` + RenderPipelinePanel

继续在 RenderPipelinePanel 中通过 `pass->OnDebugGUI()` 列出。

#### 方案 D2：移除 `OnDebugGUI`，改为独立的 DebugVisualizePanel

新建一个面板专门管理调试开关。

#### 方案 D3：将调试开关合并进视口工具栏（★★★★★ 已采用）

参考 Unity 的 "Scene View Toolbar"。本项目已有 `SceneViewportPanel` 顶部 ToolBar（含 Local/World 下拉、Grid 按钮），直接追加 "CSM" 勾选按钮。

```cpp
// SceneViewportPanel.cpp 的 ToolBar 区域追加：
//   - 在 Grid 按钮之后用 ImGui::SameLine() 接 "CSM" 按钮
//   - 点击切换 DebugVisualizePass::SetMode( None <-> CSMCascades )
//   - 按钮颜色按 Grid 同款逻辑（蓝色=开启 / 灰色=关闭）
//   - 状态由 Pass 自身托管（C1 决策），SceneViewportPanel 不持有重复状态
```

#### 决策（v1.1 已确认）

- ★★★★★ **方案 D3（已采用）** —— 与主流 DCC / 引擎一致，用户使用最方便，不新建额外面板

---

### 6.5 决策点 E：Pass 分组归属

#### 方案 E1：新建 "Debug" 分组（★★★★★ 推荐）

```cpp
const std::string& GetGroup() const override {
    static std::string group = "Debug";
    return group;
}
```

`Renderer3D::EndScene()` 显式调用 `ExecuteGroup("Debug", context)`。

| 优点 |
|------|
| 语义清晰，未来加更多调试 Pass 自然归入 |
| 调用顺序可控（在 Main 之后、PostProcess 之前） |
| 关闭时整个分组都不执行（如果以后加更多 Debug Pass） |

#### 方案 E2：归入 "Main" 分组

会和 OpaquePass / SkyboxPass 等混在一起，难以理解执行顺序。

#### 方案 E3：归入 "PostProcess" 分组

会被 PostProcess 的 Tonemapping 影响时机（Tonemapping 是 PostProcessPass 内部最后一步），逻辑模糊。

**决策**：★★★★★ **E1**。

---

## 七、Pass DebugUI 方法移除分析

### 7.1 当前现状

`RenderPass` 基类提供：
```cpp
virtual void OnDebugGUI() {}   // 默认空实现
```

实测代码中（截至本期）：
- **唯一实现者**：`ShadowPass`（仅用于 CSM 调试可视化开关 + 只读分辨率/级联数量信息）
- **调用者**：`RenderPipelinePanel::OnGUI()` 中对每个 Pass 展开区域调用

### 7.2 用户的诉求

> "现在的每个 Pass 都有一个 DebugUI 方法，我觉得应该去掉，分析一下应该怎么做"

需要回答：
1. 这个方法**是否真的应该去掉**？
2. 如果去掉，**调试开关 UI 应放在哪里**？

### 7.3 分析

#### 7.3.1 OnDebugGUI 的本质问题

| 问题 | 说明 |
|------|------|
| **职责越界** | RenderPass 的核心职责是**渲染**，不是**绘制 UI**。把 ImGui 调用放在 Renderer 层会让 Lucky 引擎核心**强依赖 ImGui**（即使运行时不画 UI，编译期也得链接） |
| **耦合方向反了** | 正确方向应该是：UI 层（Editor）依赖渲染层；现在却是渲染层提供了 UI 层使用的虚函数 |
| **分层混乱** | Lucky/Renderer/Passes/ShadowPass.cpp 里出现了 ImGui / UI:: 调用，违反"Renderer 不感知 Editor"的分层 |
| **调试模式分散** | 每个 Pass 都可能有自己的调试 UI，调试入口散落各处，找起来困难 |
| **运行时 vs 编辑器混淆** | 调试 UI 只在编辑器中有意义，不该污染运行时（Runtime 模式下需要在 Pass 内 `#ifdef LK_EDITOR`） |

#### 7.3.2 本期 CSM 迁移让 OnDebugGUI 变得**可有可无**

| 项 | 迁移前 | 迁移后 |
|----|-------|-------|
| ShadowPass 还需要 OnDebugGUI 吗？ | 需要（CSM 开关） | **不再需要**（开关迁到 DebugVisualizePass） |
| DebugVisualizePass 需要 OnDebugGUI 吗？ | - | **可以需要也可以不需要**（取决于把 UI 放在哪） |
| 其他 Pass 还有 OnDebugGUI 吗？ | 没有 | 没有 |

也就是说：**移除 OnDebugGUI 是顺手就能完成的清理**。

### 7.4 替代方案对比

#### 方案 ?：保留 OnDebugGUI（不动）（★★ 不推荐）

| 优点 | 缺点 |
|------|------|
| 改动最小 | 上述所有问题都还在 |

#### 方案 ?：移除 OnDebugGUI，新建 DebugVisualizePanel（★★★★★ 强烈推荐）

**核心思想**：调试 UI 是 Editor 层的职责，应由 Editor 层主动从渲染层"拉"数据，而非渲染层"推"UI。

```cpp
// Luck3DApp/Source/Panels/DebugVisualizePanel.h（新建）
namespace Lucky
{
    class DebugVisualizePanel : public Panel
    {
    public:
        void OnGUI() override;
    };
}
```

```cpp
// DebugVisualizePanel.cpp
void DebugVisualizePanel::OnGUI()
{
    auto pass = Renderer3D::GetPipeline().GetPass<DebugVisualizePass>();
    if (!pass) return;

    if (UI::BeginPrimaryCollapsing("Visualize"))
    {
        DebugVisualizeMode mode = pass->GetMode();
        // ... 下拉框 + 说明 ...
        pass->SetMode(mode);
        UI::EndPrimaryCollapsing();
    }
}
```

**同时改造 RenderPipelinePanel**：
```cpp
// 删除：
// pass->OnDebugGUI();   ← 不再调用

// RenderPipelinePanel 退化为只展示：
//   - Statistics（DrawCalls / Triangles / Vertices）
//   - Pass 列表（Name + Group + Enable 复选框）
// 不再承担"每个 Pass 的调试参数"职责
```

**RenderPass 基类**：
```cpp
class RenderPass {
    // 删除：
    // virtual void OnDebugGUI() {}      ← 移除整个方法
};
```

| 优点 | 缺点 |
|------|------|
| ? Renderer 层完全不依赖 ImGui | 需要新建 Panel + 注册到 Editor |
| ? 调试入口集中（所有调试 mode 在一个面板） | |
| ? Pass 接口更纯粹 | |
| ? 未来加 Normal / UV 等模式时，UI 改动集中在 DebugVisualizePanel | |
| ? 与 LightingPanel / RenderPipelinePanel 同级，符合现有 Panel 体系 | |

#### 方案 ?：移除 OnDebugGUI，调试 UI 收纳到 RenderPipelinePanel 内的"Debug 区域"

```cpp
// RenderPipelinePanel::OnGUI()
if (UI::BeginPrimaryCollapsing("Debug"))
{
    // 直接在这里写 DebugVisualizePass 的 UI
    auto pass = Renderer3D::GetPipeline().GetPass<DebugVisualizePass>();
    DebugVisualizeMode mode = pass->GetMode();
    // ...
    pass->SetMode(mode);
    UI::EndPrimaryCollapsing();
}
```

| 优点 | 缺点 |
|------|------|
| 不新建 Panel | RenderPipelinePanel 又开始承担调试 UI 职责，未来有更多调试模式时面板会膨胀 |
| 改动小 | |

#### 方案 ?：场景视口顶部工具栏放 Draw Mode 下拉（Unity 风格）（★★★★ 长期理想）

参考 Unity Scene View 顶部的 "Shaded / Wireframe / Shaded Wireframe / Cascade Visualization / ..." 下拉。

| 优点 | 缺点 |
|------|------|
| 与主流 DCC / 引擎一致 | 需要先有 Scene View 工具栏体系 |
| 用户使用最方便 | 当前项目无此基础设施，改动大 |

### 7.5 推荐路径（v1.1 已确认）

| 阶段 | 方案 | 理由 |
|------|------|------|
| **本期（R30）** | ? —— 视口工具栏 "CSM" 勾选按钮 | 与决策 D3 一致；项目已有 ToolBar 基础设施，无需新建面板 |

本期既**移除 OnDebugGUI**（彻底解耦 Renderer 与 ImGui），又**直接落地视口工具栏入口**（决策 D3），合二为一。

#### 本期具体改动

| 文件 | 改动 |
|------|------|
| `RenderPass.h` | 删除 `virtual void OnDebugGUI() {}` |
| `ShadowPass.h/cpp` | 删除 `OnDebugGUI` 重写、`m_DebugCSMVisualize`、`IsDebugCSMVisualize()` |
| `RenderPipelinePanel.cpp` | 删除 `pass->OnDebugGUI()` 调用，仅保留 Enable 复选框 |
| `DebugVisualizePass.h` | **不再需要 `OnDebugGUI` override**，仅提供 `GetMode()` / `SetMode()` |
| `SceneViewportPanel.cpp` | **追加 "CSM" 勾选按钮**（与 Grid 按钮同款式，调用 `DebugVisualizePass::SetMode`） |

> **注意**：5.1.2 节中给出的 `DebugVisualizePass.h` 头文件已采用最终版（无 `OnDebugGUI` override）。

---

## 八、文件清单与改动汇总

### 8.1 新增文件

| 文件 | 类型 | 说明 |
|------|------|------|
| `Lucky/Source/Lucky/Renderer/Passes/DebugVisualizePass.h` | C++ | 调试可视化 Pass 头文件 |
| `Lucky/Source/Lucky/Renderer/Passes/DebugVisualizePass.cpp` | C++ | 调试可视化 Pass 实现 |
| `Luck3DApp/Assets/Shaders/Internal/Debug/DebugCSMVisualize.frag` | GLSL | 调试可视化片段着色器 |

### 8.2 修改文件

| 文件 | 改动 |
|------|------|
| `Luck3DApp/Assets/Shaders/Standard.frag` | 删除末尾 5 行 CSM 调试代码 |
| `Luck3DApp/Assets/Shaders/Lucky/Shadow.glsl` | 删除 `u_DebugCSMVisualize` uniform 与 `GetCascadeDebugColor()` 函数 |
| `Lucky/Source/Lucky/Renderer/Material.cpp` | 从 `s_InternalUniforms` 删除 `u_DebugCSMVisualize` |
| `Lucky/Source/Lucky/Renderer/RenderContext.h` | 删除 `bool DebugCSMVisualize` 字段 |
| `Lucky/Source/Lucky/Renderer/RenderPass.h` | 删除 `virtual void OnDebugGUI() {}`（方案 ?） |
| `Lucky/Source/Lucky/Renderer/Passes/ShadowPass.h` | 删除 `OnDebugGUI` / `IsDebugCSMVisualize` / `m_DebugCSMVisualize` |
| `Lucky/Source/Lucky/Renderer/Passes/ShadowPass.cpp` | 删除 `OnDebugGUI` 实现 |
| `Lucky/Source/Lucky/Renderer/Passes/OpaquePass.cpp` | 删除 `SetInt("u_DebugCSMVisualize", ...)` |
| `Lucky/Source/Lucky/Renderer/Passes/TransparentPass.cpp` | 同上 |
| `Lucky/Source/Lucky/Renderer/Renderer3D.cpp` | 注册新 Pass + 新增 `ExecuteGroup("Debug")` + 加载 Shader + 删除 `context.DebugCSMVisualize` 同步 |
| `Luck3DApp/Source/Panels/RenderPipelinePanel.cpp` | 删除 `pass->OnDebugGUI()` 调用（方案 ?） |
| `Luck3DApp/Source/...EditorLayer.cpp` | 注册 DebugVisualizePanel（方案 ?） |

### 8.3 改动量统计

| 类别 | 数量 |
|------|------|
| 新增 C++ 文件 | 2 |
| 新增 GLSL 文件 | 1 |
| 修改 C++ 文件 | 11（含 Common.glsl C++端 UBO 同步） |
| 修改 GLSL 文件 | 3（Standard.frag / Shadow.glsl / Common.glsl） |
| 总改动文件数 | **15** |

---

## 九、实施步骤

### Step 1：扩展 Camera UBO（决策 A2）
1. `Renderer3D::CameraUBOData` 增加 `glm::mat4 InvProjectionMatrix`
2. `Renderer3D::BeginScene()` 中填充 `s_Data.CameraBuffer.InvProjectionMatrix = glm::inverse(camera.GetProjectionMatrix())`
3. `Common.glsl` Camera UBO 块增加对应字段（保持 std140 对齐）

### Step 2：基础设施
4. 新建 `Internal/Debug/DebugCSMVisualize.frag`
5. 在 `Renderer3D::Init()` 中追加 `ShaderLib->Load("Assets/Shaders/Internal/Debug/DebugCSMVisualize")`

### Step 3：DebugVisualizePass 实现
6. 新建 `DebugVisualizePass.h/cpp`（无 OnDebugGUI override）
7. 在 `Renderer3D::Init()` 中创建并注册到 Pipeline
8. 在 `Renderer3D::EndScene()` 中插入 `ExecuteGroup("Debug", context)`

### Step 4：清理用户层
9. 删除 `Standard.frag` 末尾 CSM 调试代码
10. 删除 `Lucky/Shadow.glsl` 中 `u_DebugCSMVisualize` 与 `GetCascadeDebugColor`
11. 删除 `Material.cpp` 白名单中 `u_DebugCSMVisualize`

### Step 5：清理引擎中转层
12. 删除 `OpaquePass.cpp` / `TransparentPass.cpp` 中 `SetInt("u_DebugCSMVisualize", ...)`
13. 删除 `RenderContext.h` 中 `DebugCSMVisualize` 字段
14. 删除 `Renderer3D::EndScene()` 中 `context.DebugCSMVisualize = ...` 一行
15. 删除 `ShadowPass.h/cpp` 中 `OnDebugGUI` / `IsDebugCSMVisualize` / `m_DebugCSMVisualize`

### Step 6：移除 OnDebugGUI 接口
16. 删除 `RenderPass.h` 中 `virtual void OnDebugGUI() {}`
17. 删除 `RenderPipelinePanel.cpp` 中 `pass->OnDebugGUI()` 调用与相关 include

### Step 7：视口工具栏添加 CSM 勾选按钮（决策 D3）
18. 在 `SceneViewportPanel.cpp` 的 ToolBar 区域 "Grid" 按钮之后追加 "CSM" 按钮（同款样式）
19. 按钮点击时调用 `Renderer3D::GetPipeline().GetPass<DebugVisualizePass>()->SetMode(...)` 在 None / CSMCascades 之间切换
20. 按钮当前状态显示从 `pass->GetMode()` 获取（避免重复持有状态）

### Step 8：验证
21. 启动编辑器
22. 点击视口工具栏 "CSM" 按钮
23. 确认场景中可见红/绿/蓝/黄四级颜色
24. 确认场景中天空盒区域不被染色
25. 再次点击关闭，确认场景表现回到 PBR 正常状态
26. 创建用户自定义 Shader（如复制 Standard.frag 改名 Toon.frag），确认调试模式下也能看到级联色（说明零侵入成功）

---

## 十、风险与回滚

### 10.1 风险点

| 风险 | 等级 | 缓解 |
|------|------|------|
| 重建 ViewSpace Z 与 Shadow.glsl 中算法不一致 | 中 | 在 frag 中保留与 `Lucky/Shadow.glsl::SelectCascadeIndex` 相同的算法注释 + 单元测试比对级联结果 |
| HDR FBO 同纹理读写 UB | 高 | 必须使用 PingPong（方案 B1-b） |
| 调试 Pass Resize 未跟随视口 | 中 | `DebugVisualizePass::Resize()` 中同步 `m_PingFBO->Resize` |
| 用户自定义 Shader 缺少调试可视化 | 低 | 设计本身解决——调试在 Pass 中而不在 Shader 中 |
| `UI::PropertyDropdown` 接口不存在 | 低 | 实施时如未找到，退化为 `ImGui::Combo` |

### 10.2 回滚

如发现问题需要回滚：
1. 在 `DebugVisualizePass::Execute()` 顶部强制 `return`，调试功能完全失效但不影响主渲染
2. 完整回滚：参考第八章修改文件清单，逆向操作即可恢复（所有改动都是可逆的）

---

## 附录：与上一阶段（R29 PointLight Shadow）的差异

| 项 | R29 之前（含 R30 改造前） | R30 改造后 |
|----|---------------------------|-----------|
| 用户 Shader 是否含调试代码 | 是 | **否** |
| RenderPass 是否依赖 ImGui | 是（通过 OnDebugGUI） | **否** |
| 调试可视化扩展性 | 每加一种需修改用户层 | 仅新增 Pass |
| 调试入口 | RenderPipelinePanel 的 ShadowPass 展开 | **DebugVisualizePanel 集中管理** |

