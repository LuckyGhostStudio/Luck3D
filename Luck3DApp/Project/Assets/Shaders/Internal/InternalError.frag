#version 450 core

layout(location = 0) out vec4 o_Color;  // 颜色缓冲区 0 输出颜色

void main()
{
    o_Color = vec4(1.0, 0.0, 1.0, 1.0); // 洋红色，表示内部错误
}