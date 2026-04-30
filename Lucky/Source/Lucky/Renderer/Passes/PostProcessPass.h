#pragma once

#include "Lucky/Renderer/RenderPass.h"
#include "Lucky/Renderer/Framebuffer.h"
#include "Lucky/Renderer/Shader.h"
#include "Lucky/Renderer/PostProcessStack.h"

namespace Lucky
{
    /// <summary>
    /// 后处理 Pass
    /// 持有 HDR FBO + PostProcessStack，执行完整的后处理管线：
    /// HDR 效果链 -> Tonemapping -> LDR 效果链
    /// 属于 "PostProcess" 分组，在 Main 分组之后执行
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
        /// 获取 HDR 颜色纹理 ID
        /// </summary>
        uint32_t GetHDRColorTextureID() const;

        /// <summary>
        /// 获取后处理栈（供外部添加/配置效果）
        /// </summary>
        PostProcessStack& GetPostProcessStack() { return m_PostProcessStack; }

    private:
        Ref<Framebuffer> m_HDR_FBO;             // HDR 渲染目标（RGBA16F + RED_INTEGER + Depth）
        Ref<Shader> m_TonemappingShader;        // Tonemapping 着色器
        PostProcessStack m_PostProcessStack;    // 后处理效果栈
    };
}
