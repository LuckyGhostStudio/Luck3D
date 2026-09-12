#version 450 core

layout(location = 0) out vec4 o_Color;

in vec2 v_TexCoord;

uniform sampler2D u_SilhouetteTexture;  // Silhouette 纹理（选中物体轮廓掩码）
uniform vec4 u_OutlineColor;            // 描边颜色（默认橙色）
uniform float u_OutlineWidth;           // 描边宽度（像素）

void main()
{
    vec2 texelSize = 1.0 / textureSize(u_SilhouetteTexture, 0);
    
    // 当前像素的 Silhouette 值
    float centerSilhouette = texture(u_SilhouetteTexture, v_TexCoord).r;
    
    // 如果当前像素在选中物体内部，不描边（discard）
    if (centerSilhouette > 0.5)
    {
        discard;
    }
    
    // 边缘检测：在 N×N 范围内采样 Silhouette 纹理
    float maxSilhouette = 0.0;
    int range = int(u_OutlineWidth);
    
    for (int x = -range; x <= range; x++)
    {
        for (int y = -range; y <= range; y++)
        {
            // 圆形采样区域（避免方形描边伪影）
            if (x * x + y * y > range * range)
                continue;
            
            vec2 offset = vec2(float(x), float(y)) * texelSize;
            float s = texture(u_SilhouetteTexture, v_TexCoord + offset).r;
            maxSilhouette = max(maxSilhouette, s);
        }
    }
    
    // 如果采样范围内存在选中物体像素，输出描边颜色
    if (maxSilhouette > 0.5)
    {
        o_Color = u_OutlineColor;
    }
    else
    {
        discard;  // 非描边像素，丢弃
    }
}
