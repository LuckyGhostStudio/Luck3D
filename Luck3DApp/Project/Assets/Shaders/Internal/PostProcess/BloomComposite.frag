#version 450 core

layout(location = 0) out vec4 o_Color;

in vec2 v_TexCoord;

uniform sampler2D u_SourceTexture;  // 原始 HDR 纹理
uniform sampler2D u_BloomTexture;   // 模糊后的泛光纹理
uniform float u_BloomIntensity;

void main()
{
    vec3 hdrColor = texture(u_SourceTexture, v_TexCoord).rgb;
    vec3 bloomColor = texture(u_BloomTexture, v_TexCoord).rgb;

    // 叠加泛光
    vec3 result = hdrColor + bloomColor * u_BloomIntensity;

    o_Color = vec4(result, 1.0);
}