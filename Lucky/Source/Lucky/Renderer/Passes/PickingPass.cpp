#include "lcpch.h"
#include "PickingPass.h"
#include "Lucky/Renderer/RenderContext.h"
#include "Lucky/Renderer/RenderCommand.h"
#include "Lucky/Renderer/RenderState.h"
#include "Lucky/Renderer/Renderer3D.h"

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
        
        // 切换 DrawBuffers：只写入 Attachment 1（Entity ID，R32I）
        uint32_t pickBuffers[] = { DrawBuffer::None, DrawBuffer::Attachment1 };
        RenderCommand::SetDrawBuffers(pickBuffers, 2);
        
        // 关闭深度写入，保持深度测试（复用 OpaquePass 的深度缓冲区）
        RenderCommand::SetDepthWrite(false);
        RenderCommand::SetDepthFunc(DepthCompareFunc::LessEqual);   // GL_LEQUAL 保证相同深度可通过
        
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
        
        // 恢复 DrawBuffers：只写入 Attachment 0（颜色），禁用 Attachment 1（EntityID）
        // 防止后续 Gizmo 渲染时向 EntityID 缓冲区写入未定义数据
        uint32_t normalBuffers[] = { DrawBuffer::Attachment0, DrawBuffer::None };
        RenderCommand::SetDrawBuffers(normalBuffers, 2);
        RenderCommand::SetDepthWrite(true);
        RenderCommand::SetDepthFunc(DepthCompareFunc::Less);
    }
}
