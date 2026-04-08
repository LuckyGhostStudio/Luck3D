#version 450 core

layout(location = 0) out vec4 o_Color;  // 颜色缓冲区 0 输出颜色

// 相机 Uniform 缓冲区
layout(std140, binding = 0) uniform Camera
{
    mat4 ViewProjectionMatrix;
    vec3 Position;  // 相机位置（用于计算视线向量）
} u_Camera;

// 光照 Uniform 缓冲区
layout(std140, binding = 1) uniform Light
{
    float Intensity;
    vec3 Direction;
    vec3 Color;
} u_Light;

// 顶点着色器输出数据
struct VertexOutput
{
    vec4 Color;
    vec3 Normal;
    vec2 TexCoord;
    vec3 WorldPos;
    mat3 TBN;
};

layout(location = 0) in VertexOutput v_Input;

// 材质 Uniform
uniform vec3 u_AmbientCoeff;      // 环境光系数
uniform vec3 u_DiffuseCoeff;      // 漫反射系数
uniform vec3 u_SpecularCoeff;     // 镜面反射系数
uniform float u_Shininess;        // 镜面指数

uniform sampler2D u_MainTexture;    // 测试纹理

void main()
{
    // 归一化法向量
    vec3 N = normalize(v_Input.Normal);

    // 光向量（方向光，反向）
    vec3 L = normalize(-u_Light.Direction);

    // 视图向量
    vec3 V = normalize(u_Camera.Position - v_Input.WorldPos);

    // 反射向量
    vec3 R = reflect(-L, N);
    
    // 环境光
    vec3 ambient = u_AmbientCoeff * u_Light.Color * u_Light.Intensity;

    // 漫反射
    float diff = max(dot(N, L), 0.0);
    vec3 diffuse = diff * u_DiffuseCoeff * u_Light.Color * u_Light.Intensity;

    // 镜面反射
    float spec = pow(max(dot(R, V), 0.0), u_Shininess);
    vec3 specular = spec * u_SpecularCoeff * u_Light.Color * u_Light.Intensity;

    // 总光照
    vec3 lighting = ambient + diffuse + specular;

    // 采样主纹理颜色
    vec4 mainColor = texture(u_MainTexture, v_Input.TexCoord);

    // 最终颜色：纹理 * 顶点颜色 * 光照
    o_Color = mainColor * v_Input.Color * vec4(lighting, 1.0);
}