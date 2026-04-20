#version 450 core

// 逆 VP 矩阵（用于将 NDC 坐标反投影到世界空间）
uniform mat4 u_InverseVP;

// 输出到片段着色器的世界空间射线端点
out vec3 v_NearPoint;   // 近平面上的世界空间点
out vec3 v_FarPoint;    // 远平面上的世界空间点

// 全屏四边形顶点（硬编码，使用 gl_VertexID 索引）
// 两个三角形组成覆盖整个 NDC 空间的四边形
vec3 gridPlane[6] = vec3[](
    vec3(-1.0, -1.0, 0.0), vec3(1.0, -1.0, 0.0), vec3(1.0, 1.0, 0.0),
    vec3(-1.0, -1.0, 0.0), vec3(1.0, 1.0, 0.0), vec3(-1.0, 1.0, 0.0)
);

/// 将 NDC 空间的点通过逆 VP 矩阵反投影到世界空间
vec3 UnprojectPoint(vec3 clipPos)
{
    vec4 worldPos = u_InverseVP * vec4(clipPos, 1.0);
    return worldPos.xyz / worldPos.w;   // 透视除法
}

void main()
{
    vec3 pos = gridPlane[gl_VertexID];
    
    // 将 NDC 空间的近平面（z=-1）和远平面（z=1）上的点反投影到世界空间
    // OpenGL NDC 的 Z 范围是 [-1, 1]
    v_NearPoint = UnprojectPoint(vec3(pos.xy, -1.0));   // 近平面
    v_FarPoint  = UnprojectPoint(vec3(pos.xy,  1.0));   // 远平面
    
    // 全屏四边形直接输出 NDC 坐标，不需要任何变换
    gl_Position = vec4(pos, 1.0);
}
