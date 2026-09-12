#version 450 core

// 点光源阴影片段着色器
// 使用线性深度（距离/远平面），确保深度精度均匀分布

layout(location = 0) in vec3 v_WorldPos;    // 世界空间位置

uniform vec3 u_LightPos;                    // 点光源世界空间位置
uniform float u_FarPlane;                   // 远平面距离（= Range）

void main()
{
    // 计算片段到光源的线性距离，归一化到 [0, 1]
    float lightDistance = length(v_WorldPos - u_LightPos);
    gl_FragDepth = lightDistance / u_FarPlane;
}
