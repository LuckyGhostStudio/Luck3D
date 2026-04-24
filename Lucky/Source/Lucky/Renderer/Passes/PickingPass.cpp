#include "lcpch.h"
#include "PickingPass.h"
#include "Lucky/Renderer/RenderContext.h"
#include "Lucky/Renderer/RenderCommand.h"
#include "Lucky/Renderer/Renderer3D.h"

#include <glad/glad.h>

namespace Lucky
{
    void PickingPass::Init()
    {
        m_EntityIDShader = Renderer3D::GetShaderLibrary()->Get("EntityID");
    }
    
    void PickingPass::Execute(const RenderContext& context)
    {
        if (!context.OpaqueDrawCommands || context.OpaqueDrawCommands->empty())
        {
            return;
        }
        
        // ---- 设置渲染状态 ----
        
        // 切换 glDrawBuffers：只写入 Attachment 1（Entity ID，R32I）
        GLenum pickBuffers[] = { GL_NONE, GL_COLOR_ATTACHMENT1 };
        glDrawBuffers(2, pickBuffers);
        
        // 关闭深度写入，保持深度测试（复用 OpaquePass 的深度缓冲区）
        glDepthMask(GL_FALSE);
        glDepthFunc(GL_LEQUAL);     // GL_LEQUAL 保证相同深度可通过
        
        // 绑定 EntityID Shader（VP 矩阵通过 Camera UBO binding=0 自动传递，无需手动设置）
        m_EntityIDShader->Bind();
        
        // ---- 遍历绘制 ----
        for (const DrawCommand& cmd : *context.OpaqueDrawCommands)
        {
            // 设置模型变换矩阵
            m_EntityIDShader->SetMat4("u_ObjectToWorldMatrix", cmd.Transform);
            // 设置 Entity ID
            m_EntityIDShader->SetInt("u_EntityID", cmd.EntityID);
            
            // 绘制
            RenderCommand::DrawIndexedRange(
                cmd.MeshData->GetVertexArray(),
                cmd.SubMeshPtr->IndexOffset,
                cmd.SubMeshPtr->IndexCount
            );
        }
        
        // ---- 恢复渲染状态 ----
        
        // 恢复 glDrawBuffers：只写入 Attachment 0（颜色），禁用 Attachment 1（EntityID）
        // 防止后续 Gizmo 渲染时向 EntityID 缓冲区写入未定义数据
        GLenum normalBuffers[] = { GL_COLOR_ATTACHMENT0, GL_NONE };
        glDrawBuffers(2, normalBuffers);
        glDepthMask(GL_TRUE);
        glDepthFunc(GL_LESS);
    }
}
