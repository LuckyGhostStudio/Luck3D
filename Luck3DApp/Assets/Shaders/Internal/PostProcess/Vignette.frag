#version 450 core

layout(location = 0) out vec4 o_Color;

in vec2 v_TexCoord;

uniform sampler2D u_SourceTexture;
uniform float u_Intensity;      // 暗角强度
uniform float u_Smoothness;     // 暗角平滑度

void main()
{
    vec3 color = texture(u_SourceTexture, v_TexCoord).rgb;

    // 计算到中心的距离
    vec2 uv = v_TexCoord * 2.0 - 1.0;
    float dist = length(uv);

    // 暗角衰减
    float vignette = 1.0 - smoothstep(1.0 - u_Intensity, 1.0 - u_Intensity + u_Smoothness * 0.5, dist);

    o_Color = vec4(color * vignette, 1.0);
}