#include "lcpch.h"
#include "FXAAEffect.h"

#include "Lucky/Renderer/RenderCommand.h"
#include "Lucky/Renderer/ScreenQuad.h"
#include "Lucky/Renderer/Renderer3D.h"

namespace Lucky
{
    void FXAAEffect::Init()
    {
        m_FXAAShader = Renderer3D::GetShaderLibrary()->Get("FXAA");
    }

    void FXAAEffect::Execute(uint32_t sourceTexture, Ref<Framebuffer> destFBO, uint32_t width, uint32_t height)
    {
        RenderCommand::SetDepthTest(false);
        RenderCommand::SetDepthWrite(false);

        destFBO->Bind();
        RenderCommand::SetViewport(0, 0, width, height);
        RenderCommand::Clear();

        m_FXAAShader->Bind();
        RenderCommand::BindTextureUnit(0, sourceTexture);
        m_FXAAShader->SetInt("u_SourceTexture", 0);
        ScreenQuad::Draw();

        destFBO->Unbind();

        RenderCommand::SetDepthTest(true);
        RenderCommand::SetDepthWrite(true);
    }
}