// Shadow Pass 顶点着色器
// 从光源视角渲染场景深度到 Shadow Map
// 支持 Alpha Test：传递纹理坐标到片段着色器，用于透明物体的阴影裁剪
#version 450 core

// 顶点布局必须与 Vertex 结构体一致（即使不使用某些属性，也必须声明以保持 layout 偏移正确）
layout(location = 0) in vec3 a_Position;
layout(location = 1) in vec4 a_Color;       // 不使用
layout(location = 2) in vec3 a_Normal;      // 不使用
layout(location = 3) in vec2 a_TexCoord;    // 用于 Alpha Test 采样
layout(location = 4) in vec4 a_Tangent;     // 不使用

// 传递到片段着色器的纹理坐标
layout(location = 0) out vec2 v_TexCoord;

uniform mat4 u_LightSpaceMatrix;    // 光源空间 VP 矩阵
uniform mat4 u_ObjectToWorldMatrix; // 模型矩阵（与 OpaquePass 中的 uniform 名称一致）

void main()
{
    v_TexCoord = a_TexCoord;
    gl_Position = u_LightSpaceMatrix * u_ObjectToWorldMatrix * vec4(a_Position, 1.0);
}
