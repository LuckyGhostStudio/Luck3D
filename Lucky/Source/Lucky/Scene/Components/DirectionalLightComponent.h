#pragma once

#include <glm/glm.hpp>

namespace Lucky
{
    /// <summary>
    /// 方向光组件：表示一个平行光源 光照方向由实体的 TransformComponent 旋转推导（取 forward 向量）
    /// </summary>
    struct DirectionalLightComponent
    {
        glm::vec3 Color = glm::vec3(1.0f, 1.0f, 1.0f);  // 光照颜色
        float Intensity = 1.0f;                         // 光照强度

        DirectionalLightComponent() = default;
        DirectionalLightComponent(const DirectionalLightComponent& other) = default;
        DirectionalLightComponent(const glm::vec3& color, float intensity)
            : Color(color), Intensity(intensity) {}
    };
}