#pragma once

#include "Lucky/Renderer/PostProcessEffect.h"

namespace Lucky
{
    /// <summary>
    /// FXAA 快速抗锯齿效果
    /// 在 LDR 空间执行（Tonemapping 之后）
    /// </summary>
    class FXAAEffect : public PostProcessEffect
    {
    public:
        void Init() override;
        void Execute(uint32_t sourceTexture, Ref<Framebuffer> destFBO, uint32_t width, uint32_t height) override;

        const std::string& GetName() const override
        {
            static std::string name = "FXAA";
            return name;
        }

        PostProcessSpace GetSpace() const override { return PostProcessSpace::LDR; }

    private:
        Ref<Shader> m_FXAAShader;
    };
}