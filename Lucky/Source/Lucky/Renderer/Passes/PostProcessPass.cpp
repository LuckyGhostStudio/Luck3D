#include "lcpch.h"
#include "PostProcessPass.h"

#include "Lucky/Renderer/RenderContext.h"
#include "Lucky/Renderer/RenderCommand.h"
#include "Lucky/Renderer/ScreenQuad.h"
#include "Lucky/Renderer/Renderer3D.h"

namespace Lucky
{
    void PostProcessPass::Init()
    {
        // 创建 HDR FBO（与主 FBO 相同的附件布局，但颜色格式为 RGBA16F）
        FramebufferSpecification hdrSpec;
        hdrSpec.Width = 1280;   // 初始大小，后续随视口调整
        hdrSpec.Height = 720;
        hdrSpec.Attachments = {
            FramebufferTextureFormat::RGBA16F,          // HDR 颜色（Attachment 0）
            FramebufferTextureFormat::RED_INTEGER,      // Entity ID（Attachment 1，保留鼠标拾取功能）
            FramebufferTextureFormat::DEFPTH24STENCIL8  // 深度模板
        };
        m_HDR_FBO = Framebuffer::Create(hdrSpec);

        // 加载 Tonemapping Shader
        m_TonemappingShader = Renderer3D::GetShaderLibrary()->Get("Tonemapping");
    }

    void PostProcessPass::Execute(const RenderContext& context)
    {
        // 绑定主 FBO（RGBA8）作为 Tonemapping 输出目标
        if (context.TargetFramebuffer)
        {
            context.TargetFramebuffer->Bind();
        }

        // 禁用深度测试（全屏后处理不需要）
        RenderCommand::SetDepthTest(false);
        RenderCommand::SetDepthWrite(false);

        // 绑定 Tonemapping Shader
        m_TonemappingShader->Bind();
        m_TonemappingShader->SetFloat("u_Exposure", context.Exposure);
        m_TonemappingShader->SetInt("u_TonemapMode", context.TonemapMode);

        // 绑定 HDR 颜色纹理到纹理单元 0
        RenderCommand::BindTextureUnit(0, GetHDRColorTextureID());
        m_TonemappingShader->SetInt("u_HDRTexture", 0);

        // 绘制全屏 Quad
        ScreenQuad::Draw();

        // 恢复深度测试状态
        RenderCommand::SetDepthTest(true);
        RenderCommand::SetDepthWrite(true);

        // 将 HDR FBO 的深度缓冲区 Blit 到主 FBO（使 Gizmo 能被场景物体正确遮挡）
        if (m_HDR_FBO && context.TargetFramebuffer)
        {
            m_HDR_FBO->BlitDepthTo(context.TargetFramebuffer);
        }
    }

    void PostProcessPass::Resize(uint32_t width, uint32_t height)
    {
        if (m_HDR_FBO)
        {
            m_HDR_FBO->Resize(width, height);
        }
    }

    uint32_t PostProcessPass::GetHDRColorTextureID() const
    {
        return m_HDR_FBO->GetColorAttachmentRendererID(0);
    }
}
