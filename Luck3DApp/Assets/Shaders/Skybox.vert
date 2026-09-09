#version 450 core

out vec2 v_ClipXY;      // 传给片段：裁剪空间 xy（用于反算世界方向）

// 全屏四边形顶点（硬编码，使用 gl_VertexID 索引）
// 两个三角形组成覆盖整个 NDC 空间的四边形，z=1 使深度恒为远平面（配合 LessEqual 通过测试）
vec2 quadVertices[6] = vec2[](
    vec2(-1.0, -1.0), vec2(1.0, -1.0), vec2(1.0, 1.0),
    vec2(-1.0, -1.0), vec2(1.0,  1.0), vec2(-1.0, 1.0)
);

void main()
{
    vec2 pos = quadVertices[gl_VertexID];
    v_ClipXY = pos;
    gl_Position = vec4(pos, 1.0, 1.0);
}
