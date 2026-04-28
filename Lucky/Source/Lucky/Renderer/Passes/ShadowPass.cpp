#include "lcpch.h"
#include "ShadowPass.h"
#include "Lucky/Renderer/RenderContext.h"
#include "Lucky/Renderer/RenderCommand.h"
#include "Lucky/Renderer/Renderer3D.h"

namespace Lucky
{
    void ShadowPass::Init()
    {
        m_ShadowShader = Renderer3D::GetShaderLibrary()->Get("Shadow");

        // 创建 Shadow Map FBO（纯深度纹理，无颜色附件）
        FramebufferSpecification spec;
        spec.Width = m_ShadowMapResolution;
        spec.Height = m_ShadowMapResolution;
        spec.Attachments = { FramebufferTextureFormat::DEPTH_COMPONENT };
        m_ShadowMapFBO = Framebuffer::Create(spec);
    }

    void ShadowPass::Execute(const RenderContext& context)
    {
        // 条件执行：仅当阴影启用且有不透明物体时执行
        if (!context.ShadowEnabled || !context.OpaqueDrawCommands || context.OpaqueDrawCommands->empty())
        {
            return;
        }

        // ---- 绑定 Shadow Map FBO ----
        m_ShadowMapFBO->Bind();
        RenderCommand::SetViewport(0, 0, m_ShadowMapResolution, m_ShadowMapResolution);
        RenderCommand::Clear();

        // ---- 设置渲染状态：关闭面剔除（双面渲染，避免薄物体/近距离阴影缺失） ----
        RenderCommand::SetCullMode(CullMode::Off);

        // ---- 绑定 Shader 并设置 Light Space Matrix ----
        m_ShadowShader->Bind();
        m_ShadowShader->SetMat4("u_LightSpaceMatrix", context.LightSpaceMatrix);

        // ---- 遍历 DrawCommand 列表（复用 OpaquePass 的数据） ----
        for (const DrawCommand& cmd : *context.OpaqueDrawCommands)
        {
            m_ShadowShader->SetMat4("u_ObjectToWorldMatrix", cmd.Transform);

            RenderCommand::DrawIndexedRange(
                cmd.MeshData->GetVertexArray(),
                cmd.SubMeshPtr->IndexOffset,
                cmd.SubMeshPtr->IndexCount
            );
        }

        // ---- 恢复渲染状态 ----
        RenderCommand::SetCullMode(CullMode::Back);
        m_ShadowMapFBO->Unbind();

        // ---- 恢复主 FBO 视口 ----
        if (context.TargetFramebuffer)
        {
            context.TargetFramebuffer->Bind();
            const auto& spec = context.TargetFramebuffer->GetSpecification();
            RenderCommand::SetViewport(0, 0, spec.Width, spec.Height);
        }
    }

    void ShadowPass::Resize(uint32_t width, uint32_t height)
    {
        // Shadow Map 分辨率固定，不随视口变化
        // 后续可根据光源的 ShadowResolution 属性动态调整
    }

    uint32_t ShadowPass::GetShadowMapTextureID() const
    {
        return m_ShadowMapFBO->GetDepthAttachmentRendererID();
    }
}
