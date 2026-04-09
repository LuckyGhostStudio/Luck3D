# Phase R2：PBR Shader（Cook-Torrance BRDF）

> **文档版本**：v1.0  
> **创建日期**：2026-04-07  
> **优先级**：?? P0（核心目标）  
> **预计工作量**：3-5 天  
> **前置依赖**：Phase R1（Vertex 结构升级 + Tangent 空间）  
> **文档说明**：本文档详细描述如何将当前的 Blinn-Phong 光照模型替换为基于物理的 Cook-Torrance PBR 渲染模型，采用 Metallic-Roughness 工作流（与 Unity Standard Shader 和 glTF 2.0 一致）。所有代码可直接对照实现。

---

## 目录

- [一、现状分析](#一现状分析)
- [二、改进目标](#二改进目标)
- [三、涉及的文件清单](#三涉及的文件清单)
- [四、PBR 理论基础](#四pbr-理论基础)
  - [4.1 渲染方程](#41-渲染方程)
  - [4.2 Cook-Torrance BRDF](#42-cook-torrance-brdf)
  - [4.3 三个核心函数](#43-三个核心函数)
- [五、PBR 材质参数设计](#五pbr-材质参数设计)
  - [5.1 Uniform 参数列表](#51-uniform-参数列表)
  - [5.2 纹理槽位分配](#52-纹理槽位分配)
  - [5.3 默认纹理策略](#53-默认纹理策略)
  - [5.4 与当前 Blinn-Phong 参数的映射](#54-与当前-blinn-phong-参数的映射)
- [六、Standard.vert 完整代码](#六standardvert-完整代码)
- [七、Standard.frag 完整代码](#七standardfrag-完整代码)
  - [7.1 PBR 核心函数](#71-pbr-核心函数)
  - [7.2 法线贴图采样](#72-法线贴图采样)
  - [7.3 完整 Fragment Shader](#73-完整-fragment-shader)
- [八、Renderer3D.cpp 修改](#八renderer3dcpp-修改)
  - [8.1 默认材质参数更新](#81-默认材质参数更新)
  - [8.2 默认纹理绑定](#82-默认纹理绑定)
- [九、材质系统兼容性分析](#九材质系统兼容性分析)
- [十、方案选择](#十方案选择)
  - [10.1 PBR 工作流选择](#101-pbr-工作流选择)
  - [10.2 环境光处理方案](#102-环境光处理方案)
  - [10.3 Gamma 校正方案](#103-gamma-校正方案)
- [十一、验证方法](#十一验证方法)
- [十二、设计决策记录](#十二设计决策记录)

---

## 一、现状分析

### 当前光照模型：Blinn-Phong

```glsl
// Standard.frag - 当前实现
// 环境光
vec3 ambient = u_AmbientCoeff * u_Light.Color * u_Light.Intensity;

// 漫反射
float diff = max(dot(N, L), 0.0);
vec3 diffuse = diff * u_DiffuseCoeff * u_Light.Color * u_Light.Intensity;

// 镜面反射
float spec = pow(max(dot(R, V), 0.0), u_Shininess);
vec3 specular = spec * u_SpecularCoeff * u_Light.Color * u_Light.Intensity;

// 最终颜色
o_Color = mainColor * v_Input.Color * vec4(ambient + diffuse + specular, 1.0);
```

### 当前材质参数

```cpp
// Renderer3D::Init() 中的默认材质参数
s_Data.DefaultMaterial->SetFloat3("u_AmbientCoeff", glm::vec3(0.2f));
s_Data.DefaultMaterial->SetFloat3("u_DiffuseCoeff", glm::vec3(0.8f));
s_Data.DefaultMaterial->SetFloat3("u_SpecularCoeff", glm::vec3(0.5f));
s_Data.DefaultMaterial->SetFloat("u_Shininess", 32.0f);
```

### 问题

| 编号 | 问题 | 影响 |
|------|------|------|
| R2-01 | Blinn-Phong 不是基于物理的 | 无法正确模拟金属/非金属材质 |
| R2-02 | 无法表达金属度/粗糙度 | 材质表现力有限 |
| R2-03 | 无法线贴图支持 | 表面细节不足 |
| R2-04 | 无自发光支持 | 无法表现发光材质 |
| R2-05 | 无 AO 贴图支持 | 缺少环境光遮蔽细节 |

---

## 二、改进目标

1. **替换光照模型**：从 Blinn-Phong 替换为 Cook-Torrance PBR
2. **Metallic-Roughness 工作流**：与 Unity Standard Shader 和 glTF 2.0 一致
3. **法线贴图支持**：利用 Phase R1 添加的 TBN 矩阵
4. **多纹理支持**：Albedo、Metallic、Roughness、Normal、AO、Emission 贴图
5. **更新默认材质参数**：从 Blinn-Phong 参数切换到 PBR 参数
6. **Gamma 校正**：在 Fragment Shader 输出时进行 sRGB 转换

---

## 三、涉及的文件清单

| 文件路径 | 操作 | 说明 |
|---------|------|------|
| `Luck3DApp/Assets/Shaders/Standard.vert` | 修改 | 更新 VertexOutput 结构（如果 R1 未完成则需要添加 TBN） |
| `Luck3DApp/Assets/Shaders/Standard.frag` | **完全重写** | 从 Blinn-Phong 替换为 Cook-Torrance PBR |
| `Lucky/Source/Lucky/Renderer/Renderer3D.cpp` | 修改 | 更新默认材质参数 |

> **注意**：不需要修改 `Material.h/cpp`。当前材质系统的 Shader 内省机制（`Introspect()`）会自动发现新 Shader 中的 uniform 并生成对应的 `MaterialProperty`。Inspector 面板也会自动显示新参数。

---

## 四、PBR 理论基础

### 4.1 渲染方程

```
Lo(p, ωo) = Le(p, ωo) + ∫ fr(p, ωi, ωo) · Li(p, ωi) · (ωi · n) dωi
```

其中：
- `Lo` = 出射辐射度（我们要计算的）
- `Le` = 自发光
- `fr` = BRDF（双向反射分布函数）
- `Li` = 入射辐射度（光源）
- `ωi · n` = cos(θ)，Lambert 余弦定律

### 4.2 Cook-Torrance BRDF

```
fr(p, ωi, ωo) = kd · f_lambert + ks · f_cook-torrance

f_lambert = albedo / π

f_cook-torrance = D(h) · F(v,h) · G(l,v,h) / (4 · (ωo · n) · (ωi · n))
```

其中：
- `kd` = 漫反射比例 = `(1 - F) * (1 - metallic)`
- `ks` = 镜面反射比例 = `F`（Fresnel 项）
- `D` = 法线分布函数（Normal Distribution Function）
- `F` = 菲涅尔方程（Fresnel Equation）
- `G` = 几何遮蔽函数（Geometry Function）

### 4.3 三个核心函数

#### D：GGX/Trowbridge-Reitz 法线分布函数

```
D(h) = α2 / (π · ((n·h)2 · (α2 - 1) + 1)2)

其中 α = roughness2
```

#### F：Fresnel-Schlick 近似

```
F(h, v) = F0 + (1 - F0) · (1 - (h·v))?

其中 F0 = 基础反射率
  - 非金属：F0 ≈ 0.04（大多数电介质）
  - 金属：F0 = albedo（金属的反射颜色就是其表面颜色）
  - 混合：F0 = mix(0.04, albedo, metallic)
```

#### G：Smith's Schlick-GGX 几何遮蔽函数

```
G(n, v, l) = G_sub(n, v) · G_sub(n, l)

G_sub(n, x) = (n·x) / ((n·x) · (1 - k) + k)

其中 k = (roughness + 1)2 / 8  （直接光照）
```

---

## 五、PBR 材质参数设计

### 5.1 Uniform 参数列表

| Uniform 名称 | 类型 | 默认值 | 说明 |
|-------------|------|--------|------|
| `u_Albedo` | vec4 | (0.8, 0.8, 0.8, 1.0) | 基础颜色（RGBA） |
| `u_Metallic` | float | 0.0 | 金属度 [0, 1] |
| `u_Roughness` | float | 0.5 | 粗糙度 [0, 1] |
| `u_AO` | float | 1.0 | 环境光遮蔽 [0, 1] |
| `u_Emission` | vec3 | (0, 0, 0) | 自发光颜色 |
| `u_EmissionIntensity` | float | 1.0 | 自发光强度 |
| `u_AlbedoMap` | sampler2D | 白色 1×1 纹理 | 基础颜色贴图（无贴图时绑定默认白色纹理） |
| `u_NormalMap` | sampler2D | 法线蓝 1×1 纹理 | 法线贴图（无贴图时绑定默认法线纹理 (128,128,255)） |
| `u_MetallicMap` | sampler2D | 白色 1×1 纹理 | 金属度贴图（无贴图时绑定默认白色纹理） |
| `u_RoughnessMap` | sampler2D | 白色 1×1 纹理 | 粗糙度贴图（无贴图时绑定默认白色纹理） |
| `u_AOMap` | sampler2D | 白色 1×1 纹理 | 环境光遮蔽贴图（无贴图时绑定默认白色纹理） |
| `u_EmissionMap` | sampler2D | 白色 1×1 纹理 | 自发光贴图（无贴图时绑定默认白色纹理） |

> **注意**：不再使用 `u_UseXxxMap` 纹理开关。当材质未设置某个贴图时，引擎自动绑定对应的默认 1×1 纹理，Shader 无条件采样。详见 [5.3 默认纹理策略](#53-默认纹理策略)。

### 5.2 纹理槽位分配

| 槽位 | 纹理 | 默认纹理 | 说明 |
|------|------|----------|------|
| 0 | u_AlbedoMap | 白色 (1,1,1,1) | 乘以 u_Albedo 后 = u_Albedo 本身 |
| 1 | u_NormalMap | 法线蓝 (0.5, 0.5, 1.0) | 解码后 = (0,0,1)，即"不扰动法线" |
| 2 | u_MetallicMap | 白色 (1,1,1,1) | .r = 1.0，乘以 u_Metallic 后 = u_Metallic 本身 |
| 3 | u_RoughnessMap | 白色 (1,1,1,1) | .r = 1.0，乘以 u_Roughness 后 = u_Roughness 本身 |
| 4 | u_AOMap | 白色 (1,1,1,1) | .r = 1.0，乘以 u_AO 后 = u_AO 本身 |
| 5 | u_EmissionMap | 白色 (1,1,1,1) | 乘以 u_Emission 后 = u_Emission 本身 |

> **注意**：当前 `MaxTextureSlots = 32`，6 个纹理槽位完全足够。

### 5.3 默认纹理策略

采用**默认纹理方案**替代纹理开关（`u_UseXxxMap`），这是参考 Unity/Godot 等主流引擎的做法。

#### 方案对比

| 方案 | 运行时分支 | GPU 开销 | 实现复杂度 | 代码简洁度 |
|------|-----------|---------|-----------|-----------|
| 显式开关（`u_UseXxxMap`） | 有（每帧 if 判断） | 中等（分支可能导致 warp divergence） | 低 | ? 冗余 |
| **默认纹理（本方案）** | **无** | **低（1×1 纹理采样几乎免费）** | **低** | **? 简洁** |
| Shader Variant（Unity 做法） | 无 | 最低（编译期消除） | 高（需要变体管理系统） | ? 简洁 |

#### 核心原理

当材质未设置某个贴图时，引擎绑定对应的 1×1 默认纹理。由于默认纹理的值为 `1.0`（白色），采样结果参与乘法运算时不影响标量参数：

```
参数 × texture(默认白色纹理) = 参数 × 1.0 = 参数本身
```

这样 Shader 中无需任何 `if` 分支，无条件对所有纹理进行采样即可。

#### 各贴图类型的默认纹理

| 贴图类型 | 默认纹理颜色 | RGBA 值 | 原因 |
|---------|------------|---------|------|
| Albedo | 白色 | (1, 1, 1, 1) | 乘以 u_Albedo 后 = u_Albedo 本身 |
| Normal | 法线蓝 | (0.5, 0.5, 1.0, 1.0) | 解码后 = (0,0,1)，即"不扰动法线" |
| Metallic | 白色 | (1, 1, 1, 1) | .r = 1.0，乘以 u_Metallic 后 = u_Metallic 本身 |
| Roughness | 白色 | (1, 1, 1, 1) | .r = 1.0，乘以 u_Roughness 后 = u_Roughness 本身 |
| AO | 白色 | (1, 1, 1, 1) | .r = 1.0，乘以 u_AO 后 = u_AO 本身 |
| Emission | 白色 | (1, 1, 1, 1) | 乘以 u_Emission 后 = u_Emission 本身 |

#### 优势

1. **消除 6 个 `uniform int` 和 6 个 `if` 分支**：Shader 更简洁，GPU 无分支开销
2. **1×1 纹理采样零开销**：整个纹理只有一个 texel，永远命中 GPU 纹理缓存
3. **代码一致性**：所有纹理统一处理，无特殊路径
4. **后续可升级**：未来如需极致性能优化，可引入 Shader Variant 系统（编译期消除采样）

### 5.4 与当前 Blinn-Phong 参数的映射

| 旧参数 (Blinn-Phong) | 新参数 (PBR) | 说明 |
|----------------------|-------------|------|
| `u_AmbientCoeff` | 移除 | PBR 中环境光由 AO 和常量环境光控制 |
| `u_DiffuseCoeff` | `u_Albedo` | 基础颜色替代漫反射系数 |
| `u_SpecularCoeff` | `u_Metallic` + `u_Roughness` | 镜面反射由金属度和粗糙度控制 |
| `u_Shininess` | `u_Roughness` | 粗糙度替代光泽度 |
| `u_MainTexture` | `u_AlbedoMap` | 主纹理改名为 Albedo 贴图 |

---

## 六、Standard.vert 完整代码

```glsl
#version 450 core

layout(location = 0) in vec3 a_Position;    // 位置
layout(location = 1) in vec4 a_Color;       // 颜色
layout(location = 2) in vec3 a_Normal;      // 法线
layout(location = 3) in vec2 a_TexCoord;    // 纹理坐标
layout(location = 4) in vec4 a_Tangent;     // 切线 + 手性

// 相机 Uniform 缓冲区
layout(std140, binding = 0) uniform Camera
{
    mat4 ViewProjectionMatrix;
    vec3 Position;
} u_Camera;

// 模型矩阵
uniform mat4 u_ObjectToWorldMatrix;

// 顶点着色器输出数据
struct VertexOutput
{
    vec4 Color;
    vec3 Normal;
    vec2 TexCoord;
    vec3 WorldPos;
    mat3 TBN;
};

layout(location = 0) out VertexOutput v_Output;

void main()
{
    v_Output.Color = a_Color;
    v_Output.TexCoord = a_TexCoord;

    // 法线矩阵
    mat3 normalMatrix = mat3(transpose(inverse(u_ObjectToWorldMatrix)));

    // 变换法向量到世界空间
    vec3 N = normalize(normalMatrix * a_Normal);
    v_Output.Normal = N;

    // 计算 TBN 矩阵
    vec3 T = normalize(normalMatrix * a_Tangent.xyz);
    T = normalize(T - dot(T, N) * N);  // Gram-Schmidt 正交化
    vec3 B = cross(N, T) * a_Tangent.w;
    v_Output.TBN = mat3(T, B, N);

    // 计算世界空间位置
    vec4 worldPos = u_ObjectToWorldMatrix * vec4(a_Position, 1.0);
    v_Output.WorldPos = worldPos.xyz;

    gl_Position = u_Camera.ViewProjectionMatrix * worldPos;
}
```

---

## 七、Standard.frag 完整代码

### 7.1 PBR 核心函数

```glsl
const float PI = 3.14159265359;

// ---- GGX/Trowbridge-Reitz 法线分布函数 ----
float DistributionGGX(vec3 N, vec3 H, float roughness)
{
    float a = roughness * roughness;
    float a2 = a * a;
    float NdotH = max(dot(N, H), 0.0);
    float NdotH2 = NdotH * NdotH;
    
    float denom = (NdotH2 * (a2 - 1.0) + 1.0);
    denom = PI * denom * denom;
    
    return a2 / max(denom, 0.0000001);  // 防止除以零
}

// ---- Fresnel-Schlick 近似 ----
vec3 FresnelSchlick(float cosTheta, vec3 F0)
{
    return F0 + (1.0 - F0) * pow(clamp(1.0 - cosTheta, 0.0, 1.0), 5.0);
}

// ---- Smith's Schlick-GGX 几何遮蔽（单方向） ----
float GeometrySchlickGGX(float NdotV, float roughness)
{
    float r = (roughness + 1.0);
    float k = (r * r) / 8.0;
    
    return NdotV / (NdotV * (1.0 - k) + k);
}

// ---- Smith's 几何遮蔽（双方向） ----
float GeometrySmith(vec3 N, vec3 V, vec3 L, float roughness)
{
    float NdotV = max(dot(N, V), 0.0);
    float NdotL = max(dot(N, L), 0.0);
    
    float ggx1 = GeometrySchlickGGX(NdotV, roughness);
    float ggx2 = GeometrySchlickGGX(NdotL, roughness);
    
    return ggx1 * ggx2;
}
```

### 7.2 法线贴图采样

```glsl
vec3 GetNormal()
{
    // 无条件采样法线贴图（无贴图时绑定默认法线纹理 (0.5, 0.5, 1.0)，解码后为 (0,0,1)）
    vec3 normalMapSample = texture(u_NormalMap, v_Input.TexCoord).rgb;
    // [0,1] → [-1,1]
    normalMapSample = normalMapSample * 2.0 - 1.0;
    // 转换到世界空间
    return normalize(v_Input.TBN * normalMapSample);
}
```

> **说明**：不再需要 `u_UseNormalMap` 开关。当未设置法线贴图时，引擎绑定默认法线纹理 `(128, 128, 255)`，解码后为 `(0, 0, 1)`，经 TBN 变换后等价于原始顶点法线。TBN 矩阵乘法开销极小，保持代码一致性更重要。

### 7.3 完整 Fragment Shader

```glsl
#version 450 core

layout(location = 0) out vec4 o_Color;

// 相机 Uniform 缓冲区
layout(std140, binding = 0) uniform Camera
{
    mat4 ViewProjectionMatrix;
    vec3 Position;
} u_Camera;

// 光照 Uniform 缓冲区
layout(std140, binding = 1) uniform Light
{
    float Intensity;
    vec3 Direction;
    vec3 Color;
} u_Light;

// 顶点着色器输出数据
struct VertexOutput
{
    vec4 Color;
    vec3 Normal;
    vec2 TexCoord;
    vec3 WorldPos;
    mat3 TBN;
};

layout(location = 0) in VertexOutput v_Input;

// ---- PBR 材质参数 ----
uniform vec4  u_Albedo;             // 基础颜色
uniform float u_Metallic;           // 金属度
uniform float u_Roughness;          // 粗糙度
uniform float u_AO;                 // 环境光遮蔽
uniform vec3  u_Emission;           // 自发光颜色
uniform float u_EmissionIntensity;  // 自发光强度

// ---- PBR 纹理（无贴图时引擎绑定默认纹理，无需开关） ----
uniform sampler2D u_AlbedoMap;     // 默认：白色 1×1
uniform sampler2D u_NormalMap;     // 默认：法线蓝 1×1 (128,128,255)
uniform sampler2D u_MetallicMap;   // 默认：白色 1×1
uniform sampler2D u_RoughnessMap;  // 默认：白色 1×1
uniform sampler2D u_AOMap;         // 默认：白色 1×1
uniform sampler2D u_EmissionMap;   // 默认：白色 1×1

// ---- 常量 ----
const float PI = 3.14159265359;
const float AMBIENT_STRENGTH = 0.03;  // 常量环境光强度（无 IBL 时的替代方案）

// ==================== PBR 核心函数 ====================

float DistributionGGX(vec3 N, vec3 H, float roughness)
{
    float a = roughness * roughness;
    float a2 = a * a;
    float NdotH = max(dot(N, H), 0.0);
    float NdotH2 = NdotH * NdotH;
    
    float denom = (NdotH2 * (a2 - 1.0) + 1.0);
    denom = PI * denom * denom;
    
    return a2 / max(denom, 0.0000001);
}

vec3 FresnelSchlick(float cosTheta, vec3 F0)
{
    return F0 + (1.0 - F0) * pow(clamp(1.0 - cosTheta, 0.0, 1.0), 5.0);
}

float GeometrySchlickGGX(float NdotV, float roughness)
{
    float r = (roughness + 1.0);
    float k = (r * r) / 8.0;
    return NdotV / (NdotV * (1.0 - k) + k);
}

float GeometrySmith(vec3 N, vec3 V, vec3 L, float roughness)
{
    float NdotV = max(dot(N, V), 0.0);
    float NdotL = max(dot(N, L), 0.0);
    return GeometrySchlickGGX(NdotV, roughness) * GeometrySchlickGGX(NdotL, roughness);
}

// ==================== 法线获取 ====================

vec3 GetNormal()
{
    // 无条件采样（无贴图时绑定默认法线纹理，解码后为 (0,0,1)，等价于不扰动法线）
    vec3 normalMapSample = texture(u_NormalMap, v_Input.TexCoord).rgb;
    normalMapSample = normalMapSample * 2.0 - 1.0;
    return normalize(v_Input.TBN * normalMapSample);
}

// ==================== 主函数 ====================

void main()
{
    // ---- 采样材质参数（无条件采样，无贴图时默认纹理返回 1.0，不影响标量值） ----
    vec4 albedo4 = u_Albedo * texture(u_AlbedoMap, v_Input.TexCoord);
    vec3 albedo = albedo4.rgb;
    float alpha = albedo4.a;
    
    float metallic = u_Metallic * texture(u_MetallicMap, v_Input.TexCoord).r;
    
    float roughness = u_Roughness * texture(u_RoughnessMap, v_Input.TexCoord).r;
    // 限制最小粗糙度，避免除以零
    roughness = max(roughness, 0.04);
    
    float ao = u_AO * texture(u_AOMap, v_Input.TexCoord).r;
    
    vec3 emission = u_Emission * u_EmissionIntensity * texture(u_EmissionMap, v_Input.TexCoord).rgb;
    
    // ---- 获取法线 ----
    vec3 N = GetNormal();
    
    // ---- 视线方向 ----
    vec3 V = normalize(u_Camera.Position - v_Input.WorldPos);
    
    // ---- 计算 F0（基础反射率） ----
    // 非金属：F0 ≈ 0.04，金属：F0 = albedo
    vec3 F0 = vec3(0.04);
    F0 = mix(F0, albedo, metallic);
    
    // ---- 光照计算（目前仅方向光） ----
    vec3 Lo = vec3(0.0);
    
    // 方向光
    {
        vec3 L = normalize(-u_Light.Direction);
        vec3 H = normalize(V + L);
        
        // 光照辐射度
        vec3 radiance = u_Light.Color * u_Light.Intensity;
        
        // Cook-Torrance BRDF
        float D = DistributionGGX(N, H, roughness);
        vec3  F = FresnelSchlick(max(dot(H, V), 0.0), F0);
        float G = GeometrySmith(N, V, L, roughness);
        
        // 镜面反射
        vec3 numerator = D * F * G;
        float denominator = 4.0 * max(dot(N, V), 0.0) * max(dot(N, L), 0.0) + 0.0001;
        vec3 specular = numerator / denominator;
        
        // 漫反射
        vec3 kS = F;                            // 镜面反射比例 = Fresnel
        vec3 kD = (vec3(1.0) - kS) * (1.0 - metallic);  // 漫反射比例（金属无漫反射）
        
        float NdotL = max(dot(N, L), 0.0);
        
        Lo += (kD * albedo / PI + specular) * radiance * NdotL;
    }
    
    // ---- 环境光（简单常量，无 IBL） ----
    vec3 ambient = vec3(AMBIENT_STRENGTH) * albedo * ao;
    
    // ---- 最终颜色 ----
    vec3 color = ambient + Lo + emission;
    
    // ---- Gamma 校正（线性空间 → sRGB） ----
    color = pow(color, vec3(1.0 / 2.2));
    
    o_Color = vec4(color, alpha);
}
```

---

## 八、Renderer3D.cpp 修改

### 8.1 默认材质参数更新

```cpp
void Renderer3D::Init()
{
    // ... 现有初始化代码 ...
    
    // 更新默认材质参数（从 Blinn-Phong 切换到 PBR）
    // 移除旧参数（Shader 内省会自动发现新参数，旧参数不再存在于新 Shader 中）
    // s_Data.DefaultMaterial->SetFloat3("u_AmbientCoeff", ...);   // 已移除
    // s_Data.DefaultMaterial->SetFloat3("u_DiffuseCoeff", ...);   // 已移除
    // s_Data.DefaultMaterial->SetFloat3("u_SpecularCoeff", ...);  // 已移除
    // s_Data.DefaultMaterial->SetFloat("u_Shininess", ...);       // 已移除
    
    // PBR 默认参数
    s_Data.DefaultMaterial->SetFloat4("u_Albedo", glm::vec4(0.8f, 0.8f, 0.8f, 1.0f));
    s_Data.DefaultMaterial->SetFloat("u_Metallic", 0.0f);
    s_Data.DefaultMaterial->SetFloat("u_Roughness", 0.5f);
    s_Data.DefaultMaterial->SetFloat("u_AO", 1.0f);
    s_Data.DefaultMaterial->SetFloat3("u_Emission", glm::vec3(0.0f));
    s_Data.DefaultMaterial->SetFloat("u_EmissionIntensity", 1.0f);
    
    // 不再需要纹理开关（u_UseXxxMap），默认纹理方案自动处理
}
```

### 8.2 默认纹理绑定

采用默认纹理方案后，需要在 `DrawMesh` 中为所有 PBR 纹理槽位绑定默认纹理，然后由 `Material::Apply()` 覆盖已设置的纹理槽位。

#### 需要新增的默认纹理

在 `Renderer3D::Init()` 中创建默认法线纹理：

```cpp
void Renderer3D::Init()
{
    // ... 现有初始化代码 ...
    
    // 创建默认白色纹理（已存在）
    // s_Data.WhiteTexture = ...
    
    // 创建默认法线纹理（1×1，颜色 (128, 128, 255, 255)，解码后为 (0,0,1)）
    uint32_t normalData = 0xFFFF8080;  // RGBA: (128, 128, 255, 255)
    s_Data.DefaultNormalTexture = Texture2D::Create(1, 1);
    s_Data.DefaultNormalTexture->SetData(&normalData, sizeof(uint32_t));
}
```

> **注意**：需要在 `Renderer3DData` 结构体中添加 `Ref<Texture2D> DefaultNormalTexture;` 成员。

#### DrawMesh 中绑定默认纹理

```cpp
void Renderer3D::DrawMesh(/* ... */)
{
    // 绑定默认纹理到所有 PBR 纹理槽位
    s_Data.WhiteTexture->Bind(0);          // u_AlbedoMap     → 白色，乘以 u_Albedo = u_Albedo
    s_Data.DefaultNormalTexture->Bind(1);  // u_NormalMap      → 法线蓝，解码后 = (0,0,1)
    s_Data.WhiteTexture->Bind(2);          // u_MetallicMap    → 白色，.r = 1.0
    s_Data.WhiteTexture->Bind(3);          // u_RoughnessMap   → 白色，.r = 1.0
    s_Data.WhiteTexture->Bind(4);          // u_AOMap          → 白色，.r = 1.0
    s_Data.WhiteTexture->Bind(5);          // u_EmissionMap    → 白色，乘以 u_Emission = u_Emission
    
    // Material::Apply() 会覆盖已设置贴图的槽位
    material->Apply();
    
    // ... 绘制调用 ...
}
```

#### 工作流程

```
DrawMesh 调用
    │
    ├─ 1. 绑定所有默认纹理到 slot 0~5
    │     （白色 × 5 + 法线蓝 × 1）
    │
    ├─ 2. Material::Apply() 覆盖已设置的纹理
    │     （例如材质设置了 AlbedoMap，则 slot 0 被覆盖为实际贴图）
    │
    └─ 3. Shader 无条件采样所有纹理
          （未设置的贴图 → 采样默认纹理 → 返回 1.0 → 不影响标量参数）
```

---

## 九、材质系统兼容性分析

当前材质系统已经支持 Shader 内省（`Shader::Introspect()`），会自动发现 Shader 中所有 active uniform 并生成对应的 `MaterialProperty`。

| 功能 | 兼容性 | 说明 |
|------|--------|------|
| 自动发现新 uniform | ? | `Introspect()` 会发现 `u_Albedo`、`u_Metallic` 等 |
| Inspector 自动显示 | ? | `InspectorPanel` 遍历 `MaterialProperty` 列表 |
| Float 类型编辑 | ? | `u_Metallic`、`u_Roughness` 等 |
| Vec3/Vec4 类型编辑 | ? | `u_Albedo`、`u_Emission` 等 |
| Sampler2D 纹理 | ? | `u_AlbedoMap` 等纹理 |
| 材质序列化 | ? | 属性值会随材质保存/加载 |

**结论**：不需要修改 `Material` 类或 `InspectorPanel`。

> **改进建议**（非本 Phase 范围）：
> - `u_Metallic` 和 `u_Roughness` 应该限制在 [0, 1] 范围内。未来可以在 Inspector 中添加 Slider。

---

## 十、方案选择

### 10.1 PBR 工作流选择

| 方案 | 说明 | 优点 | 缺点 |
|------|------|------|------|
| **方案 A：Metallic-Roughness（推荐）** | 金属度 + 粗糙度 | 与 glTF 2.0 和 Unity Standard 一致 | 金属/非金属边界可能有瑕疵 |
| 方案 B：Specular-Glossiness | 镜面反射颜色 + 光泽度 | 更直观的镜面反射控制 | 非标准，glTF 扩展 |

**推荐方案 A**：Metallic-Roughness 工作流。与 glTF 2.0 规范和 Unity Standard Shader 一致，是业界主流。

### 10.2 环境光处理方案

| 方案 | 说明 | 优点 | 缺点 |
|------|------|------|------|
| **方案 A：常量环境光（推荐当前阶段）** | `ambient = 0.03 * albedo * ao` | 实现最简单 | 不够真实 |
| 方案 B：IBL（Image-Based Lighting） | 使用 Cubemap 预卷积 | 最真实的环境光 | 需要 Cubemap 加载、预卷积、BRDF LUT |
| 方案 C：球谐光照（SH） | 使用球谐系数近似环境光 | 比常量好，比 IBL 简单 | 需要预计算 SH 系数 |

**推荐方案 A**（当前阶段）：常量环境光。IBL 可以在后续 Phase 中添加。

### 10.3 Gamma 校正方案

| 方案 | 说明 | 优点 | 缺点 |
|------|------|------|------|
| **方案 A：Shader 中手动 Gamma 校正（推荐当前阶段）** | `color = pow(color, 1/2.2)` | 最简单，不需要修改 FBO | 每个 Shader 都需要添加 |
| 方案 B：sRGB Framebuffer | `GL_FRAMEBUFFER_SRGB` | 自动转换，更准确 | 需要修改 FBO 创建逻辑 |

**推荐方案 A**（当前阶段）：在 Shader 中手动进行 Gamma 校正。Phase R5（HDR + Tonemapping）中会统一处理。

---

## 十一、验证方法

### 11.1 基本 PBR 验证

1. 创建一个 Sphere，设置 `u_Metallic = 0.0, u_Roughness = 0.5`（非金属，中等粗糙）
2. 确认有柔和的漫反射和适度的高光
3. 调整 `u_Roughness` 从 0.04 到 1.0，确认高光从锐利变为模糊

### 11.2 金属度验证

1. 设置 `u_Metallic = 1.0, u_Roughness = 0.1`（金属，光滑）
2. 确认高光颜色与 Albedo 颜色一致（金属的 F0 = albedo）
3. 确认几乎没有漫反射（金属无漫反射）

### 11.3 法线贴图验证

1. 为材质设置一个法线贴图（引擎会自动绑定到 slot 1，覆盖默认法线纹理）
2. 确认表面有凹凸感
3. 旋转光源，确认凹凸感随光照方向变化
4. 移除法线贴图后，确认表面恢复平滑（默认法线纹理生效）

### 11.4 Gamma 校正验证

1. 对比开启/关闭 Gamma 校正的效果
2. 开启后，中间灰度应该更亮（线性空间的 0.5 在 sRGB 中约为 0.73）

### 11.5 能量守恒验证

1. 设置 `u_Roughness = 0.0`（完全光滑）
2. 确认物体不会比光源更亮（能量守恒）
3. 增加 `u_Roughness`，确认总亮度不增加

---

## 十二、设计决策记录

| 决策 | 选择 | 原因 |
|------|------|------|
| PBR 工作流 | Metallic-Roughness | 与 glTF 2.0 和 Unity Standard 一致 |
| BRDF 模型 | Cook-Torrance | 业界标准，LearnOpenGL/Unity/Unreal 都使用 |
| D 函数 | GGX/Trowbridge-Reitz | 最广泛使用的 NDF |
| F 函数 | Fresnel-Schlick | 简单高效的近似 |
| G 函数 | Smith's Schlick-GGX | 与 GGX NDF 配合最好 |
| 环境光 | 常量 0.03 | 简单，后续可升级为 IBL |
| Gamma 校正 | Shader 中手动 pow(1/2.2) | 最简单，后续在 Tonemapping 中统一处理 |
| 纹理策略 | 默认纹理方案（无开关） | 消除 Shader 分支，1×1 纹理采样零开销，代码更简洁 |
| 默认 Albedo | (0.8, 0.8, 0.8, 1.0) | 中灰色，与 Unity 默认材质接近 |
| 默认 Roughness | 0.5 | 中等粗糙度，视觉效果适中 |
| 默认 Metallic | 0.0 | 非金属，最常见的材质类型 |
