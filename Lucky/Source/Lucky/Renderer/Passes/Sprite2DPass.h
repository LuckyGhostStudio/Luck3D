#pragma once

#include "Lucky/Renderer/RenderPass.h"

namespace Lucky
{
    /// <summary>
    /// 2D Sprite Pass：将 Renderer2D 接入 RenderPipeline
    /// 从 RenderContext.SpriteDrawCommands 循环调用 Renderer2D::DrawSprite
    /// 属于 "Main" 分组，在 TransparentPass 之后、PickingPass 之前执行
    /// 
    /// 详细设计参见 docs/RenderingSystem/PhaseR31_Renderer2D_Foundation.md §8
    /// </summary>
    class Sprite2DPass : public RenderPass
    {
    public:
        void Init() override {}
        void Execute(const RenderContext& context) override;

        const std::string& GetName() const override
        {
            static std::string name = "Sprite2DPass";
            return name;
        }

        const std::string& GetGroup() const override
        {
            static std::string group = "Main";
            return group;
        }
    };
}
