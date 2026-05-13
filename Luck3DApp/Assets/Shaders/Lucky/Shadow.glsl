// Lucky/Shadow.glsl
// 引擎阴影计算函数库（CSM 版本，支持 Translucent Shadow）
// 依赖：Lucky/Common.glsl（需要在此文件之前 include）

#ifndef LUCKY_SHADOW_GLSL
#define LUCKY_SHADOW_GLSL

#define MAX_CASCADE_COUNT 4

// ---- CSM 阴影参数（由 OpaquePass / TransparentPass 在每帧设置） ----
uniform sampler2DArray u_ShadowMapArray;                            // CSM Texture2DArray 深度纹理
uniform mat4 u_CascadeLightSpaceMatrices[MAX_CASCADE_COUNT];        // 每级 Light Space Matrix
uniform float u_CascadeFarPlanes[MAX_CASCADE_COUNT];                // 每级远平面距离（视图空间）
uniform int u_CascadeCount;                                         // 实际级联数量
uniform mat4 u_CameraViewMatrix;                                    // 相机视图矩阵

// ---- 通用阴影参数 ----
uniform float u_ShadowBias;         // 阴影偏移
uniform float u_ShadowStrength;     // 阴影强度 [0, 1]
uniform int u_ShadowEnabled;        // 阴影开关（0 = 关闭, 1 = 开启）
uniform int u_ShadowType;           // 阴影类型（1 = Hard 硬阴影, 2 = Soft 软阴影 PCF 5x5）

// ---- Translucent Shadow Map 参数（所有级联） ----
uniform sampler2DArray u_TranslucentShadowMap;   // Translucent Shadow Map 颜色纹理（Texture2DArray，所有级联）
uniform int u_TranslucentShadowEnabled;     // 是否启用 Translucent Shadow（0 = 关闭, 1 = 开启）

// ---- 调试 ----
uniform int u_DebugCSMVisualize;    // CSM 级联颜色可视化（0 = 关闭, 1 = 开启）

// ==================== 阴影计算 ====================

/// <summary>
/// 计算动态 Bias：根据法线和光照方向的夹角调整
/// </summary>
float CalcShadowBias(vec3 normal, vec3 lightDir)
{
    float NdotL = dot(normal, lightDir);
    return u_ShadowBias * (1.0 + 9.0 * (1.0 - clamp(NdotL, 0.0, 1.0)));
}

/// <summary>
/// 选择当前片段所属的级联索引
/// </summary>
int SelectCascadeIndex(vec3 worldPos)
{
    vec4 viewPos = u_CameraViewMatrix * vec4(worldPos, 1.0);
    float depth = -viewPos.z;  // 视图空间 z 为负值，取反得到正的深度

    for (int i = 0; i < u_CascadeCount; ++i)
    {
        if (depth < u_CascadeFarPlanes[i])
        {
            return i;
        }
    }
    return u_CascadeCount - 1;
}

/// <summary>
/// 硬阴影计算（单次采样）
/// </summary>
float ShadowCalculationHard(vec3 projCoords, float bias, int cascadeIndex)
{
    float currentDepth = projCoords.z;
    float closestDepth = texture(u_ShadowMapArray, vec3(projCoords.xy, float(cascadeIndex))).r;
    return currentDepth - bias > closestDepth ? 1.0 : 0.0;
}

/// <summary>
/// 软阴影计算（PCF 5x5 采样核）
/// </summary>
float ShadowCalculationSoft(vec3 projCoords, float bias, int cascadeIndex)
{
    float currentDepth = projCoords.z;
    float shadow = 0.0;
    vec2 texelSize = 1.0 / vec2(textureSize(u_ShadowMapArray, 0).xy);
    for (int x = -2; x <= 2; ++x)
    {
        for (int y = -2; y <= 2; ++y)
        {
            float pcfDepth = texture(u_ShadowMapArray, vec3(projCoords.xy + vec2(x, y) * texelSize, float(cascadeIndex))).r;
            shadow += currentDepth - bias > pcfDepth ? 1.0 : 0.0;
        }
    }
    shadow /= 25.0;
    return shadow;
}

/// <summary>
/// 获取级联调试颜色
/// </summary>
vec3 GetCascadeDebugColor(int cascadeIndex)
{
    if (cascadeIndex == 0) return vec3(1.0, 0.0, 0.0);  // 红色
    if (cascadeIndex == 1) return vec3(0.0, 1.0, 0.0);  // 绿色
    if (cascadeIndex == 2) return vec3(0.0, 0.0, 1.0);  // 蓝色
    return vec3(1.0, 1.0, 0.0);                         // 黄色
}

/// <summary>
/// Translucent Shadow Map 硬采样（单次采样）
/// </summary>
vec3 TranslucentShadowHard(vec3 projCoords, int cascadeIndex)
{
    return texture(u_TranslucentShadowMap, vec3(projCoords.xy, float(cascadeIndex))).rgb;
}

/// <summary>
/// Translucent Shadow Map 软采样（PCF 5x5 模糊，与 CSM 深度阴影使用相同的采样核）
/// 对颜色纹理做均值模糊，使透明阴影边缘也有平滑过渡
/// </summary>
vec3 TranslucentShadowSoft(vec3 projCoords, int cascadeIndex)
{
    vec3 colorSum = vec3(0.0);
    vec2 texelSize = 1.0 / vec2(textureSize(u_TranslucentShadowMap, 0).xy);
    for (int x = -2; x <= 2; ++x)
    {
        for (int y = -2; y <= 2; ++y)
        {
            colorSum += texture(u_TranslucentShadowMap, vec3(projCoords.xy + vec2(x, y) * texelSize, float(cascadeIndex))).rgb;
        }
    }
    return colorSum / 25.0;
}

/// <summary>
/// CSM 阴影计算入口（支持 Translucent Shadow）
/// 返回值：vec3, 每个通道 0.0 = 完全在阴影中, 1.0 = 完全不在阴影中
/// </summary>
vec3 ShadowCalculation(vec3 worldPos, vec3 normal, vec3 lightDir)
{
    // 1. 选择级联
    int cascadeIndex = SelectCascadeIndex(worldPos);

    // 2. 变换到该级联的光源空间
    vec4 fragPosLightSpace = u_CascadeLightSpaceMatrices[cascadeIndex] * vec4(worldPos, 1.0);
    vec3 projCoords = fragPosLightSpace.xyz / fragPosLightSpace.w;
    projCoords = projCoords * 0.5 + 0.5;

    // 超出 Shadow Map 范围
    if (projCoords.z > 1.0)
    {
        return vec3(1.0);
    }

    // 3. 动态 Bias
    float bias = CalcShadowBias(normal, lightDir);

    // 4. 根据阴影类型选择计算方式（CSM 深度阴影 + Translucent Shadow Map 同步模糊）
    float shadow = 0.0;
    vec3 translucentColor = vec3(1.0);

    if (u_ShadowType == 1)  // Hard
    {
        shadow = ShadowCalculationHard(projCoords, bias, cascadeIndex);
        if (u_TranslucentShadowEnabled != 0)
        {
            translucentColor = TranslucentShadowHard(projCoords, cascadeIndex);
        }
    }
    else  // Soft (u_ShadowType == 2)
    {
        shadow = ShadowCalculationSoft(projCoords, bias, cascadeIndex);
        if (u_TranslucentShadowEnabled != 0)
        {
            translucentColor = TranslucentShadowSoft(projCoords, cascadeIndex);
        }
    }

    // 5. 应用阴影强度
    shadow *= u_ShadowStrength;
    float baseShadow = 1.0 - shadow;

    // 6. 合成不透明阴影和透明阴影
    // 两者都经过相同的 PCF 模糊，边缘过渡一致，直接用 min() 取更暗的效果
    // 避免双重衰减，同时消除因模糊宽度不一致导致的颜色边界
    return min(vec3(baseShadow), translucentColor);
}

#endif // LUCKY_SHADOW_GLSL
