#version 450 core

// ---- 顶点属性（与 Renderer2D::QuadVertex 内存布局一致） ----
layout(location = 0) in vec3 a_Position;
layout(location = 1) in vec4 a_Color;
layout(location = 2) in vec2 a_TexCoord;
layout(location = 3) in float a_TexIndex;
layout(location = 4) in float a_TilingFactor;
layout(location = 5) in int a_EntityID;

// ---- 相机 Uniform 缓冲区（与 Sprite.vert 保持一致，binding=0） ----
layout(std140, binding = 0) uniform Camera
{
    mat4 ViewProjectionMatrix;
    mat4 InvProjectionMatrix;
    vec3 Position;
    float _padding;
} u_Camera;

// ---- 输出到片段：仅 EntityID（材质丢失时不需要顶点色 / UV） ----
layout(location = 0) out flat int v_EntityID;

void main()
{
    v_EntityID = a_EntityID;
    gl_Position = u_Camera.ViewProjectionMatrix * vec4(a_Position, 1.0);
}
