// Lucky/Common.glsl
// 引擎公共定义：Camera UBO、Lights UBO、光源结构体、常量
// 所有用户 Shader 都应 #include 此文件

#ifndef LUCKY_COMMON_GLSL
#define LUCKY_COMMON_GLSL

// ---- 常量 ----
#define MAX_DIRECTIONAL_LIGHTS 4
#define MAX_POINT_LIGHTS 8
#define MAX_SPOT_LIGHTS 4

const float PI = 3.14159265359;
const float AMBIENT_STRENGTH = 0.03;  // 常量环境光强度（无 IBL 时的替代方案）

// ---- 光源结构体 ----

struct DirectionalLight
{
    vec3 Direction;
    float Intensity;
    vec3 Color;
    float _padding;
};

struct PointLight
{
    vec3 Position;
    float Intensity;
    vec3 Color;
    float Range;
};

struct SpotLight
{
    vec3 Position;
    float Intensity;
    vec3 Direction;
    float Range;
    vec3 Color;
    float InnerCutoff;  // cos(innerAngle)
    float OuterCutoff;  // cos(outerAngle)
    float _pading0;     // 填充对齐到 16 字节（不使用数组，避免 std140 数组元素 16 字节对齐）
    float _pading1;
    float _pading2;
};

// ---- 相机 Uniform 缓冲区 ----
layout(std140, binding = 0) uniform Camera
{
    mat4 ViewProjectionMatrix;
    vec3 Position;  // 相机位置（用于计算视线向量）
} u_Camera;

// ---- 光照 Uniform 缓冲区 ----
layout(std140, binding = 1) uniform Lights
{
    int DirectionalLightCount;
    int PointLightCount;
    int SpotLightCount;
    float _padding;     // 填充对齐到 16 字节

    DirectionalLight DirectionalLights[MAX_DIRECTIONAL_LIGHTS];
    PointLight PointLights[MAX_POINT_LIGHTS];
    SpotLight SpotLights[MAX_SPOT_LIGHTS];
} u_Lights;

#endif // LUCKY_COMMON_GLSL
