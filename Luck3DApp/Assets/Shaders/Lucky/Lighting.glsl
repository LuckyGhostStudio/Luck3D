// Lucky/Lighting.glsl
// 引擎光照计算函数库（PBR 工作流）
// 依赖：Lucky/Common.glsl、Lucky/Shadow.glsl（需要在此文件之前 include）

#ifndef LUCKY_LIGHTING_GLSL
#define LUCKY_LIGHTING_GLSL

// ==================== PBR BRDF 核心函数 ====================

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

// ==================== 光源计算函数 ====================

// 计算方向光的 PBR 贡献
vec3 CalcDirectionalLight(DirectionalLight light, vec3 N, vec3 V, vec3 albedo, float metallic, float roughness, vec3 F0)
{
    vec3 L = normalize(-light.Direction);
    vec3 H = normalize(V + L);

    vec3 radiance = light.Color * light.Intensity;

    float D = DistributionGGX(N, H, roughness);
    vec3  F = FresnelSchlick(max(dot(H, V), 0.0), F0);
    float G = GeometrySmith(N, V, L, roughness);

    vec3 numerator = D * F * G;
    float denominator = 4.0 * max(dot(N, V), 0.0) * max(dot(N, L), 0.0) + 0.0001;
    vec3 specular = numerator / denominator;

    vec3 kD = (vec3(1.0) - F) * (1.0 - metallic);
    float NdotL = max(dot(N, L), 0.0);

    return (kD * albedo / PI + specular) * radiance * NdotL;
}

// 计算点光源的 PBR 贡献
vec3 CalcPointLight(PointLight light, vec3 N, vec3 V, vec3 worldPos, vec3 albedo, float metallic, float roughness, vec3 F0)
{
    vec3 L = light.Position - worldPos;
    float distance = length(L);

    // 超出范围则不计算
    if (distance > light.Range)
    return vec3(0.0);

    L = normalize(L);
    vec3 H = normalize(V + L);

    // 距离衰减（平滑衰减到 Range 边界）
    float attenuation = clamp(1.0 - (distance / light.Range), 0.0, 1.0);
    attenuation *= attenuation;  // 二次衰减，更自然

    vec3 radiance = light.Color * light.Intensity * attenuation;

    float D = DistributionGGX(N, H, roughness);
    vec3  F = FresnelSchlick(max(dot(H, V), 0.0), F0);
    float G = GeometrySmith(N, V, L, roughness);

    vec3 numerator = D * F * G;
    float denominator = 4.0 * max(dot(N, V), 0.0) * max(dot(N, L), 0.0) + 0.0001;
    vec3 specular = numerator / denominator;

    vec3 kD = (vec3(1.0) - F) * (1.0 - metallic);
    float NdotL = max(dot(N, L), 0.0);

    return (kD * albedo / PI + specular) * radiance * NdotL;
}

// 计算聚光灯的 PBR 贡献
vec3 CalcSpotLight(SpotLight light, vec3 N, vec3 V, vec3 worldPos, vec3 albedo, float metallic, float roughness, vec3 F0)
{
    vec3 L = light.Position - worldPos;
    float distance = length(L);

    if (distance > light.Range)
    return vec3(0.0);

    L = normalize(L);
    vec3 H = normalize(V + L);

    // 锥形衰减
    float theta = dot(L, normalize(-light.Direction));
    float epsilon = light.InnerCutoff - light.OuterCutoff;
    float spotIntensity = clamp((theta - light.OuterCutoff) / epsilon, 0.0, 1.0);

    // 距离衰减
    float attenuation = clamp(1.0 - (distance / light.Range), 0.0, 1.0);
    attenuation *= attenuation;

    vec3 radiance = light.Color * light.Intensity * attenuation * spotIntensity;

    float D = DistributionGGX(N, H, roughness);
    vec3  F = FresnelSchlick(max(dot(H, V), 0.0), F0);
    float G = GeometrySmith(N, V, L, roughness);

    vec3 numerator = D * F * G;
    float denominator = 4.0 * max(dot(N, V), 0.0) * max(dot(N, L), 0.0) + 0.0001;
    vec3 specular = numerator / denominator;

    vec3 kD = (vec3(1.0) - F) * (1.0 - metallic);
    float NdotL = max(dot(N, L), 0.0);

    return (kD * albedo / PI + specular) * radiance * NdotL;
}

// ==================== 便捷函数 ====================

/// <summary>
/// 计算所有光源的 PBR 贡献（含阴影）
/// 这是一个一站式函数，用户只需调用此函数即可获得完整的光照结果
/// 需要同时 #include "Lucky/Shadow.glsl"
/// </summary>
/// <param name="N">世界空间法线（归一化）</param>
/// <param name="V">视线方向（归一化，从片元指向相机）</param>
/// <param name="worldPos">片元世界空间位置</param>
/// <param name="albedo">基础颜色</param>
/// <param name="metallic">金属度</param>
/// <param name="roughness">粗糙度</param>
/// <param name="F0">基础反射率</param>
/// <returns>所有光源的直接光照贡献总和（含阴影衰减）</returns>
vec3 CalcAllLights(vec3 N, vec3 V, vec3 worldPos, vec3 albedo, float metallic, float roughness, vec3 F0)
{
    vec3 Lo = vec3(0.0);

    // 方向光
    for (int i = 0; i < u_Lights.DirectionalLightCount; ++i)
    {
        vec3 contribution = CalcDirectionalLight(u_Lights.DirectionalLights[i], N, V, albedo, metallic, roughness, F0);
        
        // 仅对第一个方向光应用阴影（当前只支持单光源阴影）
        if (i == 0 && u_ShadowEnabled != 0)
        {
            vec3 lightDir = normalize(-u_Lights.DirectionalLights[i].Direction);
            float shadow = ShadowCalculation(worldPos, N, lightDir);
            contribution *= shadow;
        }
        
        Lo += contribution;
    }

    // 点光源
    for (int i = 0; i < u_Lights.PointLightCount; ++i)
    {
        Lo += CalcPointLight(u_Lights.PointLights[i], N, V, worldPos, albedo, metallic, roughness, F0);
    }

    // 聚光灯
    for (int i = 0; i < u_Lights.SpotLightCount; ++i)
    {
        Lo += CalcSpotLight(u_Lights.SpotLights[i], N, V, worldPos, albedo, metallic, roughness, F0);
    }

    return Lo;
}

#endif // LUCKY_LIGHTING_GLSL
