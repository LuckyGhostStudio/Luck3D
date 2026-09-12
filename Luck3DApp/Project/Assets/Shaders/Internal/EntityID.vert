#version 450 core

layout(location = 0) in vec3 a_Position;    // 位置

// 相机 Uniform 缓冲区（与 Standard.vert 共享）
layout(std140, binding = 0) uniform Camera
{
    mat4 ViewProjectionMatrix;
    vec3 Position;
} u_Camera;

// 模型矩阵
uniform mat4 u_ObjectToWorldMatrix;

void main()
{
    vec4 worldPos = u_ObjectToWorldMatrix * vec4(a_Position, 1.0);
    gl_Position = u_Camera.ViewProjectionMatrix * worldPos;
}
