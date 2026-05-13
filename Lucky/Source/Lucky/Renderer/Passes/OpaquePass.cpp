#include "lcpch.h"
#include "OpaquePass.h"
#include "Lucky/Renderer/RenderContext.h"
#include "Lucky/Renderer/RenderCommand.h"
#include "Lucky/Renderer/RenderState.h"

namespace Lucky
{
    void OpaquePass::Execute(const RenderContext& context)
    {
        // ---- 绑定 HDR FBO 作为渲染目标（如果可用） ----
        if (context.HDR_FBO)
        {
            context.HDR_FBO->Bind();
            
            // 将清屏颜色从 sRGB 空间转换到线性空间
            // 用户设置的清屏颜色是 sRGB 值，HDR FBO 工作在线性空间
            // 后续 Tonemapping + Gamma 校正会将线性值转回 sRGB，确保最终显示与用户设置一致
            glm::vec4 linearClearColor = glm::vec4(
                glm::pow(glm::vec3(context.ClearColor), glm::vec3(2.2f)),
                context.ClearColor.a
            );
            RenderCommand::SetClearColor(linearClearColor);
            RenderCommand::Clear();
            context.HDR_FBO->ClearAttachment(1, -1);  // 清除 Entity ID 缓冲区
        }
        
        if (!context.OpaqueDrawCommands || context.OpaqueDrawCommands->empty())
        {
            return;
        }
        
        // ---- 绑定 Shadow Map 纹理 ----
        if (context.ShadowEnabled && context.CascadeShadowMapArrayTextureID != 0)
        {
            // 绑定 CSM Texture2DArray 到纹理单元 15
            RenderCommand::BindTextureUnit(15, context.CascadeShadowMapArrayTextureID);

            // 绑定 Translucent Shadow Map 纹理（纹理单元 8，仅 Cascade 0 的颜色衰减）
            if (context.TranslucentShadowEnabled && context.TranslucentShadowMapTextureID != 0)
            {
                RenderCommand::BindTextureUnit(8, context.TranslucentShadowMapTextureID);
            }
        }
        
        // ---- 批量绘制不透明物体（DrawCommands 已在外部按 SortKey 排序） ----
        
        // 状态跟踪（避免重复设置 GPU 状态）
        RenderState lastRenderState;        // 上一次应用的渲染状态（默认值 = 引擎默认状态）
        uint32_t lastShaderID = 0;          // 跟踪上一次绑定的 Shader
        Material* lastMaterial = nullptr;   // 跟踪上一次应用的材质
        
        for (const DrawCommand& cmd : *context.OpaqueDrawCommands)
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
                // CSM uniform
                cmd.MaterialData->GetShader()->SetInt("u_ShadowMapArray", 15);
                cmd.MaterialData->GetShader()->SetInt("u_CascadeCount", context.CascadeCount);
                
                for (int i = 0; i < context.CascadeCount; ++i)
                {
                    std::string matName = "u_CascadeLightSpaceMatrices[" + std::to_string(i) + "]";
                    cmd.MaterialData->GetShader()->SetMat4(matName, context.CascadeLightSpaceMatrices[i]);
                    
                    std::string farName = "u_CascadeFarPlanes[" + std::to_string(i) + "]";
                    cmd.MaterialData->GetShader()->SetFloat(farName, context.CascadeFarPlanes[i]);
                }
                
                cmd.MaterialData->GetShader()->SetMat4("u_CameraViewMatrix", context.CameraViewMatrix);
                cmd.MaterialData->GetShader()->SetFloat("u_ShadowBias", context.ShadowBias);
                cmd.MaterialData->GetShader()->SetFloat("u_ShadowStrength", context.ShadowStrength);
                cmd.MaterialData->GetShader()->SetInt("u_ShadowEnabled", 1);
                cmd.MaterialData->GetShader()->SetInt("u_ShadowType", static_cast<int>(context.ShadowShadowType));

                // Translucent Shadow Map（纹理单元 8，仅 Cascade 0 的颜色衰减）
                cmd.MaterialData->GetShader()->SetInt("u_TranslucentShadowMap", 8);
                cmd.MaterialData->GetShader()->SetInt("u_TranslucentShadowEnabled", context.TranslucentShadowEnabled ? 1 : 0);
            }
            else
            {
                cmd.MaterialData->GetShader()->SetInt("u_ShadowEnabled", 0);
                cmd.MaterialData->GetShader()->SetInt("u_TranslucentShadowEnabled", 0);
            }
            
            // CSM 调试可视化
            cmd.MaterialData->GetShader()->SetInt("u_DebugCSMVisualize", context.DebugCSMVisualize ? 1 : 0);
            
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
