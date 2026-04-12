#pragma once

#include <glm/glm.hpp>

namespace Lucky
{
    /// <summary>
    /// 点光源组件：表示一个向所有方向发射光线的点光源
    /// </summary>
    struct PointLightComponent
    {
        glm::vec3 Color = glm::vec3(1.0f, 1.0f, 1.0f);  // 光照颜色
        float Intensity = 1.0f;                         // 光照强度
        float Range = 10.0f;                            // 光照范围

        PointLightComponent() = default;
        PointLightComponent(const PointLightComponent& other) = default;
        PointLightComponent(const glm::vec3& color, float intensity, float range)
            : Color(color), Intensity(intensity), Range(range) {}
    };
}