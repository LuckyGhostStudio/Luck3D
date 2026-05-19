#include "lcpch.h"
#include "DebugVisualizePass.h"

#include "Lucky/Renderer/RenderContext.h"
#include "Lucky/Renderer/RenderCommand.h"
#include "Lucky/Renderer/ScreenQuad.h"
#include "Lucky/Renderer/Renderer3D.h"

namespace Lucky
{
    void DebugVisualizePass::Init()
    {
        m_CSMVisualizeShader = Renderer3D::GetShaderLibrary()->Get("DebugCSMVisualize");

        // 创建 PingPong 中转 FBO（与 HDR FBO 同尺寸 + RGBA16F，避免同纹理读写 UB）
        FramebufferSpecification spec;
        spec.Width = 1280;
        spec.Height = 720;
        spec.Attachments = { FramebufferTextureFormat::RGBA16F };
        m_PingFBO = Framebuffer::Create(spec);
    }

    void DebugVisualizePass::Execute(const RenderContext& context)
    {
        // 关闭时零开销直接返回
        if (m_Mode == DebugVisualizeMode::None)
        {
            return;
        }

        // 必要资源检查
        if (!context.HDR_FBO)
        {
            return;
        }

        switch (m_Mode)
        {
            case DebugVisualizeMode::CSMCascades:
                // 仅当阴影开启时叠加级联色，否则没有意义
                if (context.ShadowEnabled)
                {
                    ExecuteCSMCascades(context);
                }
                break;
            default:
                break;
        }
    }

    void DebugVisualizePass::Resize(uint32_t width, uint32_t height)
    {
        if (m_PingFBO)
        {
            m_PingFBO->Resize(width, height);
        }
    }

    void DebugVisualizePass::ExecuteCSMCascades(const RenderContext& context)
    {
        const Ref<Framebuffer>& hdrFBO = context.HDR_FBO;
        uint32_t width = hdrFBO->GetSpecification().Width;
        uint32_t height = hdrFBO->GetSpecification().Height;

        // ---- 阶段 1：HDR -> PingFBO（叠加级联颜色） ----
        m_PingFBO->Bind();
        RenderCommand::SetViewport(0, 0, width, height);
        RenderCommand::SetDepthTest(false);
        RenderCommand::SetDepthWrite(false);

        m_CSMVisualizeShader->Bind();

        // 输入 0：场景颜色（HDR FBO 颜色附件 0）
        RenderCommand::BindTextureUnit(0, hdrFBO->GetColorAttachmentRendererID(0));
        m_CSMVisualizeShader->SetInt("u_SceneColor", 0);

        // 输入 1：场景深度（HDR FBO 深度附件）
        RenderCommand::BindTextureUnit(1, hdrFBO->GetDepthAttachmentRendererID());
        m_CSMVisualizeShader->SetInt("u_SceneDepth", 1);

        // InvProjection 已通过 Camera UBO 自动可用，无需在此 SetMat4

        // 上传级联参数（与 Lucky/Shadow.glsl::SelectCascadeIndex 同算法）
        m_CSMVisualizeShader->SetInt("u_CascadeCount", context.CascadeCount);
        for (int i = 0; i < context.CascadeCount; ++i)
        {
            std::string idx = std::to_string(i);
            m_CSMVisualizeShader->SetFloat("u_DebugCascadeFarPlanes[" + idx + "]", context.CascadeFarPlanes[i]);
        }

        ScreenQuad::Draw();

        m_PingFBO->Unbind();

        // ---- 阶段 2：PingFBO 颜色 Blit 回 HDR_FBO ----
        m_PingFBO->BlitColorTo(hdrFBO);
    }
}
