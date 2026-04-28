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

// ---- PBR 材质参数 ----
uniform vec4  u_Albedo;             // 基础颜色
uniform float u_Metallic;           // 金属度
uniform float u_Roughness;          // 粗糙度
uniform float u_AO;                 // 环境光遮蔽
uniform vec3  u_Emission;           // 自发光颜色
uniform float u_EmissionIntensity;  // 自发光强度

// ---- PBR 纹理（无贴图时引擎绑定默认纹理） ----
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

// ==================== 法线获取 ====================

vec3 GetNormal()
{
    // 采样法线贴图（无贴图时绑定默认法线纹理 (0.5, 0.5, 1.0)，解码后为 (0,0,1)）
    vec3 normalMapSample = texture(u_NormalMap, v_Input.TexCoord).rgb;
    // [0,1] → [-1,1]
    normalMapSample = normalMapSample * 2.0 - 1.0;
    // 转换到世界空间
    return normalize(v_Input.TBN * normalMapSample);
}

void main()
{
    // ---- 采样材质参数（无贴图时默认纹理返回 1.0） ----
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
    
    // ---- 光照计算（一行调用，引擎自动处理所有光源 + 阴影） ----
    vec3 Lo = CalcAllLights(N, V, v_Input.WorldPos, albedo, metallic, roughness, F0);

    // ---- 环境光（简单常量，无 IBL） ----
    vec3 ambient = vec3(AMBIENT_STRENGTH) * albedo * ao;

    // ---- 最终颜色 = 环境光 + 直接光照 + 自发光 ----
    vec3 color = ambient + Lo + emission;

    // ---- Gamma 校正（线性空间 → sRGB） ----
    color = pow(color, vec3(1.0 / 2.2));

    o_Color = vec4(color, alpha);
}