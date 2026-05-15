#include "lcpch.h"
#include "TransparentPass.h"
#include "Lucky/Renderer/RenderContext.h"
#include "Lucky/Renderer/RenderCommand.h"
#include "Lucky/Renderer/RenderState.h"
#include "Lucky/Renderer/IBLPrecompute.h"

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
        if (context.ShadowEnabled && context.CascadeShadowMapArrayTextureID != 0)
        {
            // 绑定 CSM Texture2DArray 到纹理单元 15
            RenderCommand::BindTextureUnit(15, context.CascadeShadowMapArrayTextureID);

            // 绑定 Translucent Shadow Map 纹理（纹理单元 8），使透明物体能接收其他透明物体的阴影
            // 自阴影问题可接受：Translucent Shadow Map 使用乘法混合且透明物体不写入深度，
            // 自阴影表现为物体自身颜色略微加深，不会产生错误的黑色条纹
            if (context.TranslucentShadowEnabled && context.TranslucentShadowMapTextureID != 0)
            {
                RenderCommand::BindTextureUnit(8, context.TranslucentShadowMapTextureID);
            }
        }
        
        // ---- 绑定 Shadow Atlas 纹理（聚光灯 + 点光源阴影） ----
        if ((context.ShadowData.SpotLightShadowCount > 0 || context.ShadowData.PointLightShadowCount > 0) && context.ShadowAtlasTextureID != 0)
        {
            RenderCommand::BindTextureUnit(14, context.ShadowAtlasTextureID);
        }
        
        // ---- 绑定 IBL 纹理（始终绑定，避免 sampler 未绑定导致未定义行为） ----
        if (context.IBLEnabled)
        {
            RenderCommand::BindTextureUnit(10, context.IrradianceMapID);
            RenderCommand::BindTextureUnit(11, context.PrefilterMapID);
            RenderCommand::BindTextureUnit(12, context.BRDFLUTID);
        }
        else
        {
            // IBL 关闭时绑定默认黑色纹理，确保 sampler 始终有效
            const IBLData& iblData = IBLPrecompute::GetIBLData();
            RenderCommand::BindTextureUnit(10, iblData.DefaultBlackCubemapID);
            RenderCommand::BindTextureUnit(11, iblData.DefaultBlackCubemapID);
            RenderCommand::BindTextureUnit(12, iblData.DefaultBlack2DID);
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
            
            // 设置阴影相关 uniform（CSM 版本）
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

                // 启用 Translucent Shadow Map 采样，使透明物体能接收其他透明物体的阴影
                cmd.MaterialData->GetShader()->SetInt("u_TranslucentShadowMap", 8);
                cmd.MaterialData->GetShader()->SetInt("u_TranslucentShadowEnabled", context.TranslucentShadowEnabled ? 1 : 0);
            }
            else
            {
                cmd.MaterialData->GetShader()->SetInt("u_ShadowEnabled", 0);
                cmd.MaterialData->GetShader()->SetInt("u_TranslucentShadowEnabled", 0);
            }
            
            // ---- 聚光灯阴影 uniform ----
            if (context.ShadowData.SpotLightShadowCount > 0)
            {
                cmd.MaterialData->GetShader()->SetInt("u_ShadowAtlas", 14);
                cmd.MaterialData->GetShader()->SetFloat("u_ShadowAtlasSize", static_cast<float>(context.ShadowAtlasSize));
                cmd.MaterialData->GetShader()->SetInt("u_SpotShadowCount", context.ShadowData.SpotLightShadowCount);

                for (int i = 0; i < context.ShadowData.SpotLightShadowCount; ++i)
                {
                    const auto& spotShadow = context.ShadowData.SpotLights[i];
                    std::string idx = std::to_string(i);

                    cmd.MaterialData->GetShader()->SetInt("u_SpotShadowLightIndex[" + idx + "]", spotShadow.LightIndex);
                    cmd.MaterialData->GetShader()->SetMat4("u_SpotShadowLightSpaceMatrices[" + idx + "]", spotShadow.LightSpaceMatrix);
                    cmd.MaterialData->GetShader()->SetFloat4("u_SpotShadowAtlasScaleBias[" + idx + "]", spotShadow.AtlasScaleBias);
                    cmd.MaterialData->GetShader()->SetFloat("u_SpotShadowBias[" + idx + "]", spotShadow.ShadowBias);
                    cmd.MaterialData->GetShader()->SetFloat("u_SpotShadowStrength[" + idx + "]", spotShadow.ShadowStrength);
                    cmd.MaterialData->GetShader()->SetInt("u_SpotShadowType[" + idx + "]", spotShadow.ShadowType);
                }
            }
            else
            {
                cmd.MaterialData->GetShader()->SetInt("u_SpotShadowCount", 0);
            }
            
            // ---- 点光源阴影 uniform ----
            if (context.ShadowData.PointLightShadowCount > 0)
            {
                cmd.MaterialData->GetShader()->SetInt("u_ShadowAtlas", 14);
                cmd.MaterialData->GetShader()->SetFloat("u_ShadowAtlasSize", static_cast<float>(context.ShadowAtlasSize));
                cmd.MaterialData->GetShader()->SetInt("u_PointShadowCount", context.ShadowData.PointLightShadowCount);

                for (int i = 0; i < context.ShadowData.PointLightShadowCount; ++i)
                {
                    const auto& pointShadow = context.ShadowData.PointLights[i];
                    std::string idx = std::to_string(i);

                    cmd.MaterialData->GetShader()->SetInt("u_PointShadowLightIndex[" + idx + "]", pointShadow.LightIndex);
                    cmd.MaterialData->GetShader()->SetFloat("u_PointShadowBias[" + idx + "]", pointShadow.ShadowBias);
                    cmd.MaterialData->GetShader()->SetFloat("u_PointShadowStrength[" + idx + "]", pointShadow.ShadowStrength);
                    cmd.MaterialData->GetShader()->SetInt("u_PointShadowType[" + idx + "]", pointShadow.ShadowType);
                    cmd.MaterialData->GetShader()->SetFloat("u_PointShadowFarPlane[" + idx + "]", pointShadow.FarPlane);
                    cmd.MaterialData->GetShader()->SetFloat3("u_PointShadowLightPos[" + idx + "]", pointShadow.LightPos);

                    for (int face = 0; face < 6; ++face)
                    {
                        std::string tileIdx = std::to_string(i * 6 + face);
                        cmd.MaterialData->GetShader()->SetMat4("u_PointShadowLightSpaceMatrices[" + tileIdx + "]", pointShadow.LightSpaceMatrices[face]);
                        cmd.MaterialData->GetShader()->SetFloat4("u_PointShadowAtlasScaleBias[" + tileIdx + "]", pointShadow.AtlasScaleBias[face]);
                    }
                }
            }
            else
            {
                cmd.MaterialData->GetShader()->SetInt("u_PointShadowCount", 0);
            }
            
            // 设置 IBL 相关 uniform（始终设置采样器绑定，确保 sampler 指向有效纹理单元）
            cmd.MaterialData->GetShader()->SetInt("u_IBLEnabled", context.IBLEnabled ? 1 : 0);
            cmd.MaterialData->GetShader()->SetInt("u_IrradianceMap", 10);
            cmd.MaterialData->GetShader()->SetInt("u_PrefilterMap", 11);
            cmd.MaterialData->GetShader()->SetInt("u_BRDFLUT", 12);
            cmd.MaterialData->GetShader()->SetFloat("u_PrefilterMaxMipLevel", context.PrefilterMaxMipLevel);
            
            // 设置环境设置 uniform
            cmd.MaterialData->GetShader()->SetInt("u_AmbientSource", static_cast<int>(context.EnvironmentSource));
            cmd.MaterialData->GetShader()->SetFloat3("u_AmbientColor", context.AmbientColor);
            cmd.MaterialData->GetShader()->SetFloat("u_IBLDiffuseIntensity", context.IBLDiffuseIntensity);
            cmd.MaterialData->GetShader()->SetFloat("u_IBLSpecularIntensity", context.IBLSpecularIntensity);
            
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
    }
}
