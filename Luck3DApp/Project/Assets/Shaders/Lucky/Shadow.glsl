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

// ==================== Shadow Atlas 聚光灯阴影 ====================

#define MAX_SPOT_SHADOW_LIGHTS 4

// ---- Shadow Atlas 纹理 ----
uniform sampler2D u_ShadowAtlas;                                        // Shadow Atlas 深度纹理
uniform float u_ShadowAtlasSize;                                        // Atlas 纹理尺寸

// ---- 聚光灯阴影数据 ----
uniform int u_SpotShadowCount;                                          // 投射阴影的聚光灯数量
uniform int u_SpotShadowLightIndex[MAX_SPOT_SHADOW_LIGHTS];             // 对应的光源索引
uniform mat4 u_SpotShadowLightSpaceMatrices[MAX_SPOT_SHADOW_LIGHTS];    // Light Space Matrix
uniform vec4 u_SpotShadowAtlasScaleBias[MAX_SPOT_SHADOW_LIGHTS];        // Atlas UV 变换
uniform float u_SpotShadowBias[MAX_SPOT_SHADOW_LIGHTS];                 // 阴影偏移
uniform float u_SpotShadowStrength[MAX_SPOT_SHADOW_LIGHTS];             // 阴影强度
uniform int u_SpotShadowType[MAX_SPOT_SHADOW_LIGHTS];                   // 阴影类型

/// <summary>
/// 将世界空间坐标变换到 Atlas UV 空间
/// </summary>
vec3 WorldToAtlasUV(vec3 worldPos, mat4 lightSpaceMatrix, vec4 atlasScaleBias)
{
    vec4 lightSpacePos = lightSpaceMatrix * vec4(worldPos, 1.0);
    vec3 projCoords = lightSpacePos.xyz / lightSpacePos.w;
    projCoords = projCoords * 0.5 + 0.5;   // [-1,1] -> [0,1]

    // 应用 Atlas 缩放和偏移
    projCoords.xy = projCoords.xy * atlasScaleBias.xy + atlasScaleBias.zw;

    return projCoords;
}

/// <summary>
/// Atlas 硬阴影采样（单次采样）
/// </summary>
float AtlasShadowSampleHard(vec3 atlasUV, float bias)
{
    float currentDepth = atlasUV.z;
    float closestDepth = texture(u_ShadowAtlas, atlasUV.xy).r;
    return currentDepth - bias > closestDepth ? 1.0 : 0.0;
}

/// <summary>
/// Atlas 软阴影采样（PCF 5×5，带边界保护）
/// </summary>
float AtlasShadowSampleSoft(vec3 atlasUV, float bias, vec4 atlasScaleBias)
{
    float currentDepth = atlasUV.z;
    float shadow = 0.0;
    vec2 atlasTexelSize = 1.0 / vec2(textureSize(u_ShadowAtlas, 0));

    // Tile 边界（留 1 像素边距防止采样溢出）
    vec2 tileMin = atlasScaleBias.zw + atlasTexelSize;
    vec2 tileMax = atlasScaleBias.zw + atlasScaleBias.xy - atlasTexelSize;

    for (int x = -2; x <= 2; ++x)
    {
        for (int y = -2; y <= 2; ++y)
        {
            vec2 sampleUV = clamp(
                atlasUV.xy + vec2(x, y) * atlasTexelSize,
                tileMin, tileMax
            );
            float pcfDepth = texture(u_ShadowAtlas, sampleUV).r;
            shadow += currentDepth - bias > pcfDepth ? 1.0 : 0.0;
        }
    }
    return shadow / 25.0;
}

/// <summary>
/// 计算聚光灯阴影因子
/// 返回 float：1.0 = 完全不在阴影中，0.0 = 完全在阴影中
/// </summary>
float CalcSpotLightShadow(int shadowIndex, vec3 worldPos, vec3 normal, vec3 lightDir)
{
    mat4 lightSpaceMatrix = u_SpotShadowLightSpaceMatrices[shadowIndex];
    vec4 atlasScaleBias = u_SpotShadowAtlasScaleBias[shadowIndex];

    vec3 atlasUV = WorldToAtlasUV(worldPos, lightSpaceMatrix, atlasScaleBias);

    // 超出 Shadow Map 范围（透视投影的 z 可能 > 1 或 < 0）
    if (atlasUV.z > 1.0 || atlasUV.z < 0.0)
    {
        return 1.0;
    }

    // 检查是否在 Tile 范围内（防止采样到相邻 Tile）
    vec2 tileMin = atlasScaleBias.zw;
    vec2 tileMax = atlasScaleBias.zw + atlasScaleBias.xy;
    if (atlasUV.x < tileMin.x || atlasUV.x > tileMax.x ||
        atlasUV.y < tileMin.y || atlasUV.y > tileMax.y)
    {
        return 1.0;
    }

    // 动态 Bias
    float NdotL = dot(normal, lightDir);
    float bias = u_SpotShadowBias[shadowIndex] * (1.0 + 9.0 * (1.0 - clamp(NdotL, 0.0, 1.0)));

    // 根据阴影类型选择采样方式
    float shadow = 0.0;
    if (u_SpotShadowType[shadowIndex] == 1)     // Hard
    {
        shadow = AtlasShadowSampleHard(atlasUV, bias);
    }
    else    // Soft
    {
        shadow = AtlasShadowSampleSoft(atlasUV, bias, atlasScaleBias);
    }

    shadow *= u_SpotShadowStrength[shadowIndex];
    return 1.0 - shadow;
}

/// <summary>
/// 获取聚光灯阴影因子（供 Lighting.glsl 调用）
/// lightIndex: 聚光灯在 u_Lights.SpotLights[] 中的索引
/// </summary>
float GetSpotLightShadow(int lightIndex, vec3 worldPos, vec3 normal, vec3 lightDir)
{
    // 查找该聚光灯对应的阴影索引
    for (int i = 0; i < u_SpotShadowCount; ++i)
    {
        if (u_SpotShadowLightIndex[i] == lightIndex)
        {
            return CalcSpotLightShadow(i, worldPos, normal, lightDir);
        }
    }
    return 1.0;  // 该光源不投射阴影
}

// ==================== Shadow Atlas 点光源阴影 ====================

#define MAX_POINT_SHADOW_LIGHTS 2

// ---- 点光源阴影数据 ----
uniform int u_PointShadowCount;                                                     // 投射阴影的点光源数量
uniform int u_PointShadowLightIndex[MAX_POINT_SHADOW_LIGHTS];                       // 对应的光源索引
uniform mat4 u_PointShadowLightSpaceMatrices[MAX_POINT_SHADOW_LIGHTS * 6];          // 6 面 Light Space Matrix
uniform vec4 u_PointShadowAtlasScaleBias[MAX_POINT_SHADOW_LIGHTS * 6];              // 6 面 Atlas UV 变换
uniform float u_PointShadowBias[MAX_POINT_SHADOW_LIGHTS];                           // 阴影偏移
uniform float u_PointShadowStrength[MAX_POINT_SHADOW_LIGHTS];                       // 阴影强度
uniform int u_PointShadowType[MAX_POINT_SHADOW_LIGHTS];                             // 阴影类型
uniform float u_PointShadowFarPlane[MAX_POINT_SHADOW_LIGHTS];                       // 远平面（= Range）
uniform vec3 u_PointShadowLightPos[MAX_POINT_SHADOW_LIGHTS];                        // 光源位置

/// <summary>
/// 根据光源到片段的方向向量选择 Cubemap 面索引
/// 选择绝对值最大的轴作为主轴
/// </summary>
int SelectPointLightFace(vec3 fragToLight)
{
    vec3 absDir = abs(fragToLight);

    if (absDir.x >= absDir.y && absDir.x >= absDir.z)
    {
        return fragToLight.x > 0.0 ? 0 : 1;   // +X or -X
    }
    else if (absDir.y >= absDir.x && absDir.y >= absDir.z)
    {
        return fragToLight.y > 0.0 ? 2 : 3;   // +Y or -Y
    }
    else
    {
        return fragToLight.z > 0.0 ? 4 : 5;   // +Z or -Z
    }
}

/// <summary>
/// 计算点光源阴影因子
/// 使用线性深度比较（距离/远平面）
/// 返回 float：1.0 = 完全不在阴影中，0.0 = 完全在阴影中
/// </summary>
float CalcPointLightShadow(int shadowIndex, vec3 worldPos, vec3 lightPos, vec3 normal)
{
    vec3 fragToLight = worldPos - lightPos;
    int faceIndex = SelectPointLightFace(fragToLight);

    int tileIndex = shadowIndex * 6 + faceIndex;
    mat4 lightSpaceMatrix = u_PointShadowLightSpaceMatrices[tileIndex];
    vec4 atlasScaleBias = u_PointShadowAtlasScaleBias[tileIndex];

    // 变换到 Atlas UV 空间
    vec3 atlasUV = WorldToAtlasUV(worldPos, lightSpaceMatrix, atlasScaleBias);

    // 超出范围检查
    if (atlasUV.z > 1.0 || atlasUV.z < 0.0)
    {
        return 1.0;
    }

    // 检查是否在 Tile 范围内（防止采样到相邻 Tile）
    vec2 tileMin = atlasScaleBias.zw;
    vec2 tileMax = atlasScaleBias.zw + atlasScaleBias.xy;
    if (atlasUV.x < tileMin.x || atlasUV.x > tileMax.x ||
        atlasUV.y < tileMin.y || atlasUV.y > tileMax.y)
    {
        return 1.0;
    }

    // 计算线性深度（当前片段到光源的距离 / 远平面）
    float currentLinearDepth = length(worldPos - lightPos) / u_PointShadowFarPlane[shadowIndex];

    // 动态 Bias（点光源需要更大的 bias）
    vec3 lightDir = normalize(lightPos - worldPos);
    float NdotL = dot(normal, lightDir);
    float bias = u_PointShadowBias[shadowIndex] * (1.0 + 9.0 * (1.0 - clamp(NdotL, 0.0, 1.0)));

    // 读取 Atlas 中存储的线性深度
    float closestLinearDepth = texture(u_ShadowAtlas, atlasUV.xy).r;

    // 根据阴影类型选择采样方式
    float shadow = 0.0;
    if (u_PointShadowType[shadowIndex] == 1)    // Hard
    {
        shadow = currentLinearDepth - bias > closestLinearDepth ? 1.0 : 0.0;
    }
    else    // Soft（PCF 5×5）
    {
        vec2 atlasTexelSize = 1.0 / vec2(textureSize(u_ShadowAtlas, 0));
        vec2 pcfTileMin = atlasScaleBias.zw + atlasTexelSize;
        vec2 pcfTileMax = atlasScaleBias.zw + atlasScaleBias.xy - atlasTexelSize;

        for (int x = -2; x <= 2; ++x)
        {
            for (int y = -2; y <= 2; ++y)
            {
                vec2 sampleUV = clamp(
                    atlasUV.xy + vec2(x, y) * atlasTexelSize,
                    pcfTileMin, pcfTileMax
                );
                float pcfDepth = texture(u_ShadowAtlas, sampleUV).r;
                shadow += currentLinearDepth - bias > pcfDepth ? 1.0 : 0.0;
            }
        }
        shadow /= 25.0;
    }

    shadow *= u_PointShadowStrength[shadowIndex];
    return 1.0 - shadow;
}

/// <summary>
/// 获取点光源阴影因子（供 Lighting.glsl 调用）
/// lightIndex: 点光源在 u_Lights.PointLights[] 中的索引
/// </summary>
float GetPointLightShadow(int lightIndex, vec3 worldPos, vec3 lightPos, vec3 normal)
{
    for (int i = 0; i < u_PointShadowCount; ++i)
    {
        if (u_PointShadowLightIndex[i] == lightIndex)
        {
            return CalcPointLightShadow(i, worldPos, lightPos, normal);
        }
    }
    return 1.0;
}

#endif // LUCKY_SHADOW_GLSL
