#include "lcpch.h"
#include "BloomEffect.h"

#include "Lucky/Renderer/RenderCommand.h"
#include "Lucky/Renderer/ScreenQuad.h"
#include "Lucky/Renderer/Renderer3D.h"

namespace Lucky
{
    void BloomEffect::Init()
    {
        m_BrightExtractShader = Renderer3D::GetShaderLibrary()->Get("BrightExtract");
        m_GaussianBlurShader = Renderer3D::GetShaderLibrary()->Get("GaussianBlur");
        m_CompositeShader = Renderer3D::GetShaderLibrary()->Get("BloomComposite");

        // 创建半分辨率 FBO（初始大小，后续随视口调整）
        FramebufferSpecification spec;
        spec.Width = 640;
        spec.Height = 360;
        spec.Attachments = { FramebufferTextureFormat::RGBA16F };

        m_BrightFBO = Framebuffer::Create(spec);
        m_BlurPingFBO = Framebuffer::Create(spec);
        m_BlurPongFBO = Framebuffer::Create(spec);
    }

    void BloomEffect::Execute(uint32_t sourceTexture, Ref<Framebuffer> destFBO, uint32_t width, uint32_t height)
    {
        uint32_t halfWidth = width / 2;
        uint32_t halfHeight = height / 2;
        if (halfWidth == 0)
        {
            halfWidth = 1;
        }
        if (halfHeight == 0)
        {
            halfHeight = 1;
        }

        // 确保 FBO 大小正确
        if (m_BrightFBO->GetSpecification().Width != halfWidth || m_BrightFBO->GetSpecification().Height != halfHeight)
        {
            m_BrightFBO->Resize(halfWidth, halfHeight);
            m_BlurPingFBO->Resize(halfWidth, halfHeight);
            m_BlurPongFBO->Resize(halfWidth, halfHeight);
        }

        RenderCommand::SetDepthTest(false);
        RenderCommand::SetDepthWrite(false);

        // ---- 1. 亮度提取 ----
        m_BrightFBO->Bind();
        RenderCommand::SetViewport(0, 0, halfWidth, halfHeight);
        RenderCommand::Clear();

        m_BrightExtractShader->Bind();
        m_BrightExtractShader->SetFloat("u_Threshold", Threshold);
        RenderCommand::BindTextureUnit(0, sourceTexture);
        m_BrightExtractShader->SetInt("u_SourceTexture", 0);
        ScreenQuad::Draw();
        m_BrightFBO->Unbind();

        // ---- 2. 高斯模糊（多次迭代，水平 + 垂直） ----
        uint32_t currentTexture = m_BrightFBO->GetColorAttachmentRendererID(0);
        bool horizontal = true;

        for (int i = 0; i < Iterations * 2; i++)
        {
            Ref<Framebuffer> targetFBO = horizontal ? m_BlurPingFBO : m_BlurPongFBO;
            targetFBO->Bind();
            RenderCommand::SetViewport(0, 0, halfWidth, halfHeight);
            RenderCommand::Clear();

            m_GaussianBlurShader->Bind();
            m_GaussianBlurShader->SetInt("u_Horizontal", horizontal ? 1 : 0);
            RenderCommand::BindTextureUnit(0, currentTexture);
            m_GaussianBlurShader->SetInt("u_SourceTexture", 0);
            ScreenQuad::Draw();
            targetFBO->Unbind();

            currentTexture = targetFBO->GetColorAttachmentRendererID(0);
            horizontal = !horizontal;
        }

        // ---- 3. 合成（原始 HDR + 模糊泛光 -> destFBO） ----
        destFBO->Bind();
        RenderCommand::SetViewport(0, 0, width, height);
        RenderCommand::Clear();

        m_CompositeShader->Bind();
        m_CompositeShader->SetFloat("u_BloomIntensity", Intensity);
        RenderCommand::BindTextureUnit(0, sourceTexture);
        m_CompositeShader->SetInt("u_SourceTexture", 0);
        RenderCommand::BindTextureUnit(1, currentTexture);
        m_CompositeShader->SetInt("u_BloomTexture", 1);
        ScreenQuad::Draw();
        destFBO->Unbind();

        // 恢复深度状态
        RenderCommand::SetDepthTest(true);
        RenderCommand::SetDepthWrite(true);
    }

    void BloomEffect::Resize(uint32_t width, uint32_t height)
    {
        uint32_t halfWidth = width / 2;
        uint32_t halfHeight = height / 2;
        if (halfWidth == 0)
        {
            halfWidth = 1;
        }
        if (halfHeight == 0)
        {
            halfHeight = 1;
        }

        if (m_BrightFBO)
        {
            m_BrightFBO->Resize(halfWidth, halfHeight);
        }
        if (m_BlurPingFBO)
        {
            m_BlurPingFBO->Resize(halfWidth, halfHeight);
        }
        if (m_BlurPongFBO)
        {
            m_BlurPongFBO->Resize(halfWidth, halfHeight);
        }
    }
}