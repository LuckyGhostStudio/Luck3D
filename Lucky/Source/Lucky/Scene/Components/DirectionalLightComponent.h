#pragma once

#include <glm/glm.hpp>

namespace Lucky
{
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
    /// 方向光组件：表示一个平行光源
    /// </summary>
    struct DirectionalLightComponent
    {
        glm::vec3 Color = glm::vec3(1.0f, 1.0f, 1.0f);  // 光照颜色
        float Intensity = 1.0f;                         // 光照强度

        ShadowType Shadows = ShadowType::Hard;          // 阴影类型
        float ShadowBias = 0.0003f;                     // 阴影偏移
        float ShadowStrength = 1.0f;                    // 阴影强度 [0, 1]

        DirectionalLightComponent() = default;
        DirectionalLightComponent(const DirectionalLightComponent& other) = default;
        DirectionalLightComponent(const glm::vec3& color, float intensity)
            : Color(color), Intensity(intensity) {}
    };
}