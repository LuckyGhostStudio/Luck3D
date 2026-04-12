#pragma once

#include <glm/glm.hpp>

namespace Lucky
{
    /// <summary>
    /// 聚光灯组件：表示一个锥形光源
    /// </summary>
    struct SpotLightComponent
    {
        glm::vec3 Color = glm::vec3(1.0f, 1.0f, 1.0f);  // 光照颜色
        float Intensity = 1.0f;                         // 光照强度
        float Range = 10.0f;                            // 光照范围
        float InnerCutoffAngle = 12.5f;                 // 内锥角（度）
        float OuterCutoffAngle = 17.5f;                 // 外锥角（度）

        SpotLightComponent() = default;
        SpotLightComponent(const SpotLightComponent& other) = default;
    };
}