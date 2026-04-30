#version 450 core

layout(location = 0) out vec4 o_Color;

in vec2 v_TexCoord;

uniform sampler2D u_SourceTexture;
uniform float u_Threshold;

void main()
{
    vec3 color = texture(u_SourceTexture, v_TexCoord).rgb;

    // º∆À„¡¡∂»
    float brightness = dot(color, vec3(0.2126, 0.7152, 0.0722));

    // »Ì„–÷µ£®±‹√‚”≤Ωÿ∂œ£©
    float softThreshold = brightness - u_Threshold;
    softThreshold = clamp(softThreshold, 0.0, 1.0);

    o_Color = vec4(color * softThreshold, 1.0);
}