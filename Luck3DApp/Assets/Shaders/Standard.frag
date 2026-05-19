#version 450 core

layout(location = 0) out vec4 o_Color;  // 颜色缓冲区 0 输出颜色

// ---- 引擎公共库 ----
#include "Lucky/Common.glsl"
#include "Lucky/Shadow.glsl"
#include "Lucky/Lighting.glsl"

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

// ==================== PBR 材质参数 ====================

uniform vec4  u_Albedo;             // 基础颜色
uniform float u_Metallic;           // 金属度
uniform float u_Roughness;          // 粗糙度
uniform float u_AO;                 // 环境光遮蔽
uniform vec3  u_Emission;           // 自发光颜色
uniform float u_EmissionIntensity;  // 自发光强度

// ---- 视差遮挡贴图（POM） ----
uniform float u_HeightScale;        // 视差强度（默认 0.0 = 关闭，对应 Unity _Parallax）

// ---- Detail 强度 ----
uniform float u_DetailNormalScale;  // Detail 法线强度（默认 1.0）

// ==================== 主纹理（无贴图时引擎绑定默认纹理） ====================

// @default: white
uniform sampler2D u_AlbedoMap;

// @default: normal
uniform sampler2D u_NormalMap;

// @default: white
uniform sampler2D u_MetallicMap;

// @default: white
uniform sampler2D u_RoughnessMap;

// @default: white
uniform sampler2D u_AOMap;

// @default: white
uniform sampler2D u_EmissionMap;

// @default: black （高度 = 0，视差自动失效）
uniform sampler2D u_HeightMap;

// ==================== Detail 纹理 ====================

// @default: white （Mask = 0 时不会被使用）
uniform sampler2D u_DetailAlbedoMap;

// @default: normal
uniform sampler2D u_DetailNormalMap;

// @default: black （Detail 默认完全不生效）
uniform sampler2D u_DetailMask;

// ==================== 工具函数 ====================

/// <summary>
/// 视差遮挡贴图（POM, Parallax Occlusion Mapping）
/// 使用 Steep Parallax + 二分细化，质量与 Unity HDRP / Unreal 标准实现一致
/// </summary>
/// <param name="uv">原始 UV</param>
/// <param name="viewDirTS">切线空间视方向（指向相机）</param>
/// <returns>视差校正后的 UV</returns>
vec2 ParallaxOcclusionMapping(vec2 uv, vec3 viewDirTS)
{
    // 视差强度过小时直接跳过（避免不必要的纹理采样）
    if (u_HeightScale <= 0.0001)
    {
        return uv;
    }

    // POM 层数（引擎实现细节，不暴露给材质）
    // 后续若需要按全局质量等级调整，可改为引擎注入的内部 uniform
    const float MIN_LAYERS = 8.0;
    const float MAX_LAYERS = 32.0;

    // 根据视角倾斜度动态调整层数（垂直看用少层数，斜视用多层数）
    float layerCount = mix(MAX_LAYERS, MIN_LAYERS, abs(viewDirTS.z));
    float layerDepth = 1.0 / layerCount;

    // 每层 UV 偏移量（基于视方向 xy 分量，z 越小 = 越斜，偏移越大）
    vec2 P = viewDirTS.xy / max(viewDirTS.z, 0.1) * u_HeightScale;
    vec2 deltaUV = P / layerCount;

    // ---- Steep Parallax：逐层向下找到首个被遮挡的层 ----
    float currentLayerDepth = 0.0;
    vec2 currentUV = uv;
    float currentDepth = texture(u_HeightMap, currentUV).r;

    while (currentLayerDepth < currentDepth)
    {
        currentUV -= deltaUV;
        currentDepth = texture(u_HeightMap, currentUV).r;
        currentLayerDepth += layerDepth;
    }

    // ---- 二分细化：在被穿透的层与上一层之间精确插值 ----
    vec2 prevUV = currentUV + deltaUV;
    float afterDepth = currentDepth - currentLayerDepth;
    float beforeDepth = texture(u_HeightMap, prevUV).r - currentLayerDepth + layerDepth;

    float weight = afterDepth / (afterDepth - beforeDepth);
    return mix(currentUV, prevUV, weight);
}

/// <summary>
/// 解码法线贴图（[0,1] → [-1,1]）
/// 可选的强度缩放：缩放 XY 分量保留 Z 主导，避免法线失真
/// </summary>
vec3 DecodeNormal(sampler2D normalMap, vec2 uv)
{
    vec3 n = texture(normalMap, uv).rgb * 2.0 - 1.0;
    return normalize(n);
}

vec3 DecodeNormalScaled(sampler2D normalMap, vec2 uv, float scale)
{
    vec3 n = texture(normalMap, uv).rgb * 2.0 - 1.0;
    n.xy *= scale;
    return normalize(n);
}

/// <summary>
/// 计算最终法线（主法线 + Detail 法线 + DetailMask 混合）
/// 使用 Whiteout 法线混合（Unity HDRP / Unreal 标准方案）
/// </summary>
vec3 GetFinalNormal(vec2 uv, float detailMask)
{
    vec3 nMain = DecodeNormal(u_NormalMap, uv);
    vec3 nDetail = DecodeNormalScaled(u_DetailNormalMap, uv, u_DetailNormalScale);

    // Whiteout 混合：xy 相加，z 相乘（Mask 控制 Detail 生效程度）
    vec3 nCombined = vec3(
        nMain.xy + nDetail.xy * detailMask,
        nMain.z * mix(1.0, nDetail.z, detailMask)
    );
    nCombined = normalize(nCombined);

    // 切线空间 → 世界空间
    return normalize(v_Input.TBN * nCombined);
}

void main()
{
    // ---- 视方向（世界空间） ----
    vec3 V = normalize(u_Camera.Position - v_Input.WorldPos);

    // ---- 切线空间视方向（用于 POM）：TBN 是正交矩阵，转置 = 逆 ----
    vec3 viewDirTS = normalize(transpose(v_Input.TBN) * V);

    // ---- 视差校正后的 UV（HeightScale = 0 时等于原始 UV） ----
    vec2 uv = ParallaxOcclusionMapping(v_Input.TexCoord, viewDirTS);

    // ---- 采样 Detail Mask（决定 Detail 层混合权重） ----
    float detailMask = texture(u_DetailMask, uv).r;

    // ---- 采样基础颜色 ----
    vec4 albedo4 = u_Albedo * texture(u_AlbedoMap, uv);

    // ---- Detail Albedo 混合（Overlay 公式：中性灰 0.5 时不改变 base） ----
    vec3 detailColor = texture(u_DetailAlbedoMap, uv).rgb;
    vec3 albedo = albedo4.rgb * mix(vec3(1.0), detailColor * 2.0, detailMask);
    float alpha = albedo4.a;

    // ---- 采样其它 PBR 通道 ----
    float metallic = u_Metallic * texture(u_MetallicMap, uv).r;

    float roughness = u_Roughness * texture(u_RoughnessMap, uv).r;
    roughness = max(roughness, 0.04);   // 限制最小粗糙度，避免除以零

    float ao = u_AO * texture(u_AOMap, uv).r;

    vec3 emission = u_Emission * u_EmissionIntensity * texture(u_EmissionMap, uv).rgb;

    // ---- 计算最终法线（主 + Detail，按 Mask 混合） ----
    vec3 N = GetFinalNormal(uv, detailMask);

    // ---- 计算 F0（基础反射率）：非金属 ≈ 0.04，金属 = albedo ----
    vec3 F0 = vec3(0.04);
    F0 = mix(F0, albedo, metallic);

    // ---- 光照计算（引擎一站式 API：直接光 + 阴影 + 环境光自动处理） ----
    vec3 lighting = CalcDirectAndIndirectLighting(N, V, v_Input.WorldPos, albedo, metallic, roughness, F0, ao);

    // ---- 最终颜色 = 光照（直接 + 间接） + 自发光 ----
    vec3 color = lighting + emission;

    o_Color = vec4(color, alpha);
}