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
        // 创建 HDR FBO
        FramebufferSpecification hdrSpec;
        hdrSpec.Width = 1280;
        hdrSpec.Height = 720;
        hdrSpec.Attachments = {
            FramebufferTextureFormat::RGBA16F,
            FramebufferTextureFormat::RED_INTEGER,
            FramebufferTextureFormat::DEFPTH24STENCIL8
        };
        m_HDR_FBO = Framebuffer::Create(hdrSpec);

        // 加载 Tonemapping Shader
        m_TonemappingShader = Renderer3D::GetShaderLibrary()->Get("Tonemapping");

        // 初始化后处理栈
        m_PostProcessStack.Init(1280, 720);
    }

    void PostProcessPass::Execute(const RenderContext& context)
    {
        uint32_t width = m_HDR_FBO->GetSpecification().Width;
        uint32_t height = m_HDR_FBO->GetSpecification().Height;

        // ---- 1. 执行 HDR 空间效果链（Bloom、Vignette 等） ----
        uint32_t processedTexture = m_PostProcessStack.Execute(GetHDRColorTextureID(), PostProcessSpace::HDR, width, height);

        // ---- 2. Tonemapping（HDR -> LDR） ----
        // 绑定主 FBO 作为 Tonemapping 输出目标
        if (context.TargetFramebuffer)
        {
            context.TargetFramebuffer->Bind();
        }

        RenderCommand::SetDepthTest(false);
        RenderCommand::SetDepthWrite(false);

        m_TonemappingShader->Bind();
        m_TonemappingShader->SetFloat("u_Exposure", context.PostProcess.Exposure);
        m_TonemappingShader->SetInt("u_TonemapMode", static_cast<int>(context.PostProcess.Tonemap));

        RenderCommand::BindTextureUnit(0, processedTexture);
        m_TonemappingShader->SetInt("u_HDRTexture", 0);

        ScreenQuad::Draw();

        // ---- 3. 执行 LDR 空间效果链（FXAA 等） ----
        if (context.TargetFramebuffer && m_PostProcessStack.HasEnabledEffects(PostProcessSpace::LDR))
        {
            // 获取 Tonemapping 输出的 LDR 纹理
            uint32_t ldrTexture = context.TargetFramebuffer->GetColorAttachmentRendererID(0);
            
            // 通过 PostProcessStack 执行 LDR 效果链
            uint32_t finalTexture = m_PostProcessStack.Execute(ldrTexture, PostProcessSpace::LDR, width, height);
            
            // 如果 LDR 效果链产生了新纹理，需要将结果 Blit 回主 FBO
            if (finalTexture != ldrTexture)
            {
                context.TargetFramebuffer->Bind();
                RenderCommand::SetDepthTest(false);
                RenderCommand::SetDepthWrite(false);
                
                m_TonemappingShader->Bind();
                m_TonemappingShader->SetFloat("u_Exposure", 1.0f);
                m_TonemappingShader->SetInt("u_TonemapMode", -1);  // -1 表示直通模式（不做 Tonemapping）
                
                RenderCommand::BindTextureUnit(0, finalTexture);
                m_TonemappingShader->SetInt("u_HDRTexture", 0);
                ScreenQuad::Draw();
            }
        }

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

        // 同步调整后处理栈的 FBO 大小
        m_PostProcessStack.Resize(width, height);
    }

    uint32_t PostProcessPass::GetHDRColorTextureID() const
    {
        return m_HDR_FBO->GetColorAttachmentRendererID(0);
    }
}
