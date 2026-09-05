#pragma once

#include "Lucky/Core/Base.h"
#include "Lucky/Renderer/Material.h"
#include "Lucky/Renderer/Texture.h"

#include <glm/glm.hpp>

namespace Lucky
{
    struct SpriteRendererComponent
    {
        Ref<Texture2D> Texture;                                         // 纹理引用（nullptr = 纯色）
        glm::vec4 Color = glm::vec4(1.0f);                              // Tint 颜色（乘到最终颜色上）
        bool FlipX = false;                                             // 水平翻转
        bool FlipY = false;                                             // 垂直翻转
        glm::vec4 UVRect = glm::vec4(0.0f, 0.0f, 1.0f, 1.0f);           // UV 区域（xy=uvMin, zw=uvMax）
        float TilingFactor = 1.0f;                                      // 平铺倍数
        Ref<Material> Material;                                         // 材质引用（决定 Shader / RenderState / 合批分组，nullptr = 使用 Renderer2D 默认材质）
        int SortingOrder = 0;                                           // 排序序号（数值越大越靠前）

        SpriteRendererComponent() = default;
        SpriteRendererComponent(const SpriteRendererComponent& other) = default;
        SpriteRendererComponent(const glm::vec4& color)
            : Color(color) {}
        SpriteRendererComponent(const Ref<Texture2D>& texture, const glm::vec4& tint = glm::vec4(1.0f))
            : Texture(texture), Color(tint) {}
    };
}
