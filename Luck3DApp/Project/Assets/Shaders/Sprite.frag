#version 450 core

// ---- 片段输出 ----
layout(location = 0) out vec4 o_Color;      // 颜色输出（HDR FBO 附件 0）
layout(location = 1) out int  o_EntityID;   // Entity ID 输出（HDR FBO 附件 1，用于鼠标拾取）

// ---- 顶点着色器输入 ----
struct VertexOutput
{
    vec4  Color;
    vec2  TexCoord;
    float TilingFactor;
};

layout(location = 0) in VertexOutput v_Input;
layout(location = 3) in flat float   v_TexIndex;
layout(location = 4) in flat int     v_EntityID;

// ---- 纹理数组：32 个纹理槽（槽 0 = 白色纹理，用于纯色 Quad） ----
uniform sampler2D u_Textures[32];

void main()
{
    // 采样对应槽位的纹理，UV 乘以平铺倍数
    // GLSL 4.5 允许 flat 修饰的整型索引用于常量数组访问
    vec4 texColor = texture(u_Textures[int(v_TexIndex)], v_Input.TexCoord * v_Input.TilingFactor);

    // 最终颜色 = 顶点颜色 × 纹理颜色
    o_Color = v_Input.Color * texColor;

    // Alpha 阈值裁剪：极低 Alpha 直接丢弃，避免半透明像素错误写入 EntityID 影响拾取
    if (o_Color.a < 0.01)
    {
        discard;
    }

    o_EntityID = v_EntityID;
}
