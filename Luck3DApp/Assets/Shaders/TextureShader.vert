#version 450 core

layout(location = 0) in vec3 a_Position;    // 位置
layout(location = 1) in vec4 a_Color;       // 颜色
layout(location = 2) in vec3 a_Normal;      // 法线
layout(location = 3) in vec2 a_TexCoord;    // 纹理坐标

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

// 模型矩阵（用于变换法向量）
uniform mat4 u_ObjectToWorldMatrix;

// 顶点着色器输出数据
struct VertexOutput
{
    vec4 Color;
    vec3 Normal;
    vec2 TexCoord;
    vec3 WorldPos;  // 世界空间位置（用于光照）
};

layout(location = 0) out VertexOutput v_Output;

void main()
{
    v_Output.Color = a_Color;
    v_Output.TexCoord = a_TexCoord;

    // 变换法向量到世界空间
    v_Output.Normal = mat3(transpose(inverse(u_ObjectToWorldMatrix))) * a_Normal;

    // 计算世界空间位置
    vec4 worldPos = u_ObjectToWorldMatrix * vec4(a_Position, 1.0);
    v_Output.WorldPos = worldPos.xyz;

    gl_Position = u_Camera.ViewProjectionMatrix * worldPos;
}