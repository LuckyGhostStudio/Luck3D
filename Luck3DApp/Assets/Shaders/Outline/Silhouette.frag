#version 450 core

layout(location = 0) out vec4 o_Color;

void main()
{
    // 输出纯白色：表示该像素属于选中物体
    o_Color = vec4(1.0, 1.0, 1.0, 1.0);
}
