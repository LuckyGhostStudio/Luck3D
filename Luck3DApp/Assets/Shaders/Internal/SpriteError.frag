#version 450 core

// ---- 片段输出（双附件，与 Sprite.frag 一致） ----
layout(location = 0) out vec4 o_Color;      // 颜色附件
layout(location = 1) out int o_EntityID;    // Entity ID 附件

// ---- 顶点着色器输入 ----
layout(location = 0) in flat int v_EntityID;

void main()
{
    // 洋红色：表示 Sprite 材质丢失
    o_Color = vec4(1.0, 0.0, 1.0, 1.0);
    o_EntityID = v_EntityID;
}
