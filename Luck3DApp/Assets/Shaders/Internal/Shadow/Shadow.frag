// Shadow Pass 片段着色器
// 支持两种透明阴影模式(互斥, 不可同时启用):
// 1. Translucent Shadow Map: 输出透射颜色到 RGBA8 颜色附件, 实现彩色半透明阴影
//    透明物体不写入深度(由 C++ 端关闭 DepthWrite), 阴影浓度完全由颜色衰减控制
//    深度 Shadow Map 只记录不透明物体的遮挡信息
// 2. Dithered Shadow(回退方案): 透明物体使用 Bayer 矩阵抖动实现阴影浓度随透明度渐变
// 不透明物体直接由硬件写入深度, 颜色输出白色(无衰减)
#version 450 core

layout(location = 0) out vec4 o_TranslucentColor;   // Translucent Shadow Map 颜色输出

layout(location = 0) in vec2 v_TexCoord;

// Dithered Shadow / Alpha Test 参数
uniform bool      u_AlphaTestEnabled;       // 是否启用 Dithered Shadow（透明物体为 true）
uniform float     u_AlphaTestThreshold;     // 保留兼容（Dithered 模式下不使用此阈值）
uniform vec4      u_Albedo;                 // 材质基础颜色（包含 Alpha）
uniform sampler2D u_AlbedoMap;              // 材质 Albedo 纹理

// Translucent Shadow Map 参数
uniform bool      u_TranslucentShadowEnabled;   // 是否启用 Translucent Shadow Map

// ---- 4x4 Bayer 抖动矩阵 ----
// 归一化到 [0, 1) 范围, 用于将连续 alpha 值转换为二值裁剪
// 矩阵值分布均匀, 确保不同 alpha 值对应不同比例的片段被 discard
const float bayerMatrix[16] = float[16](
     0.0 / 16.0,  8.0 / 16.0,  2.0 / 16.0, 10.0 / 16.0,
    12.0 / 16.0,  4.0 / 16.0, 14.0 / 16.0,  6.0 / 16.0,
     3.0 / 16.0, 11.0 / 16.0,  1.0 / 16.0,  9.0 / 16.0,
    15.0 / 16.0,  7.0 / 16.0, 13.0 / 16.0,  5.0 / 16.0
);

void main()
{
    if (u_AlphaTestEnabled)
    {
        // 采样材质颜色和 Alpha
        vec4 albedoSample = u_Albedo * texture(u_AlbedoMap, v_TexCoord);
        float alpha = albedoSample.a;
        vec3 albedo = albedoSample.rgb;

        if (u_TranslucentShadowEnabled)
        {
            // ---- Translucent Shadow Map 模式 ----
            // 深度写入已由 C++ 端关闭(SetDepthWrite(false)), 透明物体不影响深度 Shadow Map
            // 输出透射颜色: 光线穿过该片段后的剩余比例（Beer-Lambert 近似）
            // transmittance = mix(白色, 材质颜色, alpha) * (1 - alpha)
            // alpha=0 -> 完全透光, 输出 (1,1,1) 表示无衰减（无阴影）
            // alpha=1 -> 完全遮挡, 输出 (0,0,0) 表示全黑阴影（等同于不透明物体）
            // 中间值 -> 彩色半透明阴影, 颜色由 albedo 决定, 强度由 alpha 决定
            vec3 transmittance = mix(vec3(1.0), albedo, alpha) * (1.0 - alpha);
            o_TranslucentColor = vec4(transmittance, 1.0);
        }
        else
        {
            // ---- Dithered Shadow 回退模式(Translucent Shadow Map 未启用时使用) ----
            ivec2 coord = ivec2(gl_FragCoord.xy) % 4;
            float threshold = bayerMatrix[coord.y * 4 + coord.x];
            if (alpha < threshold)
            {
                discard;
            }
            o_TranslucentColor = vec4(1.0);  // 白色 = 无衰减
        }
    }
    else
    {
        // ---- 不透明物体 ----
        // 不透明物体不影响 Translucent Shadow Map（输出白色 = 无衰减）
        o_TranslucentColor = vec4(1.0);
    }
}
