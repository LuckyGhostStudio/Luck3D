#version 450 core

layout(location = 0) out vec4 o_Color;  // 颜色缓冲区 0 输出颜色

// 相机 Uniform 缓冲区
layout(std140, binding = 0) uniform Camera
{
    mat4 ViewProjectionMatrix;
    vec3 Position;  // 相机位置（用于计算视线向量）
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

// ---- 常量 ----
const float PI = 3.14159265359;
const float AMBIENT_STRENGTH = 0.03;  // 常量环境光强度（无 IBL 时的替代方案）

// ==================== PBR 核心函数 ====================

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
        vec3 kS = F;                                    // 镜面反射比例 = Fresnel
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