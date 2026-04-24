#include "lcpch.h"
#include "SilhouettePass.h"
#include "Lucky/Renderer/RenderContext.h"
#include "Lucky/Renderer/RenderCommand.h"
#include "Lucky/Renderer/Renderer3D.h"

#include <glad/glad.h>

namespace Lucky
{
    void SilhouettePass::Init()
    {
        m_SilhouetteShader = Renderer3D::GetShaderLibrary()->Get("Silhouette");
        
        // 创建 Silhouette FBO（初始大小 1280×720，后续在 Resize 时同步）
        FramebufferSpecification spec;
        spec.Width = 1280;
        spec.Height = 720;
        spec.Attachments = {
            FramebufferTextureFormat::RGBA8     // 轮廓掩码（白色 = 选中，黑色 = 未选中）
            // 不需要深度附件：描边穿透遮挡物（与 Unity 行为一致）
        };
        m_SilhouetteFBO = Framebuffer::Create(spec);
    }
    
    void SilhouettePass::Execute(const RenderContext& context)
    {
        // 条件执行：仅当描边启用且有描边物体时执行
        if (!context.OutlineEnabled || !context.OutlineDrawCommands || context.OutlineDrawCommands->empty())
        {
            return;
        }
        
        // ---- 绑定 Silhouette FBO ----
        m_SilhouetteFBO->Bind();
        
        // 清除为黑色（未选中区域 = 透明黑色）
        RenderCommand::SetClearColor({ 0.0f, 0.0f, 0.0f, 0.0f });
        RenderCommand::Clear();
        
        // 绑定 Silhouette Shader
        m_SilhouetteShader->Bind();
        
        // 禁用深度测试（描边穿透遮挡物，Silhouette FBO 无深度附件）
        glDisable(GL_DEPTH_TEST);
        
        // ---- 遍历描边物体 ----
        for (const OutlineDrawCommand& cmd : *context.OutlineDrawCommands)
        {
            m_SilhouetteShader->SetMat4("u_ObjectToWorldMatrix", cmd.Transform);
            
            RenderCommand::DrawIndexedRange(
                cmd.MeshData->GetVertexArray(),
                cmd.SubMeshPtr->IndexOffset,
                cmd.SubMeshPtr->IndexCount
            );
        }
        
        // ---- 恢复渲染状态 ----
        glEnable(GL_DEPTH_TEST);
        
        // ---- 解绑 FBO ----
        m_SilhouetteFBO->Unbind();
    }
    
    void SilhouettePass::Resize(uint32_t width, uint32_t height)
    {
        if (m_SilhouetteFBO)
        {
            m_SilhouetteFBO->Resize(width, height);
        }
    }
    
    uint32_t SilhouettePass::GetSilhouetteTextureID() const
    {
        return m_SilhouetteFBO->GetColorAttachmentRendererID(0);
    }
}
