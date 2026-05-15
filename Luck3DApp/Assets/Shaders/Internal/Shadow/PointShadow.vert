#version 450 core

// 点光源阴影顶点着色器
// 输出世界空间位置，供片段着色器计算线性深度

layout(location = 0) in vec3 a_Position;    // 位置
layout(location = 1) in vec4 a_Color;       // 颜色（未使用）
layout(location = 2) in vec3 a_Normal;      // 法线（未使用）
layout(location = 3) in vec2 a_TexCoord;    // 纹理坐标（未使用）
layout(location = 4) in vec4 a_Tangent;     // 切线（未使用）

layout(location = 0) out vec3 v_WorldPos;   // 世界空间位置（传递给 Fragment Shader）

uniform mat4 u_LightSpaceMatrix;            // 光源空间 VP 矩阵（当前面的投影 × 视图）
uniform mat4 u_ObjectToWorldMatrix;         // 模型变换矩阵

void main()
{
    vec4 worldPos = u_ObjectToWorldMatrix * vec4(a_Position, 1.0);
    v_WorldPos = worldPos.xyz;
    gl_Position = u_LightSpaceMatrix * worldPos;
}
