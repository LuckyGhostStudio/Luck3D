#version 450 core

in vec3 v_TexCoord;     // 方向向量

uniform samplerCube u_SkyboxMap;    // Cubemap 纹理

uniform float u_Exposure;           // 曝光调整（默认 1.0）

uniform vec4 u_Tint;                // 色调调整（默认白色）

layout(location = 0) out vec4 o_Color;

void main()
{
    // 使用方向向量采样 Cubemap
    vec3 color = texture(u_SkyboxMap, v_TexCoord).rgb;
    
    // 应用曝光和色调
    color *= u_Exposure;
    color *= u_Tint.rgb;
    
    o_Color = vec4(color, 1.0);
}
