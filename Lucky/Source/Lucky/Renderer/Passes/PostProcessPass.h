#pragma once

#include "Lucky/Renderer/RenderPass.h"
#include "Lucky/Renderer/Framebuffer.h"
#include "Lucky/Renderer/Shader.h"

namespace Lucky
{
    /// <summary>
    /// 后处理 Pass
    /// 持有 HDR FBO（RGBA16F），执行后处理效果链 + Tonemapping + Gamma 校正
    /// 属于 "PostProcess" 分组，在 Main 分组之后执行
    /// R5 阶段仅包含 Tonemapping，R6 阶段将扩展效果链
    /// </summary>
    class PostProcessPass : public RenderPass
    {
    public:
        void Init() override;
        void Execute(const RenderContext& context) override;
        void Resize(uint32_t width, uint32_t height) override;

        const std::string& GetName() const override
        {
            static std::string name = "PostProcessPass";
            return name;
        }

        const std::string& GetGroup() const override
        {
            static std::string group = "PostProcess";
            return group;
        }

        /// <summary>
        /// 获取 HDR FBO（供 OpaquePass 等 Main 分组 Pass 作为渲染目标）
        /// </summary>
        const Ref<Framebuffer>& GetHDR_FBO() const { return m_HDR_FBO; }

        /// <summary>
        /// 获取 HDR 颜色纹理 ID（用于 Tonemapping 采样）
        /// </summary>
        uint32_t GetHDRColorTextureID() const;

    private:
        Ref<Framebuffer> m_HDR_FBO;             // HDR 渲染目标（RGBA16F + RED_INTEGER + Depth）
        Ref<Shader> m_TonemappingShader;        // Tonemapping 着色器
    };
}
