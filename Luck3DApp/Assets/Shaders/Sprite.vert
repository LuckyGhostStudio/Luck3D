#version 450 core

// ---- 顶点属性（与 Renderer2D::QuadVertex 内存布局一致） ----
layout(location = 0) in vec3  a_Position;       // 世界空间位置（CPU 端已展开 Transform）
layout(location = 1) in vec4  a_Color;          // 顶点颜色 = SpriteRenderer.Color × 内部 tint
layout(location = 2) in vec2  a_TexCoord;       // UV
layout(location = 3) in float a_TexIndex;       // 纹理槽索引 [0, MaxTextureSlots)
layout(location = 4) in float a_TilingFactor;   // 平铺倍数
layout(location = 5) in int   a_EntityID;       // Entity ID（用于鼠标拾取，-1 表示无效）

// ---- 相机 Uniform 缓冲区（与 GizmoLine.vert / Lucky/Common.glsl 保持一致） ----
layout(std140, binding = 0) uniform Camera
{
    mat4 ViewProjectionMatrix;
    mat4 InvProjectionMatrix;
    vec3 Position;
    float _padding;
} u_Camera;

// ---- 顶点着色器输出 ----
struct VertexOutput
{
    vec4  Color;
    vec2  TexCoord;
    float TilingFactor;
};

layout(location = 0) out VertexOutput v_Output;
layout(location = 3) out flat float   v_TexIndex;
layout(location = 4) out flat int     v_EntityID;

void main()
{
    v_Output.Color        = a_Color;
    v_Output.TexCoord     = a_TexCoord;
    v_Output.TilingFactor = a_TilingFactor;
    v_TexIndex            = a_TexIndex;
    v_EntityID            = a_EntityID;

    gl_Position = u_Camera.ViewProjectionMatrix * vec4(a_Position, 1.0);
}
