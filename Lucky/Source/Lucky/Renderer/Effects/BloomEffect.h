#pragma once

#include "Lucky/Renderer/PostProcessEffect.h"

namespace Lucky
{
    /// <summary>
    /// Bloom 泛光效果
    /// 在 HDR 空间执行，提取高亮区域并模糊叠加
    /// </summary>
    class BloomEffect : public PostProcessEffect
    {
    public:
        float Threshold = 1.0f;     // 亮度阈值
        float Intensity = 1.0f;     // 泛光强度
        int Iterations = 5;         // 模糊迭代次数

        void Init() override;
        void Execute(uint32_t sourceTexture, Ref<Framebuffer> destFBO, uint32_t width, uint32_t height) override;
        void Resize(uint32_t width, uint32_t height) override;

        const std::string& GetName() const override
        {
            static std::string name = "Bloom";
            return name;
        }

        PostProcessSpace GetSpace() const override { return PostProcessSpace::HDR; }

    private:
        Ref<Shader> m_BrightExtractShader;
        Ref<Shader> m_GaussianBlurShader;
        Ref<Shader> m_CompositeShader;
        Ref<Framebuffer> m_BrightFBO;       // 亮度提取 FBO
        Ref<Framebuffer> m_BlurPingFBO;     // 模糊 Ping FBO
        Ref<Framebuffer> m_BlurPongFBO;     // 模糊 Pong FBO
    };
}