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

// 模型矩阵
uniform mat4 u_ObjectToWorldMatrix;

// 顶点着色器输出数据
struct VertexOutput
{
    vec4 Color;
    vec3 Normal;
    vec2 TexCoord;
    vec3 WorldPos;  // 世界空间位置
    mat3 TBN;       // 切线空间矩阵
};

layout(location = 0) out VertexOutput v_Output;

void main()
{
    v_Output.Color = a_Color;
    v_Output.TexCoord = a_TexCoord;
    
    // 法线矩阵
    mat3 normalMatrix = mat3(transpose(inverse(u_ObjectToWorldMatrix)));

    // 变换法向量到世界空间
    vec3 N = normalize(normalMatrix * a_Normal);
    v_Output.Normal = N;

    // 计算 TBN 矩阵
    vec3 T = normalize(normalMatrix * a_Tangent.xyz);

    // Gram-Schmidt 正交化：确保 T ⊥ N
    T = normalize(T - dot(T, N) * N);

    // Bitangent = cross(N, T) * handedness
    vec3 B = cross(N, T) * a_Tangent.w;
    v_Output.TBN = mat3(T, B, N);

    // 计算世界空间位置
    vec4 worldPos = u_ObjectToWorldMatrix * vec4(a_Position, 1.0);
    v_Output.WorldPos = worldPos.xyz;

    gl_Position = u_Camera.ViewProjectionMatrix * worldPos;
}