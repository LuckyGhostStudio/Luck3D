#version 450 core

// FXAA 3.11 Quality - 完整实现
// 基于 NVIDIA Timothy Lottes 的 FXAA 3.11 算法

layout(location = 0) out vec4 o_Color;

in vec2 v_TexCoord;

uniform sampler2D u_SourceTexture;

// ============================================================
// 质量预设 - FXAA_QUALITY__P* 定义边缘搜索的步长序列
// 使用 Quality 29 预设（高质量，12次迭代）
// ============================================================
#define EDGE_STEP_COUNT 12
#define EDGE_STEPS 1.0, 1.0, 1.0, 1.0, 1.0, 1.5, 2.0, 2.0, 2.0, 2.0, 4.0, 8.0
#define EDGE_GUESS 8.0

// FXAA 参数
const float EDGE_THRESHOLD_MIN = 0.0312;  // 最小边缘检测阈值（暗区域跳过）
const float EDGE_THRESHOLD     = 0.125;   // 边缘检测阈值（对比度低于此值跳过）
const float SUBPIXEL_QUALITY   = 0.75;    // 子像素抗锯齿强度 (0.0 = 关闭, 1.0 = 最强)

const float STEPS[EDGE_STEP_COUNT] = float[](EDGE_STEPS);

// 计算亮度（使用感知亮度公式）
float Luminance(vec3 color)
{
    return dot(color, vec3(0.299, 0.587, 0.114));
}

void main()
{
    vec2 texelSize = 1.0 / textureSize(u_SourceTexture, 0);

    // ============================================================
    // 第一步：采样 3x3 邻域的亮度（9个采样点）
    // ============================================================
    //  NW  N  NE
    //  W   M   E
    //  SW  S  SE

    float lumM  = Luminance(texture(u_SourceTexture, v_TexCoord).rgb);
    float lumN  = Luminance(texture(u_SourceTexture, v_TexCoord + vec2( 0.0,  texelSize.y)).rgb);
    float lumS  = Luminance(texture(u_SourceTexture, v_TexCoord + vec2( 0.0, -texelSize.y)).rgb);
    float lumE  = Luminance(texture(u_SourceTexture, v_TexCoord + vec2( texelSize.x,  0.0)).rgb);
    float lumW  = Luminance(texture(u_SourceTexture, v_TexCoord + vec2(-texelSize.x,  0.0)).rgb);

    float lumNW = Luminance(texture(u_SourceTexture, v_TexCoord + vec2(-texelSize.x,  texelSize.y)).rgb);
    float lumNE = Luminance(texture(u_SourceTexture, v_TexCoord + vec2( texelSize.x,  texelSize.y)).rgb);
    float lumSW = Luminance(texture(u_SourceTexture, v_TexCoord + vec2(-texelSize.x, -texelSize.y)).rgb);
    float lumSE = Luminance(texture(u_SourceTexture, v_TexCoord + vec2( texelSize.x, -texelSize.y)).rgb);

    // ============================================================
    // 第二步：计算局部对比度，决定是否需要抗锯齿
    // ============================================================
    float lumMin = min(lumM, min(min(lumN, lumS), min(lumE, lumW)));
    float lumMax = max(lumM, max(max(lumN, lumS), max(lumE, lumW)));
    float lumRange = lumMax - lumMin;

    // 对比度太低，不需要抗锯齿，直接输出原始颜色
    if (lumRange < max(EDGE_THRESHOLD_MIN, lumMax * EDGE_THRESHOLD))
    {
        o_Color = texture(u_SourceTexture, v_TexCoord);
        return;
    }

    // ============================================================
    // 第三步：计算子像素混合因子
    // ============================================================
    // 使用 3x3 邻域的加权平均亮度与中心亮度的差异
    float lumL = (2.0 * (lumN + lumS + lumE + lumW) + lumNW + lumNE + lumSW + lumSE) / 12.0;
    float rangeL = abs(lumL - lumM);
    // 归一化到 [0, 1] 范围
    float blendL = max(0.0, (rangeL / lumRange) - 0.25);
    float blendL_clamped = min(1.0, blendL * (1.0 / 0.75));
    // 应用平滑曲线
    float subpixelBlend = blendL_clamped * blendL_clamped * SUBPIXEL_QUALITY;

    // ============================================================
    // 第四步：确定边缘方向（水平 or 垂直）
    // ============================================================
    // 使用 Sobel 算子风格的梯度检测
    float edgeHorz = abs(-2.0 * lumW + lumNW + lumSW)
                   + abs(-2.0 * lumM + lumN  + lumS ) * 2.0
                   + abs(-2.0 * lumE + lumNE + lumSE);

    float edgeVert = abs(-2.0 * lumN + lumNW + lumNE)
                   + abs(-2.0 * lumM + lumW  + lumE ) * 2.0
                   + abs(-2.0 * lumS + lumSW + lumSE);

    bool isHorizontal = (edgeHorz >= edgeVert);

    // ============================================================
    // 第五步：选择边缘的正负方向
    // ============================================================
    // 垂直于边缘方向的两个相邻像素亮度
    float lum1 = isHorizontal ? lumS : lumW;  // 负方向
    float lum2 = isHorizontal ? lumN : lumE;  // 正方向

    float gradient1 = abs(lum1 - lumM);
    float gradient2 = abs(lum2 - lumM);

    // 选择梯度更大的方向作为边缘的法线方向
    bool is1Steeper = gradient1 >= gradient2;

    float gradientScaled = 0.25 * max(gradient1, gradient2);

    // 垂直于边缘方向的步长
    float stepLength = isHorizontal ? texelSize.y : texelSize.x;

    // 边缘处的平均亮度
    float lumaLocalAverage;
    if (is1Steeper)
    {
        stepLength = -stepLength;
        lumaLocalAverage = 0.5 * (lum1 + lumM);
    }
    else
    {
        lumaLocalAverage = 0.5 * (lum2 + lumM);
    }

    // 将当前 UV 偏移到边缘上（半个像素）
    vec2 currentUV = v_TexCoord;
    if (isHorizontal)
    {
        currentUV.y += stepLength * 0.5;
    }
    else
    {
        currentUV.x += stepLength * 0.5;
    }

    // ============================================================
    // 第六步：边缘搜索（Edge Walking）- 核心步骤
    // 沿边缘的正负两个方向迭代搜索，找到边缘的端点
    // ============================================================
    // 沿边缘方向的偏移量
    vec2 offset = isHorizontal ? vec2(texelSize.x, 0.0) : vec2(0.0, texelSize.y);

    // 从当前位置开始，向两个方向搜索
    vec2 uv1 = currentUV - offset;  // 负方向
    vec2 uv2 = currentUV + offset;  // 正方向

    // 采样端点亮度并计算与边缘平均亮度的差值
    float lumaEnd1 = Luminance(texture(u_SourceTexture, uv1).rgb);
    float lumaEnd2 = Luminance(texture(u_SourceTexture, uv2).rgb);
    lumaEnd1 -= lumaLocalAverage;
    lumaEnd2 -= lumaLocalAverage;

    // 判断是否已到达边缘端点（亮度变化超过梯度阈值）
    bool reached1 = abs(lumaEnd1) >= gradientScaled;
    bool reached2 = abs(lumaEnd2) >= gradientScaled;
    bool reachedBoth = reached1 && reached2;

    // 如果未到达端点，继续向该方向搜索
    if (!reached1) uv1 -= offset;
    if (!reached2) uv2 += offset;

    // 迭代搜索边缘端点
    if (!reachedBoth)
    {
        for (int i = 2; i < EDGE_STEP_COUNT; i++)
        {
            if (!reached1)
            {
                lumaEnd1 = Luminance(texture(u_SourceTexture, uv1).rgb);
                lumaEnd1 -= lumaLocalAverage;
            }
            if (!reached2)
            {
                lumaEnd2 = Luminance(texture(u_SourceTexture, uv2).rgb);
                lumaEnd2 -= lumaLocalAverage;
            }

            reached1 = abs(lumaEnd1) >= gradientScaled;
            reached2 = abs(lumaEnd2) >= gradientScaled;
            reachedBoth = reached1 && reached2;

            // 使用质量预设中的步长
            if (!reached1) uv1 -= offset * STEPS[i];
            if (!reached2) uv2 += offset * STEPS[i];

            if (reachedBoth) break;
        }
    }

    // ============================================================
    // 第七步：根据端点距离计算最终偏移量
    // ============================================================
    // 计算当前像素到两个端点的距离
    float distance1, distance2;
    if (isHorizontal)
    {
        distance1 = v_TexCoord.x - uv1.x;
        distance2 = uv2.x - v_TexCoord.x;
    }
    else
    {
        distance1 = v_TexCoord.y - uv1.y;
        distance2 = uv2.y - v_TexCoord.y;
    }

    // 选择较近的端点
    bool isDirection1 = distance1 < distance2;
    float distanceFinal = min(distance1, distance2);
    float edgeLength = distance1 + distance2;

    // 像素在边缘上的位置比例 -> UV 偏移量
    float pixelOffset = -distanceFinal / edgeLength + 0.5;

    // ============================================================
    // 第八步：验证偏移方向的正确性
    // ============================================================
    // 检查较近端点的亮度变化方向是否与中心像素一致
    // 如果不一致，说明偏移方向错误，不应用边缘偏移
    bool isLumaCenterSmaller = lumM < lumaLocalAverage;
    bool correctVariation = ((isDirection1 ? lumaEnd1 : lumaEnd2) < 0.0) != isLumaCenterSmaller;

    float edgeBlend = correctVariation ? pixelOffset : 0.0;

    // ============================================================
    // 第九步：取边缘偏移和子像素偏移的较大值，采样最终颜色
    // ============================================================
    float finalBlend = max(edgeBlend, subpixelBlend);

    // 计算最终 UV
    vec2 finalUV = v_TexCoord;
    if (isHorizontal)
    {
        finalUV.y += finalBlend * stepLength;
    }
    else
    {
        finalUV.x += finalBlend * stepLength;
    }

    o_Color = texture(u_SourceTexture, finalUV);
}