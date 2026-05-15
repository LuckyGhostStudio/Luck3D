#pragma once

#include <glm/glm.hpp>

namespace Lucky
{
    /// <summary>
    /// 光源类型
    /// </summary>
    enum class LightType : uint8_t
    {
        Directional = 0,    // 方向光（平行光）
        Point,              // 点光源
        Spot                // 聚光灯
    };

    /// <summary>
    /// 阴影类型
    /// </summary>
    enum class ShadowType : uint8_t
    {
        None = 0,       // 不投射阴影
        Hard,           // 硬阴影（无 PCF）
        Soft            // 软阴影（PCF 3×3）
    };

    /// <summary>
    /// 统一光源组件：表示场景中的一个光源
    /// 通过 Type 字段区分光源类型，不同类型使用不同的属性子集
    /// </summary>
    struct LightComponent
    {
        // ======== 通用属性（所有光源类型共享） ========
        LightType Type = LightType::Directional;            // 光源类型
        glm::vec3 Color = glm::vec3(1.0f, 1.0f, 1.0f);    // 光照颜色
        float Intensity = 1.0f;                             // 光照强度

        // ======== Point / Spot 属性 ========
        float Range = 10.0f;                                // 光照范围（Point/Spot 使用）

        // ======== Spot 属性 ========
        float InnerCutoffAngle = 12.5f;                     // 内锥角（度）（Spot 使用）
        float OuterCutoffAngle = 17.5f;                     // 外锥角（度）（Spot 使用）

        // ======== 阴影属性（所有光源类型共享） ========
        ShadowType Shadows = ShadowType::None;              // 阴影类型
        float ShadowBias = 0.0003f;                         // 阴影偏移
        float ShadowStrength = 1.0f;                        // 阴影强度 [0, 1]

        // ======== CSM 属性（仅 LightType::Directional 使用） ========
        int CascadeCount = 4;                                       // 级联数量 [1, 4]
        float ShadowDistance = 150.0f;                              // 阴影最大距离（世界空间单位）
        float CascadeSplits[4] = { 0.067f, 0.2f, 0.467f, 1.0f };    // 级联分割比例（占 ShadowDistance 的百分比）
        int ShadowMapResolution = 2048;                             // 每级 Shadow Map 分辨率

        // ======== 构造函数 ========
        LightComponent() = default;
        LightComponent(const LightComponent& other) = default;

        /// <summary>
        /// 创建指定类型的光源组件
        /// </summary>
        /// <param name="type">光源类型</param>
        LightComponent(LightType type)
            : Type(type)
        {
            // 根据类型设置合理的默认值
            switch (type)
            {
                case LightType::Directional:
                    Shadows = ShadowType::Hard;
                    break;
                case LightType::Point:
                    Shadows = ShadowType::Hard;
                    break;
                case LightType::Spot:
                    Shadows = ShadowType::Hard;
                    break;
            }
        }
    };
}
