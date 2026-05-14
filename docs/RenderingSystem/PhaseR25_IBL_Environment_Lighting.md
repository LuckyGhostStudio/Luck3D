# Phase R25：IBL 环境光照（Image-Based Lighting）

> **文档版本**：v1.0  
> **创建日期**：2026-05-13  
> **优先级**：?? P0（核心目标）  
> **预计工作量**：5-8 天  
> **前置依赖**：Phase R2（PBR Shader）、Phase R23（Skybox Rendering）、Phase R5（HDR + Tonemapping）  
> **文档说明**：本文档详细描述如何为 Luck3D 引擎实现 IBL（Image-Based Lighting）环境光照系统，将天空盒 Cubemap 作为全方向光源参与场景光照计算，替换当前的常量环境光 `AMBIENT_STRENGTH = 0.03`。所有代码可直接对照实现。

---

## 目录

- [一、现状分析](#一现状分析)
- [二、改进目标](#二改进目标)
- [三、涉及的文件清单](#三涉及的文件清单)
- [四、IBL 理论基础](#四ibl-理论基础)
  - [4.1 Split-Sum 近似](#41-split-sum-近似)
  - [4.2 三张预计算纹理](#42-三张预计算纹理)
  - [4.3 最终环境光公式](#43-最终环境光公式)
- [五、纹理单元分配规划](#五纹理单元分配规划)
- [六、Step 1：BRDF LUT 生成](#六step-1brdf-lut-生成)
  - [6.1 理论说明](#61-理论说明)
  - [6.2 方案选择：生成方式](#62-方案选择生成方式)
  - [6.3 BRDF LUT Shader 完整代码](#63-brdf-lut-shader-完整代码)
  - [6.4 C++ 端生成逻辑](#64-c-端生成逻辑)
- [七、Step 2：Irradiance Map 生成](#七step-2irradiance-map-生成)
  - [7.1 理论说明](#71-理论说明)
  - [7.2 方案选择：生成方式](#72-方案选择生成方式)
  - [7.3 Irradiance 卷积 Shader 完整代码](#73-irradiance-卷积-shader-完整代码)
  - [7.4 C++ 端生成逻辑](#74-c-端生成逻辑)
- [八、Step 3：Prefiltered Environment Map 生成](#八step-3prefiltered-environment-map-生成)
  - [8.1 理论说明](#81-理论说明)
  - [8.2 方案选择：生成方式](#82-方案选择生成方式)
  - [8.3 TextureCube Mipmap 扩展](#83-texturecube-mipmap-扩展)
  - [8.4 Prefilter 卷积 Shader 完整代码](#84-prefilter-卷积-shader-完整代码)
  - [8.5 C++ 端生成逻辑](#85-c-端生成逻辑)
- [九、Step 4：IBL 预计算管理器](#九step-4ibl-预计算管理器)
  - [9.1 IBLPrecompute 类设计](#91-iblprecompute-类设计)
  - [9.2 CubemapRenderHelper 工具类](#92-cubemaprenderhelper-工具类)
- [十、Step 5：Shader 集成](#十step-5shader-集成)
  - [10.1 Lighting.glsl 新增 IBL 函数](#101-lightingglsl-新增-ibl-函数)
  - [10.2 Standard.frag 修改](#102-standardfrag-修改)
- [十一、Step 6：渲染管线集成](#十一step-6渲染管线集成)
  - [11.1 RenderContext 扩展](#111-rendercontext-扩展)
  - [11.2 Renderer3D 集成](#112-renderer3d-集成)
  - [11.3 OpaquePass / TransparentPass 绑定 IBL 纹理](#113-opaquepass--transparentpass-绑定-ibl-纹理)
- [十二、Step 7：天空盒变更时重新生成](#十二step-7天空盒变更时重新生成)
- [十三、方案选择汇总](#十三方案选择汇总)
- [十四、实现顺序与依赖关系](#十四实现顺序与依赖关系)
- [十五、验证方法](#十五验证方法)
- [十六、设计决策记录](#十六设计决策记录)

---

## 一、现状分析

### 当前环境光实现

在 `Standard.frag` 第 88 行：

```glsl
// ---- 环境光（简单常量，无 IBL） ----
vec3 ambient = vec3(AMBIENT_STRENGTH) * albedo * ao;
```

其中 `AMBIENT_STRENGTH = 0.03`（定义在 `Common.glsl`），这是一个极其简陋的常量环境光。

### 当前已有的基础设施

| 模块 | 文件 | 状态 |
|------|------|------|
| PBR BRDF | `Lighting.glsl` | ? 完整的 Cook-Torrance（GGX + Fresnel-Schlick + Smith GGX） |
| TextureCube | `TextureCube.h/cpp` | ? 支持 6 面加载 + HDR Equirectangular → Cubemap 转换 |
| 天空盒渲染 | `SkyboxPass.cpp` | ? Material 驱动，采样 `samplerCube` |
| HDR 管线 | `PostProcessPass.cpp` | ? RGBA16F FBO → Tonemapping → Bloom |
| ScreenQuad | `ScreenQuad.h/cpp` | ? 全屏四边形绘制工具 |
| OpenGL 版本 | 4.5 | ? 支持 DSA、Compute Shader |
| Framebuffer | `Framebuffer.h/cpp` | ? 支持 RGBA16F、Texture2DArray、BindDepthLayer/BindColorLayer |

### 问题

| 编号 | 问题 | 影响 |
|------|------|------|
| R25-01 | 环境光为常量 0.03 | 阴影区域几乎全黑，缺乏环境光填充 |
| R25-02 | 无环境反射 | 金属物体看不到环境映射，金属度参数几乎无效果 |
| R25-03 | 天空盒与场景脱节 | 天空盒只是背景图，不参与光照计算 |
| R25-04 | 粗糙度对环境反射无影响 | 光滑金属和粗糙金属的环境反射完全相同（都没有） |

---

## 二、改进目标

1. **实现 IBL 漫反射**：使用 Irradiance Map 替代常量环境光，阴影区域有自然的环境光填充
2. **实现 IBL 镜面反射**：使用 Prefiltered Environment Map + BRDF LUT，金属材质有真实的环境反射
3. **粗糙度影响环境反射**：光滑表面反射清晰，粗糙表面反射模糊（通过 Mipmap Level 控制）
4. **天空盒参与光照**：更换天空盒后，场景光照自动变化
5. **预计算在加载时完成**：不影响运行时性能

---

## 三、涉及的文件清单

| 文件路径 | 操作 | 说明 |
|---------|------|------|
| `Lucky/Source/Lucky/Renderer/IBLPrecompute.h` | **新建** | IBL 预计算管理器头文件 |
| `Lucky/Source/Lucky/Renderer/IBLPrecompute.cpp` | **新建** | IBL 预计算管理器实现 |
| `Lucky/Source/Lucky/Renderer/TextureCube.h` | 修改 | 新增带 Mipmap 的构造方式 |
| `Lucky/Source/Lucky/Renderer/TextureCube.cpp` | 修改 | 实现 Mipmap 分配 |
| `Lucky/Source/Lucky/Renderer/RenderContext.h` | 修改 | 新增 IBL 纹理 ID 字段 |
| `Lucky/Source/Lucky/Renderer/Renderer3D.h` | 修改 | 新增 IBL 相关接口 |
| `Lucky/Source/Lucky/Renderer/Renderer3D.cpp` | 修改 | 集成 IBL 预计算和纹理传递 |
| `Lucky/Source/Lucky/Renderer/Passes/OpaquePass.cpp` | 修改 | 绑定 IBL 纹理到 Shader |
| `Lucky/Source/Lucky/Renderer/Passes/TransparentPass.cpp` | 修改 | 绑定 IBL 纹理到 Shader |
| `Lucky/Source/Lucky/Renderer/Material.cpp` | 修改 | 新增 IBL 相关内部 uniform 到黑名单 |
| `Luck3DApp/Assets/Shaders/Lucky/Lighting.glsl` | 修改 | 新增 IBL 环境光函数 |
| `Luck3DApp/Assets/Shaders/Lucky/Common.glsl` | 修改 | 新增 IBL 开关 uniform 声明 |
| `Luck3DApp/Assets/Shaders/Standard.frag` | 修改 | 替换常量环境光为 IBL 环境光 |
| `Luck3DApp/Assets/Shaders/Internal/IBL/BRDFIntegration.vert` | **新建** | BRDF LUT 生成顶点着色器 |
| `Luck3DApp/Assets/Shaders/Internal/IBL/BRDFIntegration.frag` | **新建** | BRDF LUT 生成片段着色器 |
| `Luck3DApp/Assets/Shaders/Internal/IBL/IrradianceConvolution.vert` | **新建** | Irradiance 卷积顶点着色器 |
| `Luck3DApp/Assets/Shaders/Internal/IBL/IrradianceConvolution.frag` | **新建** | Irradiance 卷积片段着色器 |
| `Luck3DApp/Assets/Shaders/Internal/IBL/PrefilterConvolution.vert` | **新建** | Prefilter 卷积顶点着色器 |
| `Luck3DApp/Assets/Shaders/Internal/IBL/PrefilterConvolution.frag` | **新建** | Prefilter 卷积片段着色器 |

---

## 四、IBL 理论基础

### 4.1 Split-Sum 近似

IBL 的核心挑战是求解渲染方程中的环境光积分：

```
Lo(p, ωo) = ∫ fr(p, ωi, ωo) · Li(ωi) · (ωi · n) dωi
```

直接积分需要对半球上所有方向采样，实时不可行。Epic Games 在 2013 年提出 **Split-Sum 近似**，将积分拆分为两部分：

```
Lo ≈ (∫ Li(ωi) dωi) × (∫ fr(ωi, ωo) · (ωi · n) dωi)
     ├─────────────┘   ├──────────────────────────────┘
     Prefiltered Map    BRDF Integration (LUT)
```

再加上漫反射部分的 Irradiance Map，总共需要 **3 张预计算纹理**。

### 4.2 三张预计算纹理

```
┌─────────────────────────────────────────────────────────────────┐
│                    HDR Environment Cubemap                       │
│                    （天空盒原始 Cubemap）                          │
└──────────┬──────────────────┬──────────────────┬────────────────┘
           │                  │                  │
           ▼                  ▼                  ▼
┌──────────────────┐ ┌──────────────────┐ ┌──────────────────┐
│  Irradiance Map  │ │ Prefiltered Env  │ │    BRDF LUT      │
│  辐照度图         │ │ 预过滤环境图      │ │  BRDF 查找表      │
│                  │ │                  │ │                  │
│  格式: RGB16F    │ │  格式: RGB16F    │ │  格式: RG16F     │
│  类型: Cubemap   │ │  类型: Cubemap   │ │  类型: Texture2D │
│  分辨率: 32×32   │ │  分辨率: 128×128 │ │  分辨率: 512×512 │
│  Mipmap: 1 级    │ │  Mipmap: 5 级    │ │  Mipmap: 1 级    │
│                  │ │                  │ │                  │
│  用途: 漫反射IBL │ │  用途: 镜面反射IBL│ │  用途: Fresnel   │
│                  │ │  (不同粗糙度对应  │ │  积分查找表       │
│  依赖: 环境图    │ │   不同 mip level) │ │                  │
│  需重新生成: 是  │ │  需重新生成: 是   │ │  依赖: 无        │
└──────────────────┘ └──────────────────┘ │  需重新生成: 否   │
                                          │  (全局只需一张)   │
                                          └──────────────────┘
```

| 纹理 | 格式 | 类型 | 分辨率 | Mipmap | 依赖环境图 | 说明 |
|------|------|------|--------|--------|-----------|------|
| **BRDF LUT** | `GL_RG16F` | Texture2D | 512×512 | 1 级 | ? | 纯数学计算，全局只需一张，引擎初始化时生成 |
| **Irradiance Map** | `GL_RGB16F` | Cubemap | 32×32 per face | 1 级 | ? | 对环境图做半球余弦加权卷积，用于漫反射 IBL |
| **Prefiltered Env Map** | `GL_RGB16F` | Cubemap | 128×128 per face | 5 级 | ? | 对环境图做不同粗糙度的 GGX 重要性采样卷积 |

### 4.3 最终环境光公式

```glsl
// ---- IBL 漫反射 ----
vec3 irradiance = texture(u_IrradianceMap, N).rgb;
vec3 diffuseIBL = kD * irradiance * albedo;

// ---- IBL 镜面反射 ----
vec3 R = reflect(-V, N);
float lod = roughness * (maxMipLevel - 1);  // 粗糙度映射到 mip level
vec3 prefilteredColor = textureLod(u_PrefilterMap, R, lod).rgb;
vec2 brdfLUT = texture(u_BRDFLUT, vec2(max(dot(N, V), 0.0), roughness)).rg;
vec3 specularIBL = prefilteredColor * (F * brdfLUT.x + brdfLUT.y);

// ---- 最终环境光 ----
vec3 ambient = (diffuseIBL + specularIBL) * ao;
```

---

## 五、纹理单元分配规划

### 当前已占用的纹理单元

| 纹理单元 | 用途 | 设置位置 |
|----------|------|----------|
| 0~6 | 材质纹理（AlbedoMap, NormalMap, MetallicMap, RoughnessMap, AOMap, EmissionMap, SkyboxMap） | `Material::Apply()` 自动分配 |
| 8 | Translucent Shadow Map | `OpaquePass` / `TransparentPass` |
| 15 | CSM Shadow Map Array | `OpaquePass` / `TransparentPass` |

### IBL 纹理单元分配

| 纹理单元 | 用途 | 说明 |
|----------|------|------|
| **10** | `u_IrradianceMap` | Irradiance Cubemap（漫反射 IBL） |
| **11** | `u_PrefilterMap` | Prefiltered Environment Cubemap（镜面反射 IBL） |
| **12** | `u_BRDFLUT` | BRDF LUT Texture2D（Fresnel 积分查找表） |

> **选择 10-12 的原因**：
> - 0~6 被材质纹理占用（`Material::Apply()` 从 slot 0 开始自动递增分配）
> - 8 被 Translucent Shadow Map 占用
> - 15 被 CSM Shadow Map Array 占用
> - 10~12 处于中间空闲区域，不与任何现有纹理冲突

---

## 六、Step 1：BRDF LUT 生成

### 6.1 理论说明

BRDF LUT（Look-Up Table）预计算了 Cook-Torrance BRDF 中 Fresnel 项的积分结果。它是一张 2D 纹理：

- **U 轴**：`NdotV`（法线与视线的点积，范围 [0, 1]）
- **V 轴**：`roughness`（粗糙度，范围 [0, 1]）
- **输出**：`vec2(scale, bias)`，用于重建 Fresnel 积分：`F0 * scale + bias`

这张纹理**与环境图无关**，是纯数学计算，全局只需生成一次。

### 6.2 方案选择：生成方式

| 方案 | 说明 | 优点 | 缺点 | 推荐 |
|------|------|------|------|------|
| **方案 A：GPU ScreenQuad 渲染** | 使用全屏四边形 + Fragment Shader 生成 | 利用现有 `ScreenQuad` 工具，与项目架构一致；GPU 并行计算速度快 | 需要临时 FBO | ? **最优** |
| 方案 B：GPU Compute Shader | 使用 Compute Shader 直接写入纹理 | 不需要 FBO；概念上更"正确" | 需要新增 Compute Shader 基础设施（项目当前无 Compute Shader 支持）；增加架构复杂度 | 其次 |
| 方案 C：CPU 预计算 | 在 CPU 端用循环计算 | 最简单，无需 Shader | 速度慢（512×512 × 1024 次采样）；阻塞主线程 | 不推荐 |

**推荐方案 A**：GPU ScreenQuad 渲染。项目已有 `ScreenQuad` 工具和 FBO 基础设施，实现成本最低。

### 6.3 BRDF LUT Shader 完整代码

#### 顶点着色器：`Assets/Shaders/Internal/IBL/BRDFIntegration.vert`

```glsl
#version 450 core

layout(location = 0) in vec2 a_Position;    // 全屏四边形顶点位置 [-1, 1]
layout(location = 1) in vec2 a_TexCoord;    // 纹理坐标 [0, 1]

out vec2 v_TexCoord;

void main()
{
    v_TexCoord = a_TexCoord;
    gl_Position = vec4(a_Position, 0.0, 1.0);
}
```

#### 片段着色器：`Assets/Shaders/Internal/IBL/BRDFIntegration.frag`

```glsl
#version 450 core

in vec2 v_TexCoord;

layout(location = 0) out vec2 o_Color;  // RG16F 输出

const float PI = 3.14159265359;

// ---- Hammersley 序列（低差异序列，用于准蒙特卡洛积分） ----

// Van der Corput 序列（位反转）
float RadicalInverse_VdC(uint bits)
{
    bits = (bits << 16u) | (bits >> 16u);
    bits = ((bits & 0x55555555u) << 1u) | ((bits & 0xAAAAAAAAu) >> 1u);
    bits = ((bits & 0x33333333u) << 2u) | ((bits & 0xCCCCCCCCu) >> 2u);
    bits = ((bits & 0x0F0F0F0Fu) << 4u) | ((bits & 0xF0F0F0F0u) >> 4u);
    bits = ((bits & 0x00FF00FFu) << 8u) | ((bits & 0xFF00FF00u) >> 8u);
    return float(bits) * 2.3283064365386963e-10;  // 0x100000000
}

vec2 Hammersley(uint i, uint N)
{
    return vec2(float(i) / float(N), RadicalInverse_VdC(i));
}

// ---- GGX 重要性采样 ----
// 根据 GGX NDF 分布生成采样方向（切线空间）
vec3 ImportanceSampleGGX(vec2 Xi, vec3 N, float roughness)
{
    float a = roughness * roughness;

    // 球面坐标
    float phi = 2.0 * PI * Xi.x;
    float cosTheta = sqrt((1.0 - Xi.y) / (1.0 + (a * a - 1.0) * Xi.y));
    float sinTheta = sqrt(1.0 - cosTheta * cosTheta);

    // 切线空间半向量
    vec3 H;
    H.x = cos(phi) * sinTheta;
    H.y = sin(phi) * sinTheta;
    H.z = cosTheta;

    // 切线空间 → 世界空间
    vec3 up = abs(N.z) < 0.999 ? vec3(0.0, 0.0, 1.0) : vec3(1.0, 0.0, 0.0);
    vec3 tangent = normalize(cross(up, N));
    vec3 bitangent = cross(N, tangent);

    vec3 sampleVec = tangent * H.x + bitangent * H.y + N * H.z;
    return normalize(sampleVec);
}

// ---- Smith's Schlick-GGX 几何遮蔽（IBL 版本，k = α2/2） ----
float GeometrySchlickGGX_IBL(float NdotV, float roughness)
{
    float a = roughness;
    float k = (a * a) / 2.0;  // IBL 使用 k = α2/2（与直接光照的 k = (r+1)2/8 不同）

    return NdotV / (NdotV * (1.0 - k) + k);
}

float GeometrySmith_IBL(vec3 N, vec3 V, vec3 L, float roughness)
{
    float NdotV = max(dot(N, V), 0.0);
    float NdotL = max(dot(N, L), 0.0);
    float ggx1 = GeometrySchlickGGX_IBL(NdotV, roughness);
    float ggx2 = GeometrySchlickGGX_IBL(NdotL, roughness);
    return ggx1 * ggx2;
}

// ---- BRDF 积分 ----
vec2 IntegrateBRDF(float NdotV, float roughness)
{
    // 假设 N = (0, 0, 1)，V 在 XZ 平面
    vec3 V;
    V.x = sqrt(1.0 - NdotV * NdotV);  // sin(theta)
    V.y = 0.0;
    V.z = NdotV;                        // cos(theta)

    float A = 0.0;  // F0 的缩放因子
    float B = 0.0;  // F0 的偏移因子

    vec3 N = vec3(0.0, 0.0, 1.0);

    const uint SAMPLE_COUNT = 1024u;
    for (uint i = 0u; i < SAMPLE_COUNT; ++i)
    {
        vec2 Xi = Hammersley(i, SAMPLE_COUNT);
        vec3 H = ImportanceSampleGGX(Xi, N, roughness);
        vec3 L = normalize(2.0 * dot(V, H) * H - V);

        float NdotL = max(L.z, 0.0);
        float NdotH = max(H.z, 0.0);
        float VdotH = max(dot(V, H), 0.0);

        if (NdotL > 0.0)
        {
            float G = GeometrySmith_IBL(N, V, L, roughness);
            float G_Vis = (G * VdotH) / (NdotH * NdotV);
            float Fc = pow(1.0 - VdotH, 5.0);

            A += (1.0 - Fc) * G_Vis;
            B += Fc * G_Vis;
        }
    }

    A /= float(SAMPLE_COUNT);
    B /= float(SAMPLE_COUNT);

    return vec2(A, B);
}

void main()
{
    // v_TexCoord.x = NdotV, v_TexCoord.y = roughness
    o_Color = IntegrateBRDF(v_TexCoord.x, v_TexCoord.y);
}
```

### 6.4 C++ 端生成逻辑

在 `IBLPrecompute` 类中实现：

```cpp
/// <summary>
/// 生成 BRDF LUT（512×512，RG16F）
/// 使用 ScreenQuad 渲染到临时 FBO
/// 全局只需生成一次，在 Renderer3D::Init() 中调用
/// </summary>
/// <returns>BRDF LUT 纹理 ID（GL_TEXTURE_2D）</returns>
uint32_t IBLPrecompute::GenerateBRDFLUT()
{
    const uint32_t resolution = 512;
    
    // 1. 创建 RG16F 纹理
    uint32_t brdfLUT;
    glCreateTextures(GL_TEXTURE_2D, 1, &brdfLUT);
    glTextureStorage2D(brdfLUT, 1, GL_RG16F, resolution, resolution);
    glTextureParameteri(brdfLUT, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTextureParameteri(brdfLUT, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTextureParameteri(brdfLUT, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTextureParameteri(brdfLUT, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    
    // 2. 创建临时 FBO
    uint32_t fbo;
    glCreateFramebuffers(1, &fbo);
    glNamedFramebufferTexture(fbo, GL_COLOR_ATTACHMENT0, brdfLUT, 0);
    
    // 3. 渲染
    glBindFramebuffer(GL_FRAMEBUFFER, fbo);
    glViewport(0, 0, resolution, resolution);
    glClear(GL_COLOR_BUFFER_BIT);
    
    m_BRDFShader->Bind();
    ScreenQuad::Draw();
    
    // 4. 清理临时 FBO
    glDeleteFramebuffers(1, &fbo);
    
    // 5. 恢复之前的 FBO 和 Viewport（由调用方负责）
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    
    return brdfLUT;
}
```

---

## 七、Step 2：Irradiance Map 生成

### 7.1 理论说明

Irradiance Map（辐照度图）预计算了环境光在每个法线方向上的**漫反射积分**：

```
E(n) = ∫ Li(ωi) · max(ωi · n, 0) dωi
```

这是对整个半球的余弦加权积分。由于漫反射是低频信息，32×32 的分辨率就足够了。

### 7.2 方案选择：生成方式

| 方案 | 说明 | 优点 | 缺点 | 推荐 |
|------|------|------|------|------|
| **方案 A：GPU 渲染到 Cubemap 6 面** | 对每个面渲染一个全屏四边形，Fragment Shader 对半球采样 | 与项目架构一致；利用 GPU 并行；使用现有的 Cube 几何体 | 需要 6 次 DrawCall；需要手动绑定 Cubemap face 到 FBO | ? **最优** |
| 方案 B：GPU Compute Shader | 使用 Compute Shader 直接写入 Cubemap | 单次 Dispatch；概念更清晰 | 项目无 Compute Shader 基础设施 | 其次 |
| 方案 C：CPU 预计算 | CPU 端对每个 texel 做半球积分 | 最简单 | 极慢（32×32×6 面 × 数千次采样） | 不推荐 |

**推荐方案 A**：GPU 渲染到 Cubemap 6 面。使用 SkyboxPass 中已有的 Cube 几何体，对每个面设置不同的 View 矩阵渲染。

### 7.3 Irradiance 卷积 Shader 完整代码

#### 顶点着色器：`Assets/Shaders/Internal/IBL/IrradianceConvolution.vert`

```glsl
#version 450 core

layout(location = 0) in vec3 a_Position;

out vec3 v_WorldPos;

uniform mat4 u_Projection;
uniform mat4 u_View;

void main()
{
    v_WorldPos = a_Position;
    gl_Position = u_Projection * u_View * vec4(a_Position, 1.0);
}
```

#### 片段着色器：`Assets/Shaders/Internal/IBL/IrradianceConvolution.frag`

```glsl
#version 450 core

in vec3 v_WorldPos;

layout(location = 0) out vec4 o_Color;

uniform samplerCube u_EnvironmentMap;   // 原始环境 Cubemap

const float PI = 3.14159265359;

void main()
{
    // 法线方向 = 当前片元对应的 Cubemap 方向
    vec3 N = normalize(v_WorldPos);
    
    vec3 irradiance = vec3(0.0);
    
    // 构建切线空间
    vec3 up = vec3(0.0, 1.0, 0.0);
    vec3 right = normalize(cross(up, N));
    up = normalize(cross(N, right));
    
    // 半球均匀采样（球面坐标遍历）
    float sampleDelta = 0.025;  // 采样步长（越小越精确，越慢）
    float nrSamples = 0.0;
    
    for (float phi = 0.0; phi < 2.0 * PI; phi += sampleDelta)
    {
        for (float theta = 0.0; theta < 0.5 * PI; theta += sampleDelta)
        {
            // 球面坐标 → 切线空间方向
            vec3 tangentSample = vec3(
                sin(theta) * cos(phi),
                sin(theta) * sin(phi),
                cos(theta)
            );
            
            // 切线空间 → 世界空间
            vec3 sampleVec = tangentSample.x * right + tangentSample.y * up + tangentSample.z * N;
            
            // 余弦加权采样（cos(theta) = tangentSample.z）
            // sin(theta) 是球面积分的雅可比行列式
            irradiance += texture(u_EnvironmentMap, sampleVec).rgb * cos(theta) * sin(theta);
            nrSamples++;
        }
    }
    
    // 归一化：π 来自漫反射 BRDF 的 1/π 因子
    irradiance = PI * irradiance * (1.0 / float(nrSamples));
    
    o_Color = vec4(irradiance, 1.0);
}
```

### 7.4 C++ 端生成逻辑

```cpp
/// <summary>
/// 生成 Irradiance Map（32×32 per face，RGB16F Cubemap）
/// 对原始环境 Cubemap 做半球余弦加权卷积
/// </summary>
/// <param name="envCubemap">原始环境 Cubemap 的纹理 ID</param>
/// <returns>Irradiance Map 纹理 ID（GL_TEXTURE_CUBE_MAP）</returns>
uint32_t IBLPrecompute::GenerateIrradianceMap(uint32_t envCubemap)
{
    const uint32_t resolution = 32;
    
    // 1. 创建 Irradiance Cubemap
    uint32_t irradianceMap;
    glCreateTextures(GL_TEXTURE_CUBE_MAP, 1, &irradianceMap);
    glTextureStorage2D(irradianceMap, 1, GL_RGB16F, resolution, resolution);
    glTextureParameteri(irradianceMap, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTextureParameteri(irradianceMap, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTextureParameteri(irradianceMap, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);
    glTextureParameteri(irradianceMap, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTextureParameteri(irradianceMap, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    
    // 2. 创建临时 FBO
    uint32_t fbo, rbo;
    glCreateFramebuffers(1, &fbo);
    glCreateRenderbuffers(1, &rbo);
    glNamedRenderbufferStorage(rbo, GL_DEPTH_COMPONENT24, resolution, resolution);
    glNamedFramebufferRenderbuffer(fbo, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, rbo);
    
    // 3. 渲染 6 个面
    m_IrradianceShader->Bind();
    m_IrradianceShader->SetMat4("u_Projection", m_CaptureProjection);
    
    // 绑定原始环境 Cubemap
    glBindTextureUnit(0, envCubemap);
    m_IrradianceShader->SetInt("u_EnvironmentMap", 0);
    
    glBindFramebuffer(GL_FRAMEBUFFER, fbo);
    glViewport(0, 0, resolution, resolution);
    
    for (int face = 0; face < 6; ++face)
    {
        m_IrradianceShader->SetMat4("u_View", m_CaptureViews[face]);
        
        // 将 Cubemap 的第 face 面绑定为 FBO 颜色附件
        glNamedFramebufferTextureLayer(fbo, GL_COLOR_ATTACHMENT0, irradianceMap, 0, face);
        
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
        
        // 绘制 Cube（使用 SkyboxPass 中相同的 Cube 几何体）
        RenderCommand::DrawArrays(m_CubeVAO, 36);
    }
    
    // 4. 清理
    glDeleteFramebuffers(1, &fbo);
    glDeleteRenderbuffers(1, &rbo);
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    
    return irradianceMap;
}
```

#### 6 面 View 矩阵和 Projection 矩阵

```cpp
// 90° FOV 的透视投影（Cubemap 每面恰好覆盖 90°）
m_CaptureProjection = glm::perspective(glm::radians(90.0f), 1.0f, 0.1f, 10.0f);

// 6 面 View 矩阵（从原点看向 6 个方向）
m_CaptureViews[0] = glm::lookAt(glm::vec3(0), glm::vec3( 1,  0,  0), glm::vec3(0, -1,  0));  // +X
m_CaptureViews[1] = glm::lookAt(glm::vec3(0), glm::vec3(-1,  0,  0), glm::vec3(0, -1,  0));  // -X
m_CaptureViews[2] = glm::lookAt(glm::vec3(0), glm::vec3( 0,  1,  0), glm::vec3(0,  0,  1));  // +Y
m_CaptureViews[3] = glm::lookAt(glm::vec3(0), glm::vec3( 0, -1,  0), glm::vec3(0,  0, -1));  // -Y
m_CaptureViews[4] = glm::lookAt(glm::vec3(0), glm::vec3( 0,  0,  1), glm::vec3(0, -1,  0));  // +Z
m_CaptureViews[5] = glm::lookAt(glm::vec3(0), glm::vec3( 0,  0, -1), glm::vec3(0, -1,  0));  // -Z
```

---

## 八、Step 3：Prefiltered Environment Map 生成

### 8.1 理论说明

Prefiltered Environment Map 预计算了不同粗糙度下的环境镜面反射。使用 **GGX 重要性采样**对环境图做卷积，不同粗糙度的结果存储在 Cubemap 的不同 Mipmap Level 中：

| Mip Level | 粗糙度 | 视觉效果 |
|-----------|--------|----------|
| 0 | 0.0 | 完全清晰的环境反射（镜面） |
| 1 | 0.25 | 轻微模糊 |
| 2 | 0.5 | 中等模糊 |
| 3 | 0.75 | 较模糊 |
| 4 | 1.0 | 完全模糊（接近漫反射） |

### 8.2 方案选择：生成方式

与 Irradiance Map 相同，推荐 **GPU 渲染到 Cubemap 6 面**（方案 A）。

但 Prefiltered Map 需要写入不同的 Mip Level，因此需要：
1. 创建带 Mipmap 的 Cubemap
2. 对每个 Mip Level 的每个面分别渲染

### 8.3 TextureCube Mipmap 扩展

当前 `TextureCube` 的 `glTextureStorage2D` 只分配 1 个 Mip Level。需要新增一个支持 Mipmap 的构造方式。

#### 方案选择：扩展方式

| 方案 | 说明 | 优点 | 缺点 | 推荐 |
|------|------|------|------|------|
| **方案 A：新增带 mipLevels 参数的构造函数** | `TextureCube(uint32_t resolution, uint32_t mipLevels)` | 最小改动；不影响现有接口 | 增加一个构造函数 | ? **最优** |
| 方案 B：在 IBLPrecompute 中直接用裸 OpenGL 创建 | 不修改 TextureCube | 零侵入 | IBL 纹理不受 TextureCube 管理，生命周期需要手动管理 | 其次 |
| 方案 C：所有 Cubemap 都自动生成 Mipmap | 修改现有构造函数 | 统一行为 | 天空盒 Cubemap 不需要 Mipmap，浪费显存 | 不推荐 |

**推荐方案 A**：新增带 `mipLevels` 参数的构造函数。

#### TextureCube.h 新增

```cpp
/// <summary>
/// 创建带 Mipmap 的空 Cubemap（用于 IBL Prefiltered Map）
/// </summary>
/// <param name="resolution">每面分辨率</param>
/// <param name="mipLevels">Mipmap 级数</param>
/// <returns>TextureCube 实例</returns>
static Ref<TextureCube> Create(uint32_t resolution, uint32_t mipLevels);

/// <summary>
/// 带 Mipmap 的空 Cubemap 构造
/// </summary>
TextureCube(uint32_t resolution, uint32_t mipLevels);
```

#### TextureCube.cpp 新增

```cpp
Ref<TextureCube> TextureCube::Create(uint32_t resolution, uint32_t mipLevels)
{
    return CreateRef<TextureCube>(resolution, mipLevels);
}

TextureCube::TextureCube(uint32_t resolution, uint32_t mipLevels)
    : m_Resolution(resolution), m_IsHDR(true)
{
    glCreateTextures(GL_TEXTURE_CUBE_MAP, 1, &m_RendererID);
    glTextureStorage2D(m_RendererID, mipLevels, GL_RGB16F, m_Resolution, m_Resolution);
    
    glTextureParameteri(m_RendererID, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTextureParameteri(m_RendererID, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTextureParameteri(m_RendererID, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);
    glTextureParameteri(m_RendererID, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);  // 三线性过滤
    glTextureParameteri(m_RendererID, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
}
```

### 8.4 Prefilter 卷积 Shader 完整代码

#### 顶点着色器：`Assets/Shaders/Internal/IBL/PrefilterConvolution.vert`

```glsl
#version 450 core

layout(location = 0) in vec3 a_Position;

out vec3 v_WorldPos;

uniform mat4 u_Projection;
uniform mat4 u_View;

void main()
{
    v_WorldPos = a_Position;
    gl_Position = u_Projection * u_View * vec4(a_Position, 1.0);
}
```

#### 片段着色器：`Assets/Shaders/Internal/IBL/PrefilterConvolution.frag`

```glsl
#version 450 core

in vec3 v_WorldPos;

layout(location = 0) out vec4 o_Color;

uniform samplerCube u_EnvironmentMap;   // 原始环境 Cubemap
uniform float u_Roughness;             // 当前 Mip Level 对应的粗糙度

const float PI = 3.14159265359;

// ---- Hammersley 序列 ----

float RadicalInverse_VdC(uint bits)
{
    bits = (bits << 16u) | (bits >> 16u);
    bits = ((bits & 0x55555555u) << 1u) | ((bits & 0xAAAAAAAAu) >> 1u);
    bits = ((bits & 0x33333333u) << 2u) | ((bits & 0xCCCCCCCCu) >> 2u);
    bits = ((bits & 0x0F0F0F0Fu) << 4u) | ((bits & 0xF0F0F0F0u) >> 4u);
    bits = ((bits & 0x00FF00FFu) << 8u) | ((bits & 0xFF00FF00u) >> 8u);
    return float(bits) * 2.3283064365386963e-10;
}

vec2 Hammersley(uint i, uint N)
{
    return vec2(float(i) / float(N), RadicalInverse_VdC(i));
}

// ---- GGX 重要性采样 ----

vec3 ImportanceSampleGGX(vec2 Xi, vec3 N, float roughness)
{
    float a = roughness * roughness;

    float phi = 2.0 * PI * Xi.x;
    float cosTheta = sqrt((1.0 - Xi.y) / (1.0 + (a * a - 1.0) * Xi.y));
    float sinTheta = sqrt(1.0 - cosTheta * cosTheta);

    // 切线空间半向量
    vec3 H;
    H.x = cos(phi) * sinTheta;
    H.y = sin(phi) * sinTheta;
    H.z = cosTheta;

    // 切线空间 → 世界空间
    vec3 up = abs(N.z) < 0.999 ? vec3(0.0, 0.0, 1.0) : vec3(1.0, 0.0, 0.0);
    vec3 tangent = normalize(cross(up, N));
    vec3 bitangent = cross(N, tangent);

    vec3 sampleVec = tangent * H.x + bitangent * H.y + N * H.z;
    return normalize(sampleVec);
}

void main()
{
    vec3 N = normalize(v_WorldPos);
    
    // 假设 V = R = N（简化假设，Epic Games 的做法）
    vec3 R = N;
    vec3 V = R;
    
    const uint SAMPLE_COUNT = 1024u;
    vec3 prefilteredColor = vec3(0.0);
    float totalWeight = 0.0;
    
    for (uint i = 0u; i < SAMPLE_COUNT; ++i)
    {
        vec2 Xi = Hammersley(i, SAMPLE_COUNT);
        vec3 H = ImportanceSampleGGX(Xi, N, u_Roughness);
        vec3 L = normalize(2.0 * dot(V, H) * H - V);
        
        float NdotL = max(dot(N, L), 0.0);
        if (NdotL > 0.0)
        {
            // Mip Level 采样（减少亮点闪烁）
            // 根据 PDF 和纹理分辨率计算合适的 mip level
            float D = DistributionGGX(N, H);
            float NdotH = max(dot(N, H), 0.0);
            float HdotV = max(dot(H, V), 0.0);
            float pdf = D * NdotH / (4.0 * HdotV) + 0.0001;
            
            float resolution = float(textureSize(u_EnvironmentMap, 0).x);
            float saTexel = 4.0 * PI / (6.0 * resolution * resolution);
            float saSample = 1.0 / (float(SAMPLE_COUNT) * pdf + 0.0001);
            
            float mipLevel = (u_Roughness == 0.0) ? 0.0 : 0.5 * log2(saSample / saTexel);
            
            prefilteredColor += textureLod(u_EnvironmentMap, L, mipLevel).rgb * NdotL;
            totalWeight += NdotL;
        }
    }
    
    prefilteredColor = prefilteredColor / totalWeight;
    
    o_Color = vec4(prefilteredColor, 1.0);
}

// ---- 内联 GGX NDF（避免依赖外部文件） ----
float DistributionGGX(vec3 N, vec3 H)
{
    float a = u_Roughness * u_Roughness;
    float a2 = a * a;
    float NdotH = max(dot(N, H), 0.0);
    float NdotH2 = NdotH * NdotH;
    
    float denom = (NdotH2 * (a2 - 1.0) + 1.0);
    denom = PI * denom * denom;
    
    return a2 / max(denom, 0.0000001);
}
```

> **注意**：上述 Shader 中 `DistributionGGX` 函数需要在 `main()` 之前声明（或使用前向声明）。实际代码中应将函数定义放在 `main()` 之前。

### 8.5 C++ 端生成逻辑

```cpp
/// <summary>
/// 生成 Prefiltered Environment Map（128×128 per face，RGB16F Cubemap，5 Mip Levels）
/// 对原始环境 Cubemap 做不同粗糙度的 GGX 重要性采样卷积
/// </summary>
/// <param name="envCubemap">原始环境 Cubemap 的纹理 ID</param>
/// <returns>Prefiltered Map 纹理 ID（GL_TEXTURE_CUBE_MAP）</returns>
uint32_t IBLPrecompute::GeneratePrefilterMap(uint32_t envCubemap)
{
    const uint32_t resolution = 128;
    const uint32_t maxMipLevels = 5;
    
    // 1. 创建带 Mipmap 的 Cubemap
    uint32_t prefilterMap;
    glCreateTextures(GL_TEXTURE_CUBE_MAP, 1, &prefilterMap);
    glTextureStorage2D(prefilterMap, maxMipLevels, GL_RGB16F, resolution, resolution);
    glTextureParameteri(prefilterMap, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTextureParameteri(prefilterMap, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTextureParameteri(prefilterMap, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);
    glTextureParameteri(prefilterMap, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
    glTextureParameteri(prefilterMap, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    
    // 2. 创建临时 FBO
    uint32_t fbo, rbo;
    glCreateFramebuffers(1, &fbo);
    glCreateRenderbuffers(1, &rbo);
    
    // 3. 对每个 Mip Level 渲染
    m_PrefilterShader->Bind();
    m_PrefilterShader->SetMat4("u_Projection", m_CaptureProjection);
    
    glBindTextureUnit(0, envCubemap);
    m_PrefilterShader->SetInt("u_EnvironmentMap", 0);
    
    glBindFramebuffer(GL_FRAMEBUFFER, fbo);
    
    for (uint32_t mip = 0; mip < maxMipLevels; ++mip)
    {
        // 当前 Mip Level 的分辨率
        uint32_t mipWidth = static_cast<uint32_t>(resolution * std::pow(0.5f, mip));
        uint32_t mipHeight = mipWidth;
        
        // 调整 RBO 大小
        glNamedRenderbufferStorage(rbo, GL_DEPTH_COMPONENT24, mipWidth, mipHeight);
        glNamedFramebufferRenderbuffer(fbo, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, rbo);
        
        glViewport(0, 0, mipWidth, mipHeight);
        
        // 粗糙度 = mip / (maxMipLevels - 1)
        float roughness = static_cast<float>(mip) / static_cast<float>(maxMipLevels - 1);
        m_PrefilterShader->SetFloat("u_Roughness", roughness);
        
        // 渲染 6 个面
        for (int face = 0; face < 6; ++face)
        {
            m_PrefilterShader->SetMat4("u_View", m_CaptureViews[face]);
            
            // 绑定 Cubemap 的第 face 面的第 mip 级到 FBO
            glNamedFramebufferTextureLayer(fbo, GL_COLOR_ATTACHMENT0, prefilterMap, mip, face);
            
            glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
            RenderCommand::DrawArrays(m_CubeVAO, 36);
        }
    }
    
    // 4. 清理
    glDeleteFramebuffers(1, &fbo);
    glDeleteRenderbuffers(1, &rbo);
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    
    return prefilterMap;
}
```

---

## 九、Step 4：IBL 预计算管理器

### 9.1 IBLPrecompute 类设计

#### IBLPrecompute.h

```cpp
#pragma once

#include "Lucky/Core/Base.h"
#include "Shader.h"
#include "VertexArray.h"
#include "Buffer.h"
#include "TextureCube.h"

#include <glm/glm.hpp>

namespace Lucky
{
    /// <summary>
    /// IBL 预计算数据（持有 3 张预计算纹理的 ID）
    /// </summary>
    struct IBLData
    {
        uint32_t IrradianceMapID = 0;       // Irradiance Cubemap 纹理 ID
        uint32_t PrefilterMapID = 0;        // Prefiltered Environment Cubemap 纹理 ID
        uint32_t BRDFLUTID = 0;             // BRDF LUT Texture2D 纹理 ID
        uint32_t PrefilterMaxMipLevel = 4;  // Prefiltered Map 最大 Mip Level（maxMipLevels - 1）
        bool Valid = false;                 // 是否有效（已生成）
    };
    
    /// <summary>
    /// IBL 预计算管理器
    /// 负责生成 IBL 所需的 3 张预计算纹理：
    /// 1. BRDF LUT（全局一次性，与环境图无关）
    /// 2. Irradiance Map（依赖环境图，天空盒变更时重新生成）
    /// 3. Prefiltered Environment Map（依赖环境图，天空盒变更时重新生成）
    /// 
    /// 使用方式：
    ///   IBLPrecompute::Init();                          // 初始化（加载 Shader，生成 BRDF LUT）
    ///   IBLPrecompute::GenerateFromCubemap(cubemapID);  // 从环境 Cubemap 生成 IBL 数据
    ///   IBLData data = IBLPrecompute::GetIBLData();     // 获取 IBL 纹理 ID
    ///   IBLPrecompute::Shutdown();                      // 释放资源
    /// </summary>
    class IBLPrecompute
    {
    public:
        /// <summary>
        /// 初始化：加载 IBL 预计算 Shader，生成 BRDF LUT
        /// 在 Renderer3D::Init() 中调用
        /// </summary>
        static void Init();
        
        /// <summary>
        /// 释放所有 IBL 纹理和资源
        /// 在 Renderer3D::Shutdown() 中调用
        /// </summary>
        static void Shutdown();
        
        /// <summary>
        /// 从环境 Cubemap 生成 Irradiance Map 和 Prefiltered Map
        /// 天空盒变更时调用此方法重新生成
        /// </summary>
        /// <param name="envCubemapID">原始环境 Cubemap 的 OpenGL 纹理 ID</param>
        static void GenerateFromCubemap(uint32_t envCubemapID);
        
        /// <summary>
        /// 获取 IBL 预计算数据（3 张纹理 ID）
        /// </summary>
        static const IBLData& GetIBLData();
        
        /// <summary>
        /// IBL 数据是否有效（已生成）
        /// </summary>
        static bool IsValid();
        
    private:
        /// <summary>
        /// 生成 BRDF LUT（512×512，RG16F）
        /// </summary>
        static uint32_t GenerateBRDFLUT();
        
        /// <summary>
        /// 生成 Irradiance Map（32×32 per face，RGB16F Cubemap）
        /// </summary>
        static uint32_t GenerateIrradianceMap(uint32_t envCubemap);
        
        /// <summary>
        /// 生成 Prefiltered Environment Map（128×128 per face，RGB16F Cubemap，5 Mip Levels）
        /// </summary>
        static uint32_t GeneratePrefilterMap(uint32_t envCubemap);
    };
}
```

#### IBLPrecompute.cpp 内部数据

```cpp
#include "lcpch.h"
#include "IBLPrecompute.h"

#include "Renderer3D.h"
#include "RenderCommand.h"
#include "ScreenQuad.h"

#include <glad/glad.h>
#include <glm/ext/matrix_transform.hpp>
#include <glm/ext/matrix_clip_space.hpp>

namespace Lucky
{
    /// <summary>
    /// IBL 预计算内部数据
    /// </summary>
    struct IBLPrecomputeData
    {
        // ---- Shader ----
        Ref<Shader> BRDFShader;             // BRDF LUT 生成 Shader
        Ref<Shader> IrradianceShader;       // Irradiance 卷积 Shader
        Ref<Shader> PrefilterShader;        // Prefilter 卷积 Shader
        
        // ---- Cube 几何体（渲染到 Cubemap 使用） ----
        Ref<VertexArray> CubeVAO;
        Ref<VertexBuffer> CubeVBO;
        
        // ---- 6 面 View 矩阵和 Projection 矩阵 ----
        glm::mat4 CaptureProjection;
        glm::mat4 CaptureViews[6];
        
        // ---- IBL 数据 ----
        IBLData Data;
        
        // ---- 保存/恢复渲染状态 ----
        GLint PreviousViewport[4] = { 0 };
        GLint PreviousFBO = 0;
    };
    
    static IBLPrecomputeData s_IBLData;
    
    // ... 实现方法 ...
}
```

### 9.2 CubemapRenderHelper：渲染状态保存/恢复

IBL 预计算会修改 Viewport、FBO 等全局状态，需要在预计算前后保存/恢复：

```cpp
/// <summary>
/// 保存当前渲染状态（在 IBL 预计算前调用）
/// </summary>
static void SaveRenderState()
{
    glGetIntegerv(GL_VIEWPORT, s_IBLData.PreviousViewport);
    glGetIntegerv(GL_FRAMEBUFFER_BINDING, &s_IBLData.PreviousFBO);
}

/// <summary>
/// 恢复渲染状态（在 IBL 预计算后调用）
/// </summary>
static void RestoreRenderState()
{
    glBindFramebuffer(GL_FRAMEBUFFER, s_IBLData.PreviousFBO);
    glViewport(
        s_IBLData.PreviousViewport[0],
        s_IBLData.PreviousViewport[1],
        s_IBLData.PreviousViewport[2],
        s_IBLData.PreviousViewport[3]
    );
}
```

---

## 十、Step 5：Shader 集成

### 10.1 Lighting.glsl 新增 IBL 函数

在 `Lighting.glsl` 文件末尾（`CalcAllLights` 函数之后、`#endif` 之前）新增：

```glsl
// ==================== IBL 环境光函数 ====================

// ---- IBL 纹理（由 OpaquePass / TransparentPass 绑定） ----
uniform samplerCube u_IrradianceMap;    // Irradiance Cubemap（漫反射 IBL）
uniform samplerCube u_PrefilterMap;     // Prefiltered Environment Cubemap（镜面反射 IBL）
uniform sampler2D u_BRDFLUT;            // BRDF LUT（Fresnel 积分查找表）
uniform int u_IBLEnabled;               // IBL 开关（0 = 关闭, 1 = 开启）
uniform float u_PrefilterMaxMipLevel;   // Prefiltered Map 最大 Mip Level

// ---- Fresnel-Schlick 粗糙度修正版（IBL 使用） ----
// 在掠射角时，粗糙表面的 Fresnel 反射不应该像光滑表面那样强
vec3 FresnelSchlickRoughness(float cosTheta, vec3 F0, float roughness)
{
    return F0 + (max(vec3(1.0 - roughness), F0) - F0) * pow(clamp(1.0 - cosTheta, 0.0, 1.0), 5.0);
}

/// <summary>
/// 计算 IBL 环境光（漫反射 + 镜面反射）
/// </summary>
/// <param name="N">世界空间法线（归一化）</param>
/// <param name="V">视线方向（归一化，从片元指向相机）</param>
/// <param name="albedo">基础颜色</param>
/// <param name="metallic">金属度</param>
/// <param name="roughness">粗糙度</param>
/// <param name="F0">基础反射率</param>
/// <param name="ao">环境光遮蔽</param>
/// <returns>IBL 环境光贡献</returns>
vec3 CalcIBLAmbient(vec3 N, vec3 V, vec3 albedo, float metallic, float roughness, vec3 F0, float ao)
{
    float NdotV = max(dot(N, V), 0.0);
    
    // Fresnel（使用粗糙度修正版）
    vec3 F = FresnelSchlickRoughness(NdotV, F0, roughness);
    
    // 漫反射/镜面反射比例
    vec3 kS = F;
    vec3 kD = (1.0 - kS) * (1.0 - metallic);
    
    // ---- 漫反射 IBL ----
    vec3 irradiance = texture(u_IrradianceMap, N).rgb;
    vec3 diffuseIBL = kD * irradiance * albedo;
    
    // ---- 镜面反射 IBL ----
    vec3 R = reflect(-V, N);
    float lod = roughness * u_PrefilterMaxMipLevel;
    vec3 prefilteredColor = textureLod(u_PrefilterMap, R, lod).rgb;
    vec2 brdf = texture(u_BRDFLUT, vec2(NdotV, roughness)).rg;
    vec3 specularIBL = prefilteredColor * (F * brdf.x + brdf.y);
    
    // ---- 合成 ----
    return (diffuseIBL + specularIBL) * ao;
}
```

### 10.2 Standard.frag 修改

替换常量环境光为 IBL 环境光：

```glsl
// ---- 修改前 ----
// ---- 环境光（简单常量，无 IBL） ----
vec3 ambient = vec3(AMBIENT_STRENGTH) * albedo * ao;

// ---- 修改后 ----
// ---- 环境光（IBL 或常量 fallback） ----
vec3 ambient;
if (u_IBLEnabled != 0)
{
    ambient = CalcIBLAmbient(N, V, albedo, metallic, roughness, F0, ao);
}
else
{
    ambient = vec3(AMBIENT_STRENGTH) * albedo * ao;
}
```

> **说明**：保留常量环境光作为 fallback，当 IBL 未启用时（如天空盒未设置）仍有基本的环境光。

---

## 十一、Step 6：渲染管线集成

### 11.1 RenderContext 扩展

在 `RenderContext.h` 中新增 IBL 数据字段：

```cpp
struct RenderContext
{
    // ... 现有字段 ...
    
    // ---- IBL 数据 ----
    bool IBLEnabled = false;                    // 是否启用 IBL
    uint32_t IrradianceMapID = 0;               // Irradiance Cubemap 纹理 ID
    uint32_t PrefilterMapID = 0;                // Prefiltered Environment Cubemap 纹理 ID
    uint32_t BRDFLUTID = 0;                     // BRDF LUT Texture2D 纹理 ID
    float PrefilterMaxMipLevel = 4.0f;          // Prefiltered Map 最大 Mip Level
};
```

### 11.2 Renderer3D 集成

#### Init() 中初始化 IBL

```cpp
void Renderer3D::Init()
{
    // ... 现有初始化代码 ...
    
    // ======== 加载 IBL 预计算 Shader ========
    s_Data.ShaderLib->Load("Assets/Shaders/Internal/IBL/BRDFIntegration");
    s_Data.ShaderLib->Load("Assets/Shaders/Internal/IBL/IrradianceConvolution");
    s_Data.ShaderLib->Load("Assets/Shaders/Internal/IBL/PrefilterConvolution");
    
    // ======== 初始化 IBL 预计算系统 ========
    IBLPrecompute::Init();
    
    // ... 天空盒加载代码 ...
    
    // 天空盒加载成功后，生成 IBL 数据
    if (skyboxCubemap)
    {
        IBLPrecompute::GenerateFromCubemap(skyboxCubemap->GetRendererID());
    }
}
```

#### EndScene() 中传递 IBL 数据到 RenderContext

```cpp
void Renderer3D::EndScene()
{
    // ... 现有代码 ...
    
    // IBL 数据
    const IBLData& iblData = IBLPrecompute::GetIBLData();
    context.IBLEnabled = iblData.Valid;
    context.IrradianceMapID = iblData.IrradianceMapID;
    context.PrefilterMapID = iblData.PrefilterMapID;
    context.BRDFLUTID = iblData.BRDFLUTID;
    context.PrefilterMaxMipLevel = static_cast<float>(iblData.PrefilterMaxMipLevel);
    
    // ... 执行渲染管线 ...
}
```

#### Shutdown() 中释放 IBL 资源

```cpp
void Renderer3D::Shutdown()
{
    IBLPrecompute::Shutdown();
    s_Data.Pipeline.Shutdown();
}
```

### 11.3 OpaquePass / TransparentPass 绑定 IBL 纹理

在 `OpaquePass::Execute()` 和 `TransparentPass::Execute()` 中，在绑定 Shadow Map 纹理之后，新增 IBL 纹理绑定：

```cpp
// ---- 绑定 IBL 纹理 ----
if (context.IBLEnabled)
{
    RenderCommand::BindTextureUnit(10, context.IrradianceMapID);
    RenderCommand::BindTextureUnit(11, context.PrefilterMapID);
    RenderCommand::BindTextureUnit(12, context.BRDFLUTID);
}

// ... 在 DrawCommand 循环中设置 IBL uniform ...
cmd.MaterialData->GetShader()->SetInt("u_IrradianceMap", 10);
cmd.MaterialData->GetShader()->SetInt("u_PrefilterMap", 11);
cmd.MaterialData->GetShader()->SetInt("u_BRDFLUT", 12);
cmd.MaterialData->GetShader()->SetInt("u_IBLEnabled", context.IBLEnabled ? 1 : 0);
cmd.MaterialData->GetShader()->SetFloat("u_PrefilterMaxMipLevel", context.PrefilterMaxMipLevel);
```

#### Material.cpp 内部 uniform 黑名单更新

在 `IsInternalUniform()` 中新增 IBL 相关 uniform：

```cpp
static bool IsInternalUniform(const std::string& name)
{
    static const std::unordered_set<std::string> s_InternalUniforms = {
        // ... 现有内部 uniform ...
        
        // ---- IBL 系统：由 OpaquePass/TransparentPass 设置 ----
        "u_IrradianceMap",          // Irradiance Cubemap（纹理槽位 10）
        "u_PrefilterMap",           // Prefiltered Environment Cubemap（纹理槽位 11）
        "u_BRDFLUT",                // BRDF LUT（纹理槽位 12）
        "u_IBLEnabled",             // IBL 开关
        "u_PrefilterMaxMipLevel",   // Prefiltered Map 最大 Mip Level
    };
    
    // ... 现有逻辑 ...
}
```

---

## 十二、Step 7：天空盒变更时重新生成

### 方案选择

| 方案 | 说明 | 优点 | 缺点 | 推荐 |
|------|------|------|------|------|
| **方案 A：手动触发** | 天空盒材质变更时，由调用方显式调用 `IBLPrecompute::GenerateFromCubemap()` | 最简单；控制精确 | 需要调用方记得触发 | ? **最优（当前阶段）** |
| 方案 B：脏标记 + 延迟生成 | 天空盒 Cubemap 变更时设置脏标记，在 `BeginScene()` 中检查并重新生成 | 自动化；不遗漏 | 增加复杂度；每帧检查开销（极小） | 其次 |
| 方案 C：观察者模式 | TextureCube 变更时发送事件，IBLPrecompute 监听并重新生成 | 完全解耦 | 过度设计；增加事件系统依赖 | 不推荐 |

**推荐方案 A**（当前阶段）：手动触发。天空盒变更的场景有限（编辑器中切换天空盒材质），手动触发足够。

### 实现

在 `Renderer3D` 中新增公共方法：

```cpp
/// <summary>
/// 设置天空盒材质并重新生成 IBL 数据
/// 当天空盒 Cubemap 变更时调用
/// </summary>
/// <param name="skyboxMaterial">新的天空盒材质</param>
static void SetSkyboxMaterial(const Ref<Material>& skyboxMaterial);
```

```cpp
void Renderer3D::SetSkyboxMaterial(const Ref<Material>& skyboxMaterial)
{
    s_Data.SkyboxMaterial = skyboxMaterial;
    
    // 从天空盒材质中获取 Cubemap 纹理
    if (skyboxMaterial)
    {
        Ref<TextureCube> cubemap = skyboxMaterial->GetTextureCube("u_SkyboxMap");
        if (cubemap)
        {
            IBLPrecompute::GenerateFromCubemap(cubemap->GetRendererID());
            LF_CORE_INFO("IBL data regenerated for new skybox");
            return;
        }
    }
    
    // 天空盒为空，清除 IBL 数据
    // IBLPrecompute 内部处理（Data.Valid = false）
}
```

---

## 十三、方案选择汇总

| 决策点 | 方案 | 推荐 | 原因 |
|--------|------|------|------|
| BRDF LUT 生成方式 | GPU ScreenQuad 渲染 | ? 最优 | 利用现有 ScreenQuad 工具，与项目架构一致 |
| Irradiance Map 生成方式 | GPU 渲染到 Cubemap 6 面 | ? 最优 | 利用现有 Cube 几何体，GPU 并行 |
| Prefilter Map 生成方式 | GPU 渲染到 Cubemap 6 面 × N Mip | ? 最优 | 同上 |
| TextureCube Mipmap 扩展 | 新增带 mipLevels 参数的构造函数 | ? 最优 | 最小改动，不影响现有接口 |
| IBL 纹理单元 | 10, 11, 12 | ? 最优 | 不与现有纹理单元冲突 |
| 天空盒变更触发 | 手动触发 | ? 最优（当前阶段） | 最简单，控制精确 |
| IBL Shader 分支 | `if (u_IBLEnabled)` 运行时分支 | ? 最优（当前阶段） | 简单；未来可升级为 Shader Variant |
| Irradiance Map 分辨率 | 32×32 per face | ? 最优 | 漫反射是低频信息，32×32 足够 |
| Prefilter Map 分辨率 | 128×128 per face, 5 Mip Levels | ? 最优 | 平衡质量和性能 |
| BRDF LUT 分辨率 | 512×512 | ? 最优 | 业界标准，精度足够 |

---

## 十四、实现顺序与依赖关系

```
Step 1: BRDF LUT 生成
  ├─ 新建 BRDFIntegration.vert / .frag
  ├─ 新建 IBLPrecompute.h / .cpp（仅 GenerateBRDFLUT 部分）
  └─ 在 Renderer3D::Init() 中调用
  │
  ↓ （无依赖，可独立验证）
  │
Step 2: Irradiance Map 生成
  ├─ 新建 IrradianceConvolution.vert / .frag
  ├─ IBLPrecompute 新增 GenerateIrradianceMap()
  ├─ IBLPrecompute 新增 Cube VAO + 6 面 View 矩阵
  └─ 在 Renderer3D::Init() 中调用（天空盒加载后）
  │
  ↓ （依赖 Step 1 的 IBLPrecompute 框架）
  │
Step 3: Prefiltered Environment Map 生成
  ├─ 新建 PrefilterConvolution.vert / .frag
  ├─ TextureCube 新增带 mipLevels 的构造函数（可选，也可直接用裸 GL）
  ├─ IBLPrecompute 新增 GeneratePrefilterMap()
  └─ 在 Renderer3D::Init() 中调用（天空盒加载后）
  │
  ↓ （依赖 Step 2 的 Cube 渲染基础设施）
  │
Step 4: Shader 集成
  ├─ Lighting.glsl 新增 FresnelSchlickRoughness() + CalcIBLAmbient()
  ├─ Standard.frag 替换常量环境光为 IBL
  └─ Common.glsl 无需修改（IBL uniform 在 Lighting.glsl 中声明）
  │
  ↓ （依赖 Step 1-3 的纹理生成）
  │
Step 5: 渲染管线集成
  ├─ RenderContext.h 新增 IBL 字段
  ├─ Renderer3D.cpp EndScene() 传递 IBL 数据
  ├─ OpaquePass.cpp 绑定 IBL 纹理 + 设置 uniform
  ├─ TransparentPass.cpp 绑定 IBL 纹理 + 设置 uniform
  └─ Material.cpp 更新内部 uniform 黑名单
  │
  ↓ （依赖 Step 4 的 Shader 修改）
  │
Step 6: 天空盒变更重新生成
  ├─ Renderer3D 新增 SetSkyboxMaterial()
  └─ 编辑器中切换天空盒时调用
```

---

## 十五、验证方法

### 15.1 BRDF LUT 验证

1. 生成 BRDF LUT 后，将其渲染到屏幕上（临时调试代码）
2. 确认左下角（NdotV=0, roughness=0）接近 (1.0, 0.0)
3. 确认右上角（NdotV=1, roughness=1）接近 (0.0, 0.0)
4. 确认整体呈现从红色到黑色的渐变

### 15.2 Irradiance Map 验证

1. 使用一个纯白色 HDR 环境图，生成的 Irradiance Map 应该是均匀的白色
2. 使用一个上蓝下绿的环境图，Irradiance Map 的上半球应偏蓝，下半球应偏绿
3. 将 Irradiance Map 作为天空盒渲染，确认是原始环境图的模糊版本

### 15.3 Prefiltered Map 验证

1. Mip Level 0 应该与原始环境图几乎相同（roughness = 0）
2. 随着 Mip Level 增加，图像应越来越模糊
3. 最高 Mip Level 应接近 Irradiance Map 的效果

### 15.4 完整 IBL 验证

1. **金属球测试**：创建 `metallic=1.0, roughness=0.1` 的球体，应能看到清晰的环境反射
2. **粗糙度渐变测试**：创建一排球体，roughness 从 0.0 到 1.0，环境反射应从清晰到模糊
3. **非金属测试**：创建 `metallic=0.0` 的球体，应有自然的环境漫反射光照
4. **阴影区域测试**：将物体放入阴影中，应有 IBL 提供的环境光填充（不再全黑）
5. **天空盒切换测试**：更换天空盒后，场景中物体的环境反射应随之变化
6. **对比测试**：开启/关闭 IBL（`u_IBLEnabled`），对比常量环境光和 IBL 环境光的差异

### 15.5 性能验证

1. IBL 预计算应在 1 秒内完成（GPU 渲染方式）
2. 运行时 IBL 采样不应有明显的帧率下降（仅增加 3 次纹理采样）
3. 显存占用：BRDF LUT（512×512×RG16F ≈ 1MB）+ Irradiance（32×32×6×RGB16F ≈ 36KB）+ Prefilter（128×128×6×5级×RGB16F ≈ 3.9MB）≈ **5MB**

---

## 十六、设计决策记录

| 决策 | 选择 | 原因 |
|------|------|------|
| IBL 方法 | Split-Sum 近似（Epic Games 2013） | 业界标准，LearnOpenGL/Unity/Unreal 都使用 |
| BRDF LUT 生成 | GPU ScreenQuad 渲染 | 利用现有 ScreenQuad 工具 |
| Irradiance/Prefilter 生成 | GPU 渲染到 Cubemap 6 面 | 利用现有 Cube 几何体 |
| IBL 纹理单元 | 10, 11, 12 | 不与材质纹理（0~6）和阴影纹理（8, 15）冲突 |
| Irradiance 分辨率 | 32×32 | 漫反射低频，32×32 足够 |
| Prefilter 分辨率 | 128×128, 5 Mip | 平衡质量和显存 |
| BRDF LUT 分辨率 | 512×512 | 业界标准 |
| IBL Shader 分支 | 运行时 `if` | 简单，未来可升级为 Shader Variant |
| 天空盒变更触发 | 手动调用 | 当前阶段最简单 |
| IBL 几何遮蔽 k 值 | k = α2/2 | IBL 使用不同于直接光照的 k 值（直接光照 k = (r+1)2/8） |
| Prefilter 采样假设 | V = R = N | Epic Games 的简化假设，效果足够好 |
| Fresnel IBL 版本 | FresnelSchlickRoughness | 粗糙度修正，避免掠射角过亮 |
| 常量环境光保留 | 作为 fallback | IBL 未启用时仍有基本环境光 |
