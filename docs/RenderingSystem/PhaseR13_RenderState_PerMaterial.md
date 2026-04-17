# Phase R13：渲染状态（Per-Material RenderState）

> **文档版本**：v1.0  
> **创建日期**：2026-04-17  
> **优先级**：?? 中（Phase 0 背面剔除可立即实施）  
> **预计工作量**：Phase 0 约 5 分钟，Phase 1 约 2-3 天，Phase 2 约 1-2 天  
> **前置依赖**：R9 DrawCommand 排序（Phase 1 依赖）  
> **文档说明**：本文档详细设计 Per-Material 渲染状态系统，使每个材质可以独立控制 CullMode、ZWrite、ZTest、BlendMode、RenderingMode 等 GPU 渲染状态。参考 Unity ShaderLab 的渲染状态模型和 Hazel 引擎的 PipelineSpecification 设计。  
> **关联文档**：  
> - `PhaseR9_DrawCommand_Sorting.md`（DrawCommand 排序 + RenderQueue 演进）  
> - `PhaseR12_Renderer_Architecture_Evolution.md`（渲染器架构演进）

---

## 目录

- [一、渲染状态概念说明](#一渲染状态概念说明)
  - [1.1 RenderingMode（渲染模式）](#11-renderingmode渲染模式)
  - [1.2 ZWrite（深度写入）](#12-zwrite深度写入)
  - [1.3 ZTest（深度测试）](#13-ztest深度测试)
  - [1.4 CullMode（面剔除模式）](#14-cullmode面剔除模式)
  - [1.5 BlendMode（混合模式）](#15-blendmode混合模式)
  - [1.6 RenderQueue（渲染队列）](#16-renderqueue渲染队列)
  - [1.7 各状态之间的关系](#17-各状态之间的关系)
- [二、当前项目现状分析](#二当前项目现状分析)
  - [2.1 现有渲染状态设置](#21-现有渲染状态设置)
  - [2.2 问题总结](#22-问题总结)
- [三、必要性与优先级分析](#三必要性与优先级分析)
- [四、Hazel 引擎参考](#四hazel-引擎参考)
  - [4.1 Hazel 的渲染状态设计](#41-hazel-的渲染状态设计)
  - [4.2 设计差异与借鉴](#42-设计差异与借鉴)
- [五、详细设计方案](#五详细设计方案)
  - [5.1 渲染状态枚举定义](#51-渲染状态枚举定义)
  - [5.2 RenderState 结构体](#52-renderstate-结构体)
  - [5.3 RenderingMode 预设](#53-renderingmode-预设)
  - [5.4 Material 集成 RenderState](#54-material-集成-renderstate)
  - [5.5 RenderCommand 扩展](#55-rendercommand-扩展)
  - [5.6 绘制流程中应用 RenderState](#56-绘制流程中应用-renderstate)
  - [5.7 Inspector 面板 UI](#57-inspector-面板-ui)
  - [5.8 序列化支持](#58-序列化支持)
- [六、与其他模块的关联](#六与其他模块的关联)
  - [6.1 与 R9 DrawCommand 排序的关联](#61-与-r9-drawcommand-排序的关联)
  - [6.2 与 R12 渲染器架构演进的关联](#62-与-r12-渲染器架构演进的关联)
  - [6.3 与 R4 阴影系统的关联](#63-与-r4-阴影系统的关联)
- [七、渐进式实施路线](#七渐进式实施路线)
  - [7.1 Phase 0：立即可做（全局背面剔除）](#71-phase-0立即可做全局背面剔除)
  - [7.2 Phase 1：Per-Material 渲染状态](#72-phase-1per-material-渲染状态)
  - [7.3 Phase 2：RenderingMode 预设 + 透明物体支持](#73-phase-2renderingmode-预设--透明物体支持)
- [八、涉及的文件清单](#八涉及的文件清单)
- [九、验证方法](#九验证方法)
- [十、设计决策记录](#十设计决策记录)

---

## 一、渲染状态概念说明

> 本章节解释 Unity ShaderLab 中常见的渲染状态概念，说明每个状态的作用、可选值和典型用途。

### 1.1 RenderingMode（渲染模式）

RenderingMode 是 Unity Standard Shader 中的一个**便捷预设**，它不是一个独立的 GPU 状态，而是一组渲染状态的组合：

| 模式 | 含义 | Blend | ZWrite | RenderQueue | 典型用途 |
|------|------|-------|--------|-------------|---------|
| **Opaque** | 完全不透明 | 关闭 | 开启 | Geometry (2000) | 石头、金属、地面 |
| **Cutout** | Alpha 裁剪 | 关闭 | 开启 | AlphaTest (2450) | 树叶、铁丝网、草 |
| **Fade** | 整体淡入淡出 | SrcAlpha / OneMinusSrcAlpha | 关闭 | Transparent (3000) | 全息投影、幽灵 |
| **Transparent** | 半透明（保留高光） | SrcAlpha / OneMinusSrcAlpha | 关闭 | Transparent (3000) | 玻璃、水面 |

**本质**：RenderingMode 是一个"一键设置"，选择后自动配置 Blend、ZWrite、RenderQueue 等多个状态。用户也可以手动覆盖这些状态。

### 1.2 ZWrite（深度写入）

控制当前物体的深度值是否写入深度缓冲区：

```
ZWrite On（默认）：
  物体的深度值写入深度缓冲区
  → 后续物体如果在它后面，会被深度测试剔除
  → 适用于不透明物体

ZWrite Off：
  物体不写入深度缓冲区
  → 不会"挡住"后面的物体
  → 适用于透明物体（否则透过透明物体看不到后面的东西）
```

**直觉理解**：

```
场景：相机 → [玻璃 depth=5] → [墙壁 depth=10]

ZWrite On（错误效果）：
  1. 绘制玻璃 → 深度缓冲写入 5
  2. 绘制墙壁 → 深度 10 > 5 → 深度测试失败 → 墙壁不绘制
  3. 结果：透过玻璃看不到墙壁 ?

ZWrite Off（正确效果）：
  1. 绘制墙壁 → 深度缓冲写入 10（不透明物体先绘制）
  2. 绘制玻璃 → 深度 5 < 10 → 深度测试通过 → 玻璃绘制
  3. 玻璃不写入深度 → 墙壁的深度值保留
  4. 结果：透过玻璃看到墙壁 ?
```

### 1.3 ZTest（深度测试）

控制深度比较函数，决定一个像素是否应该被绘制：

| 值 | 含义 | 用途 |
|---|------|------|
| **LEqual**（Unity 默认） | 深度 ≤ 缓冲区中的值时通过 | 正常渲染 |
| **Less**（OpenGL 默认） | 深度 < 缓冲区中的值时通过 | 严格前方 |
| **Greater** | 深度 > 缓冲区中的值时通过 | 被遮挡时才绘制（X 光/轮廓效果） |
| **Equal** | 深度 = 缓冲区中的值时通过 | 贴花（Decal） |
| **GEqual** | 深度 ≥ 缓冲区中的值时通过 | 特殊效果 |
| **NotEqual** | 深度 ≠ 缓冲区中的值时通过 | 特殊效果 |
| **Always** | 总是通过 | 忽略深度，总是绘制（UI 叠加） |
| **Never** | 永不通过 | 不绘制（调试用） |

**典型用例**：
- 游戏中角色被墙壁遮挡时显示轮廓 → 用 `ZTest Greater` 的 Pass 绘制轮廓色
- 贴花系统（Decal）→ 用 `ZTest Equal` 精确贴合表面

### 1.4 CullMode（面剔除模式）

控制哪些三角形面不被渲染，用于性能优化和特殊效果：

| 值 | 含义 | 用途 |
|---|------|------|
| **Back**（默认） | 剔除背面（法线朝外的面保留） | 正常不透明物体（看不到内部） |
| **Front** | 剔除正面（法线朝外的面被剔除） | 特殊效果（如描边 Pass 的膨胀法） |
| **Off** | 不剔除（双面渲染） | 树叶、布料、纸片等两面都能看到的薄物体 |

**性能影响**：

```
Back Cull（默认）：
  封闭物体只渲染外表面 → 三角形数量减半 → 光栅化性能提升约 50%

Off（双面渲染）：
  正面和背面都渲染 → 三角形数量翻倍 → 仅用于需要双面可见的物体
```

**当前项目问题**：`RenderCommand::Init()` 中**没有开启背面剔除**（`glEnable(GL_CULL_FACE)` 缺失），意味着所有物体都在做双面渲染，白白浪费约 50% 的光栅化性能。

### 1.5 BlendMode（混合模式）

控制新绘制的像素颜色如何与帧缓冲区中已有的颜色混合：

| 模式 | 公式 | 用途 |
|------|------|------|
| **None（关闭混合）** | `FinalColor = SrcColor` | 不透明物体 |
| **SrcAlpha / OneMinusSrcAlpha** | `Final = Src × α + Dst × (1-α)` | 标准 Alpha 混合（半透明） |
| **One / One** | `Final = Src + Dst` | 叠加混合（粒子、发光效果） |
| **SrcAlpha / One** | `Final = Src × α + Dst` | 预乘 Alpha 叠加 |

**当前项目问题**：`RenderCommand::Init()` 中**全局开启了混合**（`glEnable(GL_BLEND)`），对于不透明物体来说这是不必要的，会有轻微的性能损失（GPU 需要读取帧缓冲区做混合计算）。

### 1.6 RenderQueue（渲染队列）

控制物体的绘制顺序，值越小越先绘制：

```
Background  = 1000   ← 天空盒（最先画）
Geometry    = 2000   ← 不透明物体（默认）
AlphaTest   = 2450   ← Alpha 裁剪物体
Transparent = 3000   ← 透明物体（从后到前画）
Overlay     = 4000   ← UI 叠加（最后画）
```

> **注意**：RenderQueue 的详细演进方案已在 `PhaseR9_DrawCommand_Sorting.md` 第七章中完整描述，本文档不再重复。R13 的 RenderState 中包含 RenderQueue 字段，与 R9 的演进方案无缝衔接。

### 1.7 各状态之间的关系

```
┌─────────────────────────────────────────────────────────────────┐
│                    RenderingMode（便捷预设）                      │
│                                                                 │
│  选择 RenderingMode 会自动设置以下状态：                          │
│                                                                 │
│  ┌──────────┐  ┌──────────┐  ┌──────────┐  ┌──────────────┐    │
│  │ BlendMode│  │ ZWrite   │  │ ZTest    │  │ RenderQueue  │    │
│  │          │  │          │  │          │  │              │    │
│  │ 混合模式  │  │ 深度写入  │  │ 深度测试  │  │ 渲染队列     │    │
│  └──────────┘  └──────────┘  └──────────┘  └──────────────┘    │
│                                                                 │
│  CullMode 独立于 RenderingMode，需要单独设置                      │
│  ┌──────────┐                                                   │
│  │ CullMode │                                                   │
│  │ 面剔除   │                                                   │
│  └──────────┘                                                   │
└─────────────────────────────────────────────────────────────────┘
```

---

## 二、当前项目现状分析

### 2.1 现有渲染状态设置

当前所有渲染状态都在 `RenderCommand::Init()` 中**全局一次性设置**：

```cpp
// RenderCommand.cpp - Init()
void RenderCommand::Init()
{
    glEnable(GL_BLEND);                                 // 全局开启混合
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);  // 全局混合函数

    glEnable(GL_DEPTH_TEST);    // 全局开启深度测试

    glEnable(GL_LINE_SMOOTH);   // 平滑直线
}
```

### 2.2 问题总结

| 渲染状态 | 当前实现 | 问题 |
|---------|---------|------|
| **CullMode** | ? 未启用面剔除 | 所有三角形都渲染（包括背面），浪费约 50% 光栅化性能 |
| **ZWrite** | ? 全局开启（`glEnable(GL_DEPTH_TEST)` 隐含） | 无法按材质控制，透明物体会出错 |
| **ZTest** | ? 全局默认 `GL_LESS` | 无法按材质控制，无法实现特殊效果 |
| **BlendMode** | ? 全局开启 `GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA` | 不透明物体不需要混合，有轻微性能损失 |
| **RenderQueue** | ? 无 | 无法控制渲染顺序（R9 已规划演进方案） |
| **RenderingMode** | ? 无 | 无法区分不透明/透明物体 |

**核心问题**：所有渲染状态都是全局的，没有任何 Per-Material 的渲染状态控制能力。

---

## 三、必要性与优先级分析

| 状态 | 必要性 | 理由 |
|------|--------|------|
| **CullMode** | ??? **高** | 当前没有开启背面剔除，所有物体都在做双面渲染，白白浪费约 50% 的光栅化性能。这是最基础的优化，5 分钟即可完成 |
| **ZWrite** | ?? 中 | 当需要透明物体时必须有。当前全是不透明物体，暂时不紧急 |
| **ZTest** | ? 低 | 高级效果才需要（轮廓、贴花等），短期不需要 |
| **BlendMode** | ?? 中 | 当前全局开启混合对不透明物体有轻微性能损失，但影响不大。需要透明物体时必须支持 Per-Material 控制 |
| **RenderingMode** | ?? 中 | 本质是一个预设组合，等 ZWrite/Blend/RenderQueue 都有了再加也不迟 |
| **RenderQueue** | ?? 中 | R9 文档第七章已有完整演进方案，等 DrawCommand 排序实现后自然引入 |

---

## 四、Hazel 引擎参考

### 4.1 Hazel 的渲染状态设计

Hazel 引擎将渲染状态放在 `PipelineSpecification` 中（因为 Hazel 面向 Vulkan，Vulkan 的 Pipeline State Object 要求渲染状态绑定在 Pipeline 上）：

```cpp
// Hazel 的 Pipeline 规格
struct PipelineSpecification
{
    Ref<Shader> Shader;
    FramebufferSpecification TargetFramebuffer;
    VertexBufferLayout Layout;
    
    // 渲染状态
    DepthCompareOperator DepthOperator = DepthCompareOperator::Greater;  // 反向深度
    bool BackfaceCulling = true;
    bool DepthTest = true;
    bool DepthWrite = true;
    bool Wireframe = false;
    float LineWidth = 1.0f;
    
    PrimitiveTopology Topology = PrimitiveTopology::Triangles;
};
```

Hazel 的 `Material` 中使用位标志控制渲染状态：

```cpp
// Hazel 的 MaterialFlag
enum class MaterialFlag
{
    None        = BIT(0),
    DepthTest   = BIT(1),
    Blend       = BIT(2),
    TwoSided    = BIT(3),
    DisableShadow = BIT(4)
};
```

### 4.2 设计差异与借鉴

| 维度 | Hazel 的做法 | 建议 Luck3D 的做法 | 原因 |
|------|-----------|--------------------|------|
| **渲染状态存放位置** | `PipelineSpecification`（Pipeline 级别） | `RenderState`（Material 级别） | Luck3D 基于 OpenGL，渲染状态是全局的，放在 Material 上更自然，也更接近 Unity 的使用体验 |
| **MaterialFlag** | 位标志（`DepthTest`, `Blend`, `TwoSided`） | 独立枚举字段 | 更清晰，扩展性更好，不需要位运算 |
| **BlendMode** | 放在 `FramebufferSpecification` 中 | 放在 Material 的 `RenderState` 中 | 更直观，用户在材质面板上直接编辑 |
| **CullMode** | `BackfaceCulling` 布尔值 | `CullMode` 三值枚举（Back/Front/Off） | 更灵活，支持 Front Cull（描边 Pass 需要） |
| **DepthCompare** | `DepthCompareOperator` 枚举 | `DepthCompareFunc` 枚举 | 命名更接近 Unity/OpenGL 习惯 |

---

## 五、详细设计方案

### 5.1 渲染状态枚举定义

在 `Lucky/Source/Lucky/Renderer/` 下新建 `RenderState.h`：

```cpp
#pragma once

#include <cstdint>

namespace Lucky
{
    /// <summary>
    /// 面剔除模式
    /// </summary>
    enum class CullMode : uint8_t
    {
        Back = 0,   // 剔除背面（默认，适用于大多数不透明物体）
        Front,      // 剔除正面（特殊效果，如描边 Pass 的膨胀法）
        Off         // 不剔除（双面渲染，适用于树叶、布料、纸片等薄物体）
    };

    /// <summary>
    /// 深度测试比较函数
    /// </summary>
    enum class DepthCompareFunc : uint8_t
    {
        Less = 0,       // <（OpenGL 默认）
        LessEqual,      // <=（Unity 默认）
        Greater,        // >（被遮挡时才绘制，用于轮廓效果）
        GreaterEqual,   // >=
        Equal,          // ==（贴花 Decal）
        NotEqual,       // !=
        Always,         // 总是通过（忽略深度，UI 叠加）
        Never           // 永不通过（调试用）
    };

    /// <summary>
    /// 混合模式
    /// </summary>
    enum class BlendMode : uint8_t
    {
        None = 0,                       // 不混合（不透明物体）
        SrcAlpha_OneMinusSrcAlpha,      // 标准 Alpha 混合（半透明，最常用）
        One_One,                        // 叠加混合（粒子、发光效果）
        SrcAlpha_One                    // 预乘 Alpha 叠加
    };

    /// <summary>
    /// 渲染模式预设（类似 Unity Standard Shader 的 RenderingMode）
    /// 选择后会自动设置 BlendMode、DepthWrite、RenderQueue 等状态
    /// </summary>
    enum class RenderingMode : uint8_t
    {
        Opaque = 0,     // 不透明（默认）
        Cutout,         // Alpha 裁剪（树叶、铁丝网）
        Fade,           // 整体淡入淡出（全息投影、幽灵）
        Transparent     // 半透明（玻璃、水面）
    };

    /// <summary>
    /// 渲染队列预定义值（与 Unity 一致）
    /// </summary>
    namespace RenderQueue
    {
        constexpr int32_t Background  = 1000;   // 天空盒等最先绘制的物体
        constexpr int32_t Geometry    = 2000;   // 默认不透明物体
        constexpr int32_t AlphaTest   = 2450;   // Alpha 测试物体
        constexpr int32_t Transparent = 3000;   // 透明物体
        constexpr int32_t Overlay     = 4000;   // UI 叠加层等最后绘制的物体
        
        /// <summary>
        /// 判断是否为透明区域（Queue >= 2500）
        /// </summary>
        constexpr bool IsTransparent(int32_t queue) { return queue >= 2500; }
    }
}
```

### 5.2 RenderState 结构体

在 `RenderState.h` 中定义 Per-Material 渲染状态结构体：

```cpp
/// <summary>
/// 材质渲染状态（Per-Material）
/// 每个材质持有一个 RenderState，控制该材质的 GPU 渲染状态
/// </summary>
struct RenderState
{
    CullMode Cull = CullMode::Back;                             // 面剔除模式
    bool DepthWrite = true;                                     // 深度写入
    DepthCompareFunc DepthTest = DepthCompareFunc::Less;        // 深度测试比较函数
    BlendMode Blend = BlendMode::None;                          // 混合模式
    int32_t Queue = RenderQueue::Geometry;                      // 渲染队列值
    
    /// <summary>
    /// 判断是否为透明物体
    /// </summary>
    bool IsTransparent() const { return RenderQueue::IsTransparent(Queue); }
    
    /// <summary>
    /// 比较两个 RenderState 是否相同（用于状态跟踪，避免重复设置）
    /// </summary>
    bool operator==(const RenderState& other) const
    {
        return Cull == other.Cull
            && DepthWrite == other.DepthWrite
            && DepthTest == other.DepthTest
            && Blend == other.Blend
            && Queue == other.Queue;
    }
    
    bool operator!=(const RenderState& other) const { return !(*this == other); }
};
```

### 5.3 RenderingMode 预设

提供一个辅助函数，根据 RenderingMode 自动设置 RenderState 的各个字段：

```cpp
/// <summary>
/// 根据 RenderingMode 预设自动配置 RenderState
/// </summary>
/// <param name="state">要配置的 RenderState</param>
/// <param name="mode">渲染模式预设</param>
inline void ApplyRenderingMode(RenderState& state, RenderingMode mode)
{
    switch (mode)
    {
        case RenderingMode::Opaque:
            state.Blend = BlendMode::None;
            state.DepthWrite = true;
            state.Queue = RenderQueue::Geometry;
            break;
            
        case RenderingMode::Cutout:
            state.Blend = BlendMode::None;
            state.DepthWrite = true;
            state.Queue = RenderQueue::AlphaTest;
            break;
            
        case RenderingMode::Fade:
            state.Blend = BlendMode::SrcAlpha_OneMinusSrcAlpha;
            state.DepthWrite = false;
            state.Queue = RenderQueue::Transparent;
            break;
            
        case RenderingMode::Transparent:
            state.Blend = BlendMode::SrcAlpha_OneMinusSrcAlpha;
            state.DepthWrite = false;
            state.Queue = RenderQueue::Transparent;
            break;
    }
    // 注意：CullMode 不受 RenderingMode 影响，需要单独设置
}
```

### 5.4 Material 集成 RenderState

在 `Material` 类中添加 `RenderState` 成员：

```cpp
// Material.h 中新增

#include "RenderState.h"

class Material
{
public:
    // ... 现有接口不变 ...
    
    // ---- 渲染状态 ----
    
    /// <summary>
    /// 获取渲染状态（可修改）
    /// </summary>
    RenderState& GetRenderState() { return m_RenderState; }
    
    /// <summary>
    /// 获取渲染状态（只读）
    /// </summary>
    const RenderState& GetRenderState() const { return m_RenderState; }
    
    /// <summary>
    /// 设置渲染模式预设（自动配置 Blend/ZWrite/Queue）
    /// </summary>
    void SetRenderingMode(RenderingMode mode)
    {
        m_RenderingMode = mode;
        ApplyRenderingMode(m_RenderState, mode);
    }
    
    /// <summary>
    /// 获取当前渲染模式预设
    /// </summary>
    RenderingMode GetRenderingMode() const { return m_RenderingMode; }
    
    /// <summary>
    /// 判断是否为透明物体
    /// </summary>
    bool IsTransparent() const { return m_RenderState.IsTransparent(); }

private:
    // ... 现有成员不变 ...
    
    RenderState m_RenderState;                              // 渲染状态
    RenderingMode m_RenderingMode = RenderingMode::Opaque;  // 渲染模式预设
};
```

### 5.5 RenderCommand 扩展

在 `RenderCommand` 中添加渲染状态控制方法：

```cpp
// RenderCommand.h 中新增

#include "RenderState.h"

class RenderCommand
{
public:
    // ... 现有方法不变 ...
    
    // ---- 渲染状态控制（新增） ----
    
    /// <summary>
    /// 设置面剔除模式
    /// </summary>
    static void SetCullMode(CullMode mode);
    
    /// <summary>
    /// 设置深度写入开关
    /// </summary>
    static void SetDepthWrite(bool enable);
    
    /// <summary>
    /// 设置深度测试比较函数
    /// </summary>
    static void SetDepthFunc(DepthCompareFunc func);
    
    /// <summary>
    /// 设置混合模式
    /// </summary>
    static void SetBlendMode(BlendMode mode);
};
```

```cpp
// RenderCommand.cpp 中新增实现

void RenderCommand::SetCullMode(CullMode mode)
{
    if (mode == CullMode::Off)
    {
        glDisable(GL_CULL_FACE);
    }
    else
    {
        glEnable(GL_CULL_FACE);
        glCullFace(mode == CullMode::Back ? GL_BACK : GL_FRONT);
    }
}

void RenderCommand::SetDepthWrite(bool enable)
{
    glDepthMask(enable ? GL_TRUE : GL_FALSE);
}

void RenderCommand::SetDepthFunc(DepthCompareFunc func)
{
    GLenum glFunc;
    switch (func)
    {
        case DepthCompareFunc::Less:         glFunc = GL_LESS;     break;
        case DepthCompareFunc::LessEqual:    glFunc = GL_LEQUAL;   break;
        case DepthCompareFunc::Greater:      glFunc = GL_GREATER;  break;
        case DepthCompareFunc::GreaterEqual: glFunc = GL_GEQUAL;   break;
        case DepthCompareFunc::Equal:        glFunc = GL_EQUAL;    break;
        case DepthCompareFunc::NotEqual:     glFunc = GL_NOTEQUAL; break;
        case DepthCompareFunc::Always:       glFunc = GL_ALWAYS;   break;
        case DepthCompareFunc::Never:        glFunc = GL_NEVER;    break;
        default:                             glFunc = GL_LESS;     break;
    }
    glDepthFunc(glFunc);
}

void RenderCommand::SetBlendMode(BlendMode mode)
{
    if (mode == BlendMode::None)
    {
        glDisable(GL_BLEND);
    }
    else
    {
        glEnable(GL_BLEND);
        switch (mode)
        {
            case BlendMode::SrcAlpha_OneMinusSrcAlpha:
                glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
                break;
            case BlendMode::One_One:
                glBlendFunc(GL_ONE, GL_ONE);
                break;
            case BlendMode::SrcAlpha_One:
                glBlendFunc(GL_SRC_ALPHA, GL_ONE);
                break;
            default:
                glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
                break;
        }
    }
}
```

同时修改 `RenderCommand::Init()` 中的初始化逻辑：

```cpp
void RenderCommand::Init()
{
    // 默认不开启混合（不透明物体不需要混合）
    glDisable(GL_BLEND);
    
    // 开启深度测试
    glEnable(GL_DEPTH_TEST);
    glDepthFunc(GL_LESS);
    
    // 开启背面剔除（默认剔除背面）
    glEnable(GL_CULL_FACE);
    glCullFace(GL_BACK);
    
    // 平滑直线
    glEnable(GL_LINE_SMOOTH);
}
```

### 5.6 绘制流程中应用 RenderState

在 `Renderer3D::EndScene()` 的绘制循环中，绘制每个物体前应用其材质的渲染状态。使用**状态跟踪**避免重复设置：

```cpp
// Renderer3D.cpp - EndScene() 中的绘制循环

// 状态跟踪（避免重复设置 GPU 状态）
static RenderState s_LastRenderState;   // 上一次应用的渲染状态
uint32_t lastShaderID = 0;

for (const DrawCommand& cmd : s_Data.OpaqueCommands)
{
    const RenderState& state = cmd.MaterialData->GetRenderState();
    
    // ---- 应用渲染状态（仅在变化时设置） ----
    
    if (state.Cull != s_LastRenderState.Cull)
    {
        RenderCommand::SetCullMode(state.Cull);
    }
    
    if (state.DepthWrite != s_LastRenderState.DepthWrite)
    {
        RenderCommand::SetDepthWrite(state.DepthWrite);
    }
    
    if (state.DepthTest != s_LastRenderState.DepthTest)
    {
        RenderCommand::SetDepthFunc(state.DepthTest);
    }
    
    if (state.Blend != s_LastRenderState.Blend)
    {
        RenderCommand::SetBlendMode(state.Blend);
    }
    
    s_LastRenderState = state;
    
    // ---- 绑定 Shader（仅在变化时） ----
    
    uint32_t currentShaderID = cmd.MaterialData->GetShader()->GetRendererID();
    if (currentShaderID != lastShaderID)
    {
        cmd.MaterialData->GetShader()->Bind();
        lastShaderID = currentShaderID;
    }
    
    // ---- 设置 per-object uniform + 应用材质 + 绘制 ----
    
    cmd.MaterialData->GetShader()->SetMat4("u_ObjectToWorldMatrix", cmd.Transform);
    cmd.MaterialData->Apply();
    RenderCommand::DrawIndexedRange(cmd.MeshData->GetVertexArray(),
                                     cmd.SubMeshPtr->IndexOffset,
                                     cmd.SubMeshPtr->IndexCount);
}

// 绘制结束后恢复默认渲染状态
RenderCommand::SetCullMode(CullMode::Back);
RenderCommand::SetDepthWrite(true);
RenderCommand::SetDepthFunc(DepthCompareFunc::Less);
RenderCommand::SetBlendMode(BlendMode::None);
s_LastRenderState = RenderState{};  // 重置跟踪状态
```

### 5.7 Inspector 面板 UI

在材质 Inspector 面板中添加渲染状态编辑控件：

```cpp
// InspectorPanel.cpp 中材质编辑器部分

// ---- 渲染状态区域 ----
if (ImGui::CollapsingHeader("Render State", ImGuiTreeNodeFlags_DefaultOpen))
{
    RenderState& state = material->GetRenderState();
    
    // RenderingMode 下拉框
    const char* renderingModes[] = { "Opaque", "Cutout", "Fade", "Transparent" };
    int currentMode = static_cast<int>(material->GetRenderingMode());
    if (ImGui::Combo("Rendering Mode", &currentMode, renderingModes, IM_ARRAYSIZE(renderingModes)))
    {
        material->SetRenderingMode(static_cast<RenderingMode>(currentMode));
    }
    
    ImGui::Separator();
    
    // CullMode 下拉框
    const char* cullModes[] = { "Back", "Front", "Off (Two Sided)" };
    int currentCull = static_cast<int>(state.Cull);
    if (ImGui::Combo("Cull Mode", &currentCull, cullModes, IM_ARRAYSIZE(cullModes)))
    {
        state.Cull = static_cast<CullMode>(currentCull);
    }
    
    // ZWrite 复选框
    ImGui::Checkbox("Depth Write", &state.DepthWrite);
    
    // ZTest 下拉框
    const char* depthFuncs[] = { "Less", "LessEqual", "Greater", "GreaterEqual", 
                                  "Equal", "NotEqual", "Always", "Never" };
    int currentDepth = static_cast<int>(state.DepthTest);
    if (ImGui::Combo("Depth Test", &currentDepth, depthFuncs, IM_ARRAYSIZE(depthFuncs)))
    {
        state.DepthTest = static_cast<DepthCompareFunc>(currentDepth);
    }
    
    // BlendMode 下拉框
    const char* blendModes[] = { "None (Opaque)", "Alpha Blend", "Additive", "Soft Additive" };
    int currentBlend = static_cast<int>(state.Blend);
    if (ImGui::Combo("Blend Mode", &currentBlend, blendModes, IM_ARRAYSIZE(blendModes)))
    {
        state.Blend = static_cast<BlendMode>(currentBlend);
    }
    
    // RenderQueue 拖拽条
    ImGui::DragInt("Render Queue", &state.Queue, 1, 0, 5000);
    
    // 快捷预设按钮
    if (ImGui::Button("Background"))  state.Queue = RenderQueue::Background;
    ImGui::SameLine();
    if (ImGui::Button("Geometry"))    state.Queue = RenderQueue::Geometry;
    ImGui::SameLine();
    if (ImGui::Button("AlphaTest"))   state.Queue = RenderQueue::AlphaTest;
    ImGui::SameLine();
    if (ImGui::Button("Transparent")) state.Queue = RenderQueue::Transparent;
    ImGui::SameLine();
    if (ImGui::Button("Overlay"))     state.Queue = RenderQueue::Overlay;
}
```

**Inspector 面板效果预览**：

```
┌─────────────────────────────────────────────┐
│ ▼ Render State                              │
│                                             │
│   Rendering Mode  [Opaque        ▼]        │
│   ─────────────────────────────────         │
│   Cull Mode       [Back          ▼]        │
│   Depth Write     [?]                      │
│   Depth Test      [Less          ▼]        │
│   Blend Mode      [None (Opaque) ▼]        │
│   Render Queue    [====2000========]        │
│   [Background] [Geometry] [AlphaTest]       │
│   [Transparent] [Overlay]                   │
│                                             │
│ ▼ Material Properties                       │
│   _BaseColor      [■ 1.0, 1.0, 1.0, 1.0]  │
│   _Metallic       [====0.5========]         │
│   _Roughness      [====0.5========]         │
│   ...                                       │
└─────────────────────────────────────────────┘
```

### 5.8 序列化支持

在场景序列化中保存和加载材质的渲染状态：

```yaml
# .luck3d 场景文件中的材质序列化格式
Materials:
  - Name: "Glass"
    Shader: "Standard"
    RenderState:
      RenderingMode: 3        # Transparent
      CullMode: 0             # Back
      DepthWrite: false
      DepthTest: 0            # Less
      BlendMode: 1            # SrcAlpha_OneMinusSrcAlpha
      RenderQueue: 3000       # Transparent
    Properties:
      _BaseColor: [1.0, 1.0, 1.0, 0.5]
      _Metallic: 0.9
      _Roughness: 0.1
```

```cpp
// SceneSerializer.cpp 中新增

// 序列化
void SerializeRenderState(YAML::Emitter& out, const RenderState& state, RenderingMode mode)
{
    out << YAML::Key << "RenderState" << YAML::BeginMap;
    out << YAML::Key << "RenderingMode" << YAML::Value << static_cast<int>(mode);
    out << YAML::Key << "CullMode" << YAML::Value << static_cast<int>(state.Cull);
    out << YAML::Key << "DepthWrite" << YAML::Value << state.DepthWrite;
    out << YAML::Key << "DepthTest" << YAML::Value << static_cast<int>(state.DepthTest);
    out << YAML::Key << "BlendMode" << YAML::Value << static_cast<int>(state.Blend);
    out << YAML::Key << "RenderQueue" << YAML::Value << state.Queue;
    out << YAML::EndMap;
}

// 反序列化
void DeserializeRenderState(const YAML::Node& node, Material& material)
{
    if (auto rsNode = node["RenderState"])
    {
        if (rsNode["RenderingMode"])
        {
            material.SetRenderingMode(static_cast<RenderingMode>(rsNode["RenderingMode"].as<int>()));
        }
        
        RenderState& state = material.GetRenderState();
        
        if (rsNode["CullMode"])
            state.Cull = static_cast<CullMode>(rsNode["CullMode"].as<int>());
        if (rsNode["DepthWrite"])
            state.DepthWrite = rsNode["DepthWrite"].as<bool>();
        if (rsNode["DepthTest"])
            state.DepthTest = static_cast<DepthCompareFunc>(rsNode["DepthTest"].as<int>());
        if (rsNode["BlendMode"])
            state.Blend = static_cast<BlendMode>(rsNode["BlendMode"].as<int>());
        if (rsNode["RenderQueue"])
            state.Queue = rsNode["RenderQueue"].as<int>();
    }
}
```

---

## 六、与其他模块的关联

### 6.1 与 R9 DrawCommand 排序的关联

R13 的 `RenderState.Queue`（RenderQueue）字段与 R9 文档第七章的渲染队列演进方案**完全一致**：

```
R9 阶段一：Material 添加 RenderQueue 字段
  → 对应 R13 的 RenderState.Queue 字段

R9 阶段二：SortKey 最高位改为 RenderQueue
  → R13 的 RenderState.Queue 值编码到 SortKey 最高 16 位

R9 阶段三：EndScene 分区绘制
  → R13 的 RenderState 中的 DepthWrite/BlendMode 在进入透明区域时自动切换

R9 阶段四：Inspector UI 暴露 RenderQueue
  → 对应 R13 的 Inspector 面板 UI 中的 RenderQueue 编辑控件
```

**结论**：R13 的 RenderState 设计是 R9 渲染队列演进的**超集**。实施 R13 时，R9 第七章的演进方案自然被包含在内。

### 6.2 与 R12 渲染器架构演进的关联

R12 文档中描述的 SceneRenderer 渲染流程依赖 Per-Material 渲染状态：

```
SceneRenderer::EndScene()
  → OpaquePass()       // 使用 RenderState: DepthWrite=true, Blend=None
  → TransparentPass()  // 使用 RenderState: DepthWrite=false, Blend=Alpha
  → DebugPass()        // 使用 RenderState: DepthTest=Always（Gizmo 在最上层）
```

R13 的 RenderState 为 R12 的 SceneRenderer 提供了必要的基础设施。

### 6.3 与 R4 阴影系统的关联

阴影 Pass 需要特殊的渲染状态：

```
Shadow Pass 渲染状态：
  CullMode = Front    // 剔除正面，避免阴影痤疮（Shadow Acne）
  DepthWrite = true   // 写入深度到 Shadow Map
  BlendMode = None    // 不需要混合
  DepthTest = Less    // 正常深度测试
```

R13 的 `RenderCommand::SetCullMode(CullMode::Front)` 等方法为 R4 阴影 Pass 提供了必要的状态切换能力。

---

## 七、渐进式实施路线

### 7.1 Phase 0：立即可做（全局背面剔除）

> **改动量**：1 行代码  
> **预计时间**：5 分钟  
> **前置依赖**：无

在 `RenderCommand::Init()` 中添加一行代码：

```cpp
void RenderCommand::Init()
{
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glEnable(GL_DEPTH_TEST);
    glEnable(GL_LINE_SMOOTH);
    
    // 新增：开启背面剔除（约 50% 光栅化性能提升）
    glEnable(GL_CULL_FACE);
    glCullFace(GL_BACK);
}
```

**收益**：零成本获得约 50% 的光栅化性能提升。所有封闭物体（Cube、Sphere、Cylinder 等）的背面三角形不再被渲染。

### 7.2 Phase 1：Per-Material 渲染状态

> **改动量**：约 200 行新增代码  
> **预计时间**：2-3 天  
> **前置依赖**：R9 DrawCommand 排序

| 步骤 | 内容 | 涉及文件 |
|------|------|---------|
| ① | 创建 `RenderState.h`（枚举 + 结构体） | 新建文件 |
| ② | `Material` 添加 `RenderState` 成员 | `Material.h` |
| ③ | `RenderCommand` 添加状态切换方法 | `RenderCommand.h/cpp` |
| ④ | `RenderCommand::Init()` 修改默认状态 | `RenderCommand.cpp` |
| ⑤ | `EndScene` 绘制循环中应用 RenderState（带状态跟踪） | `Renderer3D.cpp` |
| ⑥ | Inspector 面板暴露 CullMode / DepthWrite / DepthTest / BlendMode | `InspectorPanel.cpp` |
| ⑦ | 材质序列化支持 RenderState | `SceneSerializer.cpp` |

### 7.3 Phase 2：RenderingMode 预设 + 透明物体支持

> **改动量**：约 50 行新增代码  
> **预计时间**：1-2 天  
> **前置依赖**：Phase 1 + R9 RenderQueue 演进

| 步骤 | 内容 | 涉及文件 |
|------|------|---------|
| ① | `ApplyRenderingMode()` 辅助函数 | `RenderState.h` |
| ② | `Material` 添加 `SetRenderingMode()` | `Material.h` |
| ③ | Inspector 面板添加 RenderingMode 下拉框 + RenderQueue 预设按钮 | `InspectorPanel.cpp` |
| ④ | `EndScene` 中不透明/透明分区绘制（自动切换 DepthWrite/Blend） | `Renderer3D.cpp` |
| ⑤ | Cutout 模式的 Alpha 裁剪支持（Shader 中添加 `discard`） | `Standard.frag` |

---

## 八、涉及的文件清单

| 文件路径 | 操作 | Phase | 说明 |
|---------|------|-------|------|
| `Lucky/Source/Lucky/Renderer/RenderState.h` | **新建** | 1 | 枚举定义 + RenderState 结构体 + RenderingMode 预设函数 |
| `Lucky/Source/Lucky/Renderer/Material.h` | 修改 | 1 | 添加 `RenderState` 成员 + `GetRenderState()` / `SetRenderingMode()` |
| `Lucky/Source/Lucky/Renderer/RenderCommand.h` | 修改 | 1 | 添加 `SetCullMode` / `SetDepthWrite` / `SetDepthFunc` / `SetBlendMode` |
| `Lucky/Source/Lucky/Renderer/RenderCommand.cpp` | 修改 | 0+1 | Phase 0: 添加 `glEnable(GL_CULL_FACE)`；Phase 1: 实现状态切换方法 + 修改 Init() |
| `Lucky/Source/Lucky/Renderer/Renderer3D.cpp` | 修改 | 1 | EndScene 绘制循环中应用 RenderState（带状态跟踪） |
| `Editor/Source/Panels/InspectorPanel.cpp` | 修改 | 1+2 | 材质 Inspector 添加渲染状态编辑控件 |
| `Lucky/Source/Lucky/Scene/SceneSerializer.cpp` | 修改 | 1 | 材质序列化支持 RenderState |

---

## 九、验证方法

### 9.1 Phase 0 验证

1. 开启背面剔除后，所有封闭物体（Cube、Sphere 等）渲染结果应与之前完全一致
2. 从内部观察物体时，应该看不到内壁（背面被剔除）
3. Plane 等单面物体从背面观察时应该不可见（这是正确行为）

### 9.2 Phase 1 验证

1. 创建两个 Cube，一个设置 `CullMode::Off`（双面渲染），一个保持默认 `CullMode::Back`
2. 从内部观察：双面渲染的 Cube 应该能看到内壁，默认的不能
3. 修改 DepthWrite 为 false，确认物体不再遮挡后面的物体
4. 修改 BlendMode 为 Alpha Blend，确认物体变为半透明
5. 保存/加载场景，确认渲染状态正确序列化和反序列化

### 9.3 Phase 2 验证

1. 选择 RenderingMode 为 Transparent，确认 Blend/ZWrite/Queue 自动设置
2. 创建不透明和透明物体混合的场景，确认渲染顺序正确
3. 透明物体应该能正确显示后面的不透明物体

---

## 十、设计决策记录

| 决策 | 选择 | 原因 |
|------|------|------|
| 渲染状态存放位置 | Material 级别（`RenderState` 结构体） | 更接近 Unity 的使用体验，用户在材质面板上直接编辑；OpenGL 的渲染状态是全局的，放在 Material 上更自然 |
| 枚举 vs 位标志 | 独立枚举字段 | 比 Hazel 的 `MaterialFlag` 位标志更清晰，扩展性更好，不需要位运算 |
| CullMode 设计 | 三值枚举（Back/Front/Off） | 比 Hazel 的 `BackfaceCulling` 布尔值更灵活，支持 Front Cull（描边 Pass 需要） |
| RenderQueue 位置 | 放在 RenderState 中 | 与 R9 文档第七章的演进方案无缝衔接，避免 Material 中出现两个独立的排序相关字段 |
| RenderingMode 实现 | 辅助函数 `ApplyRenderingMode()` | 不是 GPU 状态，只是一个便捷预设；用户选择后自动设置其他状态，但仍可手动覆盖 |
| 状态跟踪机制 | `static RenderState s_LastRenderState` | 避免每个物体都重复设置相同的 GPU 状态，减少 OpenGL 调用次数 |
| Phase 0 独立性 | 全局背面剔除可立即实施 | 零风险、零依赖、5 分钟完成、约 50% 光栅化性能提升 |
| 默认混合状态 | 改为 `glDisable(GL_BLEND)` | 不透明物体不需要混合，关闭混合可避免不必要的帧缓冲区读取 |
