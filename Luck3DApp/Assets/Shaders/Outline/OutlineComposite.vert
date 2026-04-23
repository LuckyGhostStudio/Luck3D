#version 450 core

layout(location = 0) in vec2 a_Position;    // 全屏四边形顶点位置 [-1, 1]
layout(location = 1) in vec2 a_TexCoord;    // 纹理坐标 [0, 1]

out vec2 v_TexCoord;

void main()
{
    v_TexCoord = a_TexCoord;
    gl_Position = vec4(a_Position, 0.0, 1.0);
}
