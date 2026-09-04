#pragma once

#include "Lucky/Core/Base.h"
#include "Lucky/Renderer/Texture.h"

#include <glm/glm.hpp>

namespace Lucky
{
    /// <summary>
    /// Sprite 渲染器组件：世界空间 2D 精灵
    /// 由 Renderer2D 渲染，与 3D 网格一起参与深度测试、鼠标拾取
    /// 
    /// P0 版本决策（详见 docs/RenderingSystem/PhaseR31_Renderer2D_Foundation.md §7.1）：
    /// - 直接引用 Ref&lt;Texture2D&gt;，不引入独立 Sprite Asset 类型
    /// - UVRect 手填以支持简单图集切片
    /// - 未来引入 Sprite Asset 时保留 Texture 字段兼容
    /// </summary>
    struct SpriteRendererComponent
    {
        glm::vec4 Color = glm::vec4(1.0f);                              // Tint 颜色（乘到最终颜色上）
        Ref<Texture2D> Texture;                                         // 纹理引用（nullptr = 纯色）
        glm::vec4 UVRect = glm::vec4(0.0f, 0.0f, 1.0f, 1.0f);           // UV 区域（xy=uvMin, zw=uvMax）
        float TilingFactor = 1.0f;                                      // 平铺倍数

        SpriteRendererComponent() = default;
        SpriteRendererComponent(const SpriteRendererComponent& other) = default;
        SpriteRendererComponent(const glm::vec4& color)
            : Color(color) {}
        SpriteRendererComponent(const Ref<Texture2D>& texture, const glm::vec4& tint = glm::vec4(1.0f))
            : Color(tint), Texture(texture) {}
    };
}
