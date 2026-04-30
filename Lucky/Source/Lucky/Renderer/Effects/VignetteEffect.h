#pragma once

#include "Lucky/Renderer/PostProcessEffect.h"

namespace Lucky
{
    /// <summary>
    /// Vignette 暗角效果
    /// 在 HDR 空间执行
    /// </summary>
    class VignetteEffect : public PostProcessEffect
    {
    public:
        float VignetteIntensity = 0.5f;    // 暗角强度
        float VignetteSmoothness = 2.0f;   // 暗角平滑度

        void Init() override;
        void Execute(uint32_t sourceTexture, Ref<Framebuffer> destFBO, uint32_t width, uint32_t height) override;

        const std::string& GetName() const override
        {
            static std::string name = "Vignette";
            return name;
        }

        PostProcessSpace GetSpace() const override { return PostProcessSpace::HDR; }

    private:
        Ref<Shader> m_VignetteShader;
    };
}