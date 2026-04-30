#include "lcpch.h"
#include "TransparentPass.h"
#include "Lucky/Renderer/RenderContext.h"
#include "Lucky/Renderer/RenderCommand.h"
#include "Lucky/Renderer/RenderState.h"

namespace Lucky
{
    void TransparentPass::Execute(const RenderContext& context)
    {
        if (!context.TransparentDrawCommands || context.TransparentDrawCommands->empty())
        {
            return;
        }
        
        // ---- 注意：HDR FBO 已在 OpaquePass 中绑定，此处无需重新绑定 ----
        // ---- 深度缓冲区已由 OpaquePass 写入，透明物体可以正确被遮挡 ----
        
        // ---- 绑定 Shadow Map 纹理 ----
        if (context.ShadowEnabled && context.ShadowMapTextureID != 0)
        {
            RenderCommand::BindTextureUnit(15, context.ShadowMapTextureID);

            // 绑定 Translucent Shadow Map 纹理（透明物体颜色衰减）
            if (context.TranslucentShadowEnabled && context.TranslucentShadowMapTextureID != 0)
            {
                RenderCommand::BindTextureUnit(14, context.TranslucentShadowMapTextureID);
            }
        }
        
        // ---- 批量绘制透明物体（DrawCommands 已按距离从远到近排序） ----
        
        // 状态跟踪（避免重复设置 GPU 状态）
        RenderState lastRenderState;        // 上一次应用的渲染状态（默认值 = 引擎默认状态）
        uint32_t lastShaderID = 0;          // 跟踪上一次绑定的 Shader
        Material* lastMaterial = nullptr;   // 跟踪上一次应用的材质
        
        for (const DrawCommand& cmd : *context.TransparentDrawCommands)
        {
            const RenderState& state = cmd.MaterialData->GetRenderState();
            
            // ---- 应用渲染状态（仅在变化时设置） ----
            
            if (state.Cull != lastRenderState.Cull)
            {
                RenderCommand::SetCullMode(state.Cull);
            }
            
            if (state.DepthWrite != lastRenderState.DepthWrite)
            {
                RenderCommand::SetDepthWrite(state.DepthWrite);
            }
            
            if (state.DepthTest != lastRenderState.DepthTest)
            {
                RenderCommand::SetDepthFunc(state.DepthTest);
            }
            
            if (state.Blend != lastRenderState.Blend)
            {
                RenderCommand::SetBlendMode(state.Blend);
            }
            
            lastRenderState = state;
            
            // 绑定 Shader（仅在 Shader 变化时绑定，减少状态切换）
            uint32_t currentShaderID = cmd.MaterialData->GetShader()->GetRendererID();
            if (currentShaderID != lastShaderID)
            {
                cmd.MaterialData->GetShader()->Bind();
                lastShaderID = currentShaderID;
            }
            
            // 应用材质属性（仅在材质变化时应用，减少冗余 Uniform 设置）
            if (cmd.MaterialData.get() != lastMaterial)
            {
                cmd.MaterialData->Apply();
                lastMaterial = cmd.MaterialData.get();
            }
            
            // 设置引擎内部 uniform（在 Material::Apply() 之后，确保不被覆盖）
            cmd.MaterialData->GetShader()->SetMat4("u_ObjectToWorldMatrix", cmd.Transform);
            
            // 设置阴影相关 uniform（在 Material::Apply() 之后，确保不被覆盖）
            if (context.ShadowEnabled)
            {
                cmd.MaterialData->GetShader()->SetInt("u_ShadowMap", 15);
                cmd.MaterialData->GetShader()->SetMat4("u_LightSpaceMatrix", context.LightSpaceMatrix);
                cmd.MaterialData->GetShader()->SetFloat("u_ShadowBias", context.ShadowBias);
                cmd.MaterialData->GetShader()->SetFloat("u_ShadowStrength", context.ShadowStrength);
                cmd.MaterialData->GetShader()->SetInt("u_ShadowEnabled", 1);
                cmd.MaterialData->GetShader()->SetInt("u_ShadowType", static_cast<int>(context.ShadowShadowType));

                // Translucent Shadow Map
                cmd.MaterialData->GetShader()->SetInt("u_TranslucentShadowMap", 14);
                cmd.MaterialData->GetShader()->SetInt("u_TranslucentShadowEnabled", context.TranslucentShadowEnabled ? 1 : 0);
            }
            else
            {
                cmd.MaterialData->GetShader()->SetInt("u_ShadowEnabled", 0);
                cmd.MaterialData->GetShader()->SetInt("u_TranslucentShadowEnabled", 0);
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
        
        // ---- 深度补写阶段 ----
        // 透明物体混合渲染时 DepthWrite=false，深度缓冲中没有透明物体的深度信息
        // 导致后续 Gizmo 绘制时深度测试通过，Gizmo 会显示在透明物体前面
        // 此阶段对所有透明物体重新绘制一遍，只写深度不写颜色，确保 Gizmo 能被正确遮挡
        RenderCommand::SetColorMask(false, false, false, false);    // 禁用颜色写入
        RenderCommand::SetDepthWrite(true);                         // 启用深度写入
        RenderCommand::SetBlendMode(BlendMode::None);               // 关闭混合（深度补写不需要混合）

        for (const DrawCommand& cmd : *context.TransparentDrawCommands)
        {
            // 绑定 Shader（复用材质的 Shader，仅需要顶点变换写入深度）
            cmd.MaterialData->GetShader()->Bind();
            cmd.MaterialData->Apply();
            cmd.MaterialData->GetShader()->SetMat4("u_ObjectToWorldMatrix", cmd.Transform);

            RenderCommand::DrawIndexedRange(
                cmd.MeshData->GetVertexArray(),
                cmd.SubMeshPtr->IndexOffset,
                cmd.SubMeshPtr->IndexCount
            );
        }

        RenderCommand::SetColorMask(true, true, true, true);    // 恢复颜色写入

        // 绘制结束后恢复默认渲染状态
        RenderCommand::SetCullMode(CullMode::Back);
        RenderCommand::SetDepthWrite(true);
        RenderCommand::SetDepthFunc(DepthCompareFunc::Less);
        RenderCommand::SetBlendMode(BlendMode::None);
    }
}
