// Luck3DApp/Assets/Shaders/Internal/Debug/DebugCSMVisualize.frag
// 调试可视化：CSM 级联颜色叠加（HDR 空间）
// 输入：场景颜色 + 场景深度
// 输出：叠加级联色后的颜色（只对场景几何区域生效，跳过远平面 / 天空盒）

#version 450 core

#include "../../Lucky/Common.glsl"     // 引入 Camera UBO（含 InvProjectionMatrix）

layout(location = 0) out vec4 o_Color;

in vec2 v_TexCoord;

#define MAX_CASCADE_COUNT 4

uniform sampler2D u_SceneColor;                                 // HDR 场景颜色
uniform sampler2D u_SceneDepth;                                 // 场景深度（HDR FBO 深度附件）
uniform int   u_CascadeCount;                                   // 实际级联数量
uniform float u_DebugCascadeFarPlanes[MAX_CASCADE_COUNT];       // 每级远平面距离（视图空间）

/// <summary>
/// 由屏幕空间 UV 与深度重建视图空间 Z（绝对值，远处更大）
/// 与 Lucky/Shadow.glsl::SelectCascadeIndex 中 -viewPos.z 等价
/// </summary>
float ReconstructViewDepth(vec2 uv, float depth)
{
    // NDC: x,y in [-1,1]; z = depth * 2 - 1
    vec4 ndc = vec4(uv * 2.0 - 1.0, depth * 2.0 - 1.0, 1.0);
    vec4 viewPos = u_Camera.InvProjectionMatrix * ndc;
    viewPos.xyz /= viewPos.w;
    return -viewPos.z;  // OpenGL 视图空间 z 为负值，取反得到正深度
}

/// <summary>
/// 选择级联（与 Lucky/Shadow.glsl::SelectCascadeIndex 同算法）
/// </summary>
int SelectCascadeIndex(float viewDepth)
{
    for (int i = 0; i < u_CascadeCount; ++i)
    {
        if (viewDepth < u_DebugCascadeFarPlanes[i])
        {
            return i;
        }
    }
    return u_CascadeCount - 1;
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

void main()
{
    vec3 sceneColor = texture(u_SceneColor, v_TexCoord).rgb;
    float depth = texture(u_SceneDepth, v_TexCoord).r;

    // 跳过远平面（天空盒等无几何区域）
    if (depth >= 1.0)
    {
        o_Color = vec4(sceneColor, 1.0);
        return;
    }

    float viewDepth = ReconstructViewDepth(v_TexCoord, depth);
    int cascadeIndex = SelectCascadeIndex(viewDepth);
    vec3 cascadeColor = GetCascadeDebugColor(cascadeIndex);

    o_Color = vec4(mix(sceneColor, cascadeColor, 0.3), 1.0);
}
