#version 450 core

layout(location = 0) out vec4 o_Color;

in vec2 v_TexCoord;

uniform sampler2D u_SourceTexture;
uniform bool u_Horizontal;     // true = 水平模糊, false = 垂直模糊

// 9-tap 高斯权重
const float weights[5] = float[](0.227027, 0.1945946, 0.1216216, 0.054054, 0.016216);

void main()
{
    vec2 texelSize = 1.0 / textureSize(u_SourceTexture, 0);
    vec3 result = texture(u_SourceTexture, v_TexCoord).rgb * weights[0];

    if (u_Horizontal)
    {
        for (int i = 1; i < 5; ++i)
        {
            result += texture(u_SourceTexture, v_TexCoord + vec2(texelSize.x * i, 0.0)).rgb * weights[i];
            result += texture(u_SourceTexture, v_TexCoord - vec2(texelSize.x * i, 0.0)).rgb * weights[i];
        }
    }
    else
    {
        for (int i = 1; i < 5; ++i)
        {
            result += texture(u_SourceTexture, v_TexCoord + vec2(0.0, texelSize.y * i)).rgb * weights[i];
            result += texture(u_SourceTexture, v_TexCoord - vec2(0.0, texelSize.y * i)).rgb * weights[i];
        }
    }

    o_Color = vec4(result, 1.0);
}