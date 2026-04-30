#include "lcpch.h"
#include "VignetteEffect.h"

#include "Lucky/Renderer/RenderCommand.h"
#include "Lucky/Renderer/ScreenQuad.h"
#include "Lucky/Renderer/Renderer3D.h"

namespace Lucky
{
    void VignetteEffect::Init()
    {
        m_VignetteShader = Renderer3D::GetShaderLibrary()->Get("Vignette");
    }

    void VignetteEffect::Execute(uint32_t sourceTexture, Ref<Framebuffer> destFBO, uint32_t width, uint32_t height)
    {
        RenderCommand::SetDepthTest(false);
        RenderCommand::SetDepthWrite(false);

        destFBO->Bind();
        RenderCommand::SetViewport(0, 0, width, height);
        RenderCommand::Clear();

        m_VignetteShader->Bind();
        m_VignetteShader->SetFloat("u_Intensity", VignetteIntensity);
        m_VignetteShader->SetFloat("u_Smoothness", VignetteSmoothness);
        RenderCommand::BindTextureUnit(0, sourceTexture);
        m_VignetteShader->SetInt("u_SourceTexture", 0);
        ScreenQuad::Draw();

        destFBO->Unbind();

        RenderCommand::SetDepthTest(true);
        RenderCommand::SetDepthWrite(true);
    }
}