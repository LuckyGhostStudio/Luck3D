// Lucky/Shadow.glsl
// 引擎阴影计算函数库（支持 Translucent Shadow）
// 依赖：Lucky/Common.glsl（需要在此文件之前 include）

#ifndef LUCKY_SHADOW_GLSL
#define LUCKY_SHADOW_GLSL

// ---- 阴影参数（由 OpaquePass / TransparentPass 在每帧设置） ----
uniform sampler2D u_ShadowMap;      // Shadow Map 深度纹理
uniform mat4 u_LightSpaceMatrix;    // 光源空间 VP 矩阵
uniform float u_ShadowBias;         // 阴影偏移
uniform float u_ShadowStrength;     // 阴影强度 [0, 1]
uniform int u_ShadowEnabled;        // 阴影开关（0 = 关闭, 1 = 开启）
uniform int u_ShadowType;           // 阴影类型（1 = Hard 硬阴影, 2 = Soft 软阴影 PCF 3x3）

// ---- Translucent Shadow Map 参数 ----
uniform sampler2D u_TranslucentShadowMap;   // Translucent Shadow Map 颜色纹理
uniform int u_TranslucentShadowEnabled;     // 是否启用 Translucent Shadow（0 = 关闭, 1 = 开启）

// ==================== 阴影计算 ====================

/// <summary>
/// 计算动态 Bias：根据法线和光照方向的夹角调整
/// u_ShadowBias 作为基础 bias, 当法线与光照方向接近垂直时放大到 10 倍
/// </summary>
float CalcShadowBias(vec3 normal, vec3 lightDir)
{
    float NdotL = dot(normal, lightDir);
    // 基础 bias * 动态缩放因子（最小 1 倍, 最大 10 倍）
    return u_ShadowBias * (1.0 + 9.0 * (1.0 - clamp(NdotL, 0.0, 1.0)));
}

/// <summary>
/// 硬阴影计算（单次采样）
/// 返回值：0.0 = 完全不在阴影中, 1.0 = 完全在阴影中
/// </summary>
float ShadowCalculationHard(vec3 projCoords, float bias)
{
    float currentDepth = projCoords.z;
    float closestDepth = texture(u_ShadowMap, projCoords.xy).r;
    return currentDepth - bias > closestDepth ? 1.0 : 0.0;
}

/// <summary>
/// 软阴影计算（PCF 5x5 采样核）
/// 返回值：0.0 = 完全不在阴影中, 1.0 = 完全在阴影中
/// </summary>
float ShadowCalculationSoft(vec3 projCoords, float bias)
{
    float currentDepth = projCoords.z;
    float shadow = 0.0;
    vec2 texelSize = 1.0 / textureSize(u_ShadowMap, 0);
    for (int x = -2; x <= 2; ++x)
    {
        for (int y = -2; y <= 2; ++y)
        {
            float pcfDepth = texture(u_ShadowMap, projCoords.xy + vec2(x, y) * texelSize).r;
            shadow += currentDepth - bias > pcfDepth ? 1.0 : 0.0;
        }
    }
    shadow /= 25.0;
    return shadow;
}

/// <summary>
/// 阴影计算入口（支持 Translucent Shadow）
/// 返回值：vec3, 每个通道 0.0 = 完全在阴影中, 1.0 = 完全不在阴影中
/// 不启用 Translucent Shadow 时, 三个通道值相同（等价于原来的 float）
/// </summary>
vec3 ShadowCalculation(vec3 worldPos, vec3 normal, vec3 lightDir)
{
    // 将世界空间坐标变换到光源空间
    vec4 fragPosLightSpace = u_LightSpaceMatrix * vec4(worldPos, 1.0);
    // 透视除法
    vec3 projCoords = fragPosLightSpace.xyz / fragPosLightSpace.w;
    // [-1, 1] -> [0, 1]
    projCoords = projCoords * 0.5 + 0.5;

    // 超出 Shadow Map 范围的区域不在阴影中
    if (projCoords.z > 1.0)
    {
        return vec3(1.0);
    }

    // 动态 Bias
    float bias = CalcShadowBias(normal, lightDir);

    // 根据阴影类型选择计算方式
    float shadow = 0.0;
    if (u_ShadowType == 1)  // Hard
    {
        shadow = ShadowCalculationHard(projCoords, bias);
    }
    else  // Soft (u_ShadowType == 2)
    {
        shadow = ShadowCalculationSoft(projCoords, bias);
    }

    // 应用阴影强度
    shadow *= u_ShadowStrength;

    // 基础阴影因子（标量）
    float baseShadow = 1.0 - shadow;

    // Translucent Shadow 颜色衰减
    if (u_TranslucentShadowEnabled != 0)
    {
        vec3 translucentColor = texture(u_TranslucentShadowMap, projCoords.xy).rgb;
        // 仅在阴影区域内应用颜色衰减:
        // shadow=0 (不在阴影中) -> mix 返回 vec3(1.0), 不受 Translucent 影响
        // shadow=1 (完全在阴影中) -> mix 返回 translucentColor, 应用完整颜色衰减
        // 这样可以避免光源前方的物体被后方透明物体的颜色衰减错误影响
        return mix(vec3(1.0), translucentColor, shadow);
    }

    return vec3(baseShadow);
}

#endif // LUCKY_SHADOW_GLSL
