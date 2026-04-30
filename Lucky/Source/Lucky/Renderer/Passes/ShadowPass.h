#pragma once

#include "Lucky/Renderer/RenderPass.h"
#include "Lucky/Renderer/Shader.h"
#include "Lucky/Renderer/Framebuffer.h"

namespace Lucky
{
    /// <summary>
    /// 阴影 Pass：从光源视角渲染场景深度到 Shadow Map
    /// 支持 Translucent Shadow Map：额外生成透明物体的颜色衰减纹理
    /// 属于 "Shadow" 分组，在 OpaquePass 之前执行
    /// </summary>
    class ShadowPass : public RenderPass
    {
    public:
        void Init() override;
        void Execute(const RenderContext& context) override;
        void Resize(uint32_t width, uint32_t height) override;
        const std::string& GetName() const override { static std::string name = "ShadowPass"; return name; }
        const std::string& GetGroup() const override { static std::string group = "Shadow"; return group; }

        /// <summary>
        /// 获取 Shadow Map 深度纹理 ID（供 OpaquePass 中的 Standard Shader 采样）
        /// </summary>
        uint32_t GetShadowMapTextureID() const;

        /// <summary>
        /// 获取 Translucent Shadow Map 颜色纹理 ID（透明物体颜色衰减信息）
        /// </summary>
        uint32_t GetTranslucentShadowMapTextureID() const;

    private:
        Ref<Framebuffer> m_ShadowMapFBO;        // Shadow Map FBO（深度 + 颜色附件）
        Ref<Shader> m_ShadowShader;             // Shadow Pass Shader
        uint32_t m_ShadowMapResolution = 2048;  // Shadow Map 分辨率
    };
}
