#version 450 core

layout(location = 0) in vec3 a_Position;    // 位置
layout(location = 1) in vec4 a_Color;       // 颜色
layout(location = 2) in vec3 a_Normal;      // 法线
layout(location = 3) in vec2 a_TexCoord;    // 纹理坐标
layout(location = 4) in vec4 a_Tangent;     // 切线 + 手性

// 相机 Uniform 缓冲区
layout(std140, binding = 0) uniform Camera
{
    mat4 ViewProjectionMatrix;
    vec3 Position;  // 相机位置（用于计算视线向量）
} u_Camera;

// 模型矩阵（用于变换法向量）
uniform mat4 u_ObjectToWorldMatrix;

void main()
{
    // 计算世界空间位置
    vec4 worldPos = u_ObjectToWorldMatrix * vec4(a_Position, 1.0);

    gl_Position = u_Camera.ViewProjectionMatrix * worldPos;
}