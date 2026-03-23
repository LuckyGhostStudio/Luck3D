#version 450 core

layout(location = 0) out vec4 o_Color;  // 颜色缓冲区 0 输出颜色

layout(std140, binding = 0) uniform Camera
{
    mat4 u_ViewProjectionMatrix;
    vec3 u_CameraPos;   // 相机位置
};

// 顶点着色器输出数据
struct VertexOutput
{
    vec4 Color;
    vec3 Normal;
    vec2 TexCoord;
    vec3 WorldPos;
};

layout(location = 0) in VertexOutput v_Input;

layout(binding = 0) uniform sampler2D u_Textures[32];   // 纹理采样器 0 - 31
uniform int u_TextureIndex;  // 当前使用的纹理索引

// 光照 Uniform
uniform vec3 u_LightDirection;    // 方向光方向（世界空间）
uniform vec3 u_LightColor;        // 光颜色
uniform float u_LightIntensity;   // 光强度

// 材质 Uniform
uniform vec3 u_AmbientCoeff;      // 环境光系数
uniform vec3 u_DiffuseCoeff;      // 漫反射系数
uniform vec3 u_SpecularCoeff;     // 镜面反射系数
uniform float u_Shininess;        // 镜面指数

void main()
{
    // 归一化法向量
    vec3 N = normalize(v_Input.Normal);

    // 光向量（方向光，反向）
    vec3 L = normalize(-u_LightDirection);

    // 视图向量
    vec3 V = normalize(u_CameraPos - v_Input.WorldPos);

    // 反射向量
    vec3 R = reflect(-L, N);

    // 环境光
    vec3 ambient = u_AmbientCoeff * u_LightColor * u_LightIntensity;

    // 漫反射
    float diff = max(dot(N, L), 0.0);
    vec3 diffuse = diff * u_DiffuseCoeff * u_LightColor * u_LightIntensity;

    // 镜面反射
    float spec = pow(max(dot(R, V), 0.0), u_Shininess);
    vec3 specular = spec * u_SpecularCoeff * u_LightColor * u_LightIntensity;

    // 总光照
    vec3 lighting = ambient + diffuse + specular;

    // 采样纹理
    vec4 texColor = texture(u_Textures[u_TextureIndex], v_Input.TexCoord);

    // 最终颜色：纹理 * 顶点颜色 * 光照
    o_Color = texColor * v_Input.Color * vec4(lighting, 1.0);
}