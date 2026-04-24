#include "lcpch.h"
#include "OpaquePass.h"
#include "Lucky/Renderer/RenderContext.h"
#include "Lucky/Renderer/RenderCommand.h"

namespace Lucky
{
    void OpaquePass::Execute(const RenderContext& context)
    {
        if (!context.OpaqueDrawCommands || context.OpaqueDrawCommands->empty())
        {
            return;
        }
        
        // ---- 批量绘制不透明物体（DrawCommands 已在外部按 SortKey 排序） ----
        uint32_t lastShaderID = 0;      // 跟踪上一次绑定的 Shader
        Material* lastMaterial = nullptr;   // 跟踪上一次应用的材质
        
        for (const DrawCommand& cmd : *context.OpaqueDrawCommands)
        {
            // 绑定 Shader（仅在 Shader 变化时绑定，减少状态切换）
            uint32_t currentShaderID = cmd.MaterialData->GetShader()->GetRendererID();
            if (currentShaderID != lastShaderID)
            {
                cmd.MaterialData->GetShader()->Bind();
                lastShaderID = currentShaderID;
            }
            
            // 设置引擎内部 uniform
            cmd.MaterialData->GetShader()->SetMat4("u_ObjectToWorldMatrix", cmd.Transform);
            
            // 应用材质属性（仅在材质变化时应用，减少冗余 Uniform 设置）
            if (cmd.MaterialData.get() != lastMaterial)
            {
                cmd.MaterialData->Apply();
                lastMaterial = cmd.MaterialData.get();
            }
            
            // 绘制
            RenderCommand::DrawIndexedRange(
                cmd.MeshData->GetVertexArray(),
                cmd.SubMeshPtr->IndexOffset,
                cmd.SubMeshPtr->IndexCount
            );
            
            // 更新统计
            if (context.Stats)
            {
                context.Stats->DrawCalls++;
                context.Stats->TriangleCount += cmd.SubMeshPtr->IndexCount / 3;
            }
        }
    }
}
