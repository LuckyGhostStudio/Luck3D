#version 450 core

layout(location = 0) out vec4 o_Color;

in vec2 v_TexCoord;

uniform sampler2D u_HDRTexture;     // HDR 颜色纹理
uniform float u_Exposure;           // 曝光值（默认 1.0）
uniform int u_TonemapMode;          // Tonemapping 模式（0=Reinhard, 1=ACES, 2=Uncharted2）

// ==================== Tonemapping 算法 ====================

// Reinhard
vec3 TonemapReinhard(vec3 color)
{
    return color / (color + vec3(1.0));
}

// ACES Filmic（简化版，来自 Krzysztof Narkowicz）
vec3 TonemapACES(vec3 color)
{
    float a = 2.51;
    float b = 0.03;
    float c = 2.43;
    float d = 0.59;
    float e = 0.14;
    return clamp((color * (a * color + b)) / (color * (c * color + d) + e), 0.0, 1.0);
}

// Uncharted 2 Filmic
vec3 Uncharted2Helper(vec3 x)
{
    float A = 0.15;  // Shoulder Strength
    float B = 0.50;  // Linear Strength
    float C = 0.10;  // Linear Angle
    float D = 0.20;  // Toe Strength
    float E = 0.02;  // Toe Numerator
    float F = 0.30;  // Toe Denominator
    return ((x * (A * x + C * B) + D * E) / (x * (A * x + B) + D * F)) - E / F;
}

vec3 TonemapUncharted2(vec3 color)
{
    float W = 11.2;  // Linear White Point
    vec3 curr = Uncharted2Helper(color);
    vec3 whiteScale = vec3(1.0) / Uncharted2Helper(vec3(W));
    return curr * whiteScale;
}

// ==================== 主函数 ====================

void main()
{
    // 采样 HDR 颜色
    vec3 hdrColor = texture(u_HDRTexture, v_TexCoord).rgb;

    // 应用曝光
    hdrColor *= u_Exposure;

    // Tonemapping
    vec3 ldrColor;
    switch (u_TonemapMode)
    {
        case 0:
            ldrColor = TonemapReinhard(hdrColor);
            break;
        case 1:
            ldrColor = TonemapACES(hdrColor);
            break;
        case 2:
            ldrColor = TonemapUncharted2(hdrColor);
            break;
        default:
            ldrColor = TonemapACES(hdrColor);
            break;
    }

    // Gamma 校正（线性空间 → sRGB）
    ldrColor = pow(ldrColor, vec3(1.0 / 2.2));

    o_Color = vec4(ldrColor, 1.0);
}
