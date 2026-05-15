#include "lcpch.h"
#include "ShadowPass.h"
#include "Lucky/Renderer/RenderContext.h"
#include "Lucky/Renderer/RenderCommand.h"
#include "Lucky/Renderer/Renderer3D.h"
#include "Lucky/Renderer/Material.h"
#include "Lucky/Renderer/Texture.h"

#include "Lucky/UI/DrawUtils.h"
#include "Lucky/UI/PropertyGrid.h"

namespace Lucky
{
    void ShadowPass::Init()
    {
        m_ShadowShader = Renderer3D::GetShaderLibrary()->Get("Shadow");
        m_PointShadowShader = Renderer3D::GetShaderLibrary()->Get("PointShadow");
        RecreateFBOs();

        // 初始化 Shadow Atlas
        m_ShadowAtlas.Init(4096);
    }

    void ShadowPass::RecreateFBOs()
    {
        // 创建 CSM Texture2DArray FBO（多层深度纹理）
        FramebufferSpecification csmSpec;
        csmSpec.Width = m_ShadowMapResolution;
        csmSpec.Height = m_ShadowMapResolution;
        csmSpec.Layers = m_CascadeCount;
        csmSpec.Attachments = {
            FramebufferTextureFormat::DEPTH_COMPONENT_ARRAY
        };
        m_CSMFramebuffer = Framebuffer::Create(csmSpec);

        // 创建 Translucent Shadow Map FBO（RGBA8 Array 颜色 + 深度 Array，所有级联）
        FramebufferSpecification translucentSpec;
        translucentSpec.Width = m_ShadowMapResolution;
        translucentSpec.Height = m_ShadowMapResolution;
        translucentSpec.Layers = m_CascadeCount;
        translucentSpec.Attachments = {
            FramebufferTextureFormat::RGBA8_ARRAY,           // Translucent Shadow Map 颜色衰减（Texture2DArray）
            FramebufferTextureFormat::DEPTH_COMPONENT_ARRAY  // 深度（Texture2DArray，用于深度测试）
        };
        m_TranslucentShadowFBO = Framebuffer::Create(translucentSpec);
    }

    void ShadowPass::Execute(const RenderContext& context)
    {
        // 条件执行：仅当有可渲染物体时执行
        bool hasOpaque = context.OpaqueDrawCommands && !context.OpaqueDrawCommands->empty();
        bool hasTransparent = context.TransparentDrawCommands && !context.TransparentDrawCommands->empty();

        if (!hasOpaque && !hasTransparent)
        {
            return;
        }

        // ======== 阶段 1 & 2：方向光 CSM（仅当方向光阴影启用时） ========
        if (context.ShadowEnabled)
        {
            // 检查是否需要重建 FBO（分辨率或级联数量变化）
            if (context.ShadowMapResolution != (int)m_ShadowMapResolution || context.CascadeCount != m_CascadeCount)
            {
                m_ShadowMapResolution = context.ShadowMapResolution;
                m_CascadeCount = context.CascadeCount;
                RecreateFBOs();
            }

        // ======== 阶段 1：CSM 深度渲染（所有级联，仅不透明物体） ========
        m_CSMFramebuffer->Bind();
        RenderCommand::SetViewport(0, 0, m_ShadowMapResolution, m_ShadowMapResolution);
        RenderCommand::SetCullMode(CullMode::Off);

        m_ShadowShader->Bind();
        m_ShadowShader->SetInt("u_AlphaTestEnabled", 0);
        m_ShadowShader->SetInt("u_TranslucentShadowEnabled", 0);

        // 逐级联渲染深度
        for (int i = 0; i < context.CascadeCount; ++i)
        {
            // 切换到第 i 层
            m_CSMFramebuffer->BindDepthLayer(i);
            RenderCommand::Clear();

            // 设置当前级联的 Light Space Matrix
            m_ShadowShader->SetMat4("u_LightSpaceMatrix", context.CascadeLightSpaceMatrices[i]);

            // 渲染所有不透明物体
            if (hasOpaque)
            {
                for (const DrawCommand& cmd : *context.OpaqueDrawCommands)
                {
                    m_ShadowShader->SetMat4("u_ObjectToWorldMatrix", cmd.Transform);

                    RenderCommand::DrawIndexedRange(
                        cmd.MeshData->GetVertexArray(),
                        cmd.SubMeshPtr->IndexOffset,
                        cmd.SubMeshPtr->IndexCount
                    );
                }
            }
        }

        m_CSMFramebuffer->Unbind();

        // ======== 阶段 2：Translucent Shadow Map（所有级联，透明物体颜色衰减） ========
        if (!hasTransparent)
        {
            // 没有透明物体时，清除 Translucent Shadow Map 为白色（无衰减），
            // 防止上一帧残留的颜色衰减数据被 OpaquePass 采样导致阴影残留
            m_TranslucentShadowFBO->Bind();
            RenderCommand::SetDepthWrite(true);
            RenderCommand::SetColorMask(true, true, true, true);
            RenderCommand::SetBlendMode(BlendMode::None);
            RenderCommand::SetClearColor(glm::vec4(1.0f, 1.0f, 1.0f, 1.0f));
            for (int i = 0; i < context.CascadeCount; ++i)
            {
                m_TranslucentShadowFBO->BindColorLayer(0, i);
                m_TranslucentShadowFBO->BindDepthLayer(i);
                RenderCommand::Clear();
            }
            m_TranslucentShadowFBO->Unbind();
        }
        else
        {
            m_TranslucentShadowFBO->Bind();
            RenderCommand::SetViewport(0, 0, m_ShadowMapResolution, m_ShadowMapResolution);
            RenderCommand::SetCullMode(CullMode::Off);
            m_ShadowShader->Bind();

            const auto& defaultWhiteTexture = Renderer3D::GetDefaultTexture(TextureDefault::White);

            // 逐级联渲染
            for (int i = 0; i < context.CascadeCount; ++i)
            {
                // 切换到第 i 层（颜色 + 深度）
                m_TranslucentShadowFBO->BindColorLayer(0, i);
                m_TranslucentShadowFBO->BindDepthLayer(i);

                // 恢复渲染状态，确保 Clear 能正确清除颜色和深度缓冲区
                RenderCommand::SetDepthWrite(true);
                RenderCommand::SetColorMask(true, true, true, true);
                RenderCommand::SetBlendMode(BlendMode::None);

                // 清除深度（1.0）和颜色（白色 = 无衰减）
                RenderCommand::SetClearColor(glm::vec4(1.0f, 1.0f, 1.0f, 1.0f));
                RenderCommand::Clear();

                // 设置当前级联的 Light Space Matrix
                m_ShadowShader->SetMat4("u_LightSpaceMatrix", context.CascadeLightSpaceMatrices[i]);

                // 先渲染不透明物体的深度（用于深度测试，确保透明物体在不透明物体之后）
                RenderCommand::SetColorMask(false, false, false, false);
                RenderCommand::SetDepthWrite(true);
                m_ShadowShader->SetInt("u_AlphaTestEnabled", 0);
                m_ShadowShader->SetInt("u_TranslucentShadowEnabled", 0);

                if (hasOpaque)
                {
                    for (const DrawCommand& cmd : *context.OpaqueDrawCommands)
                    {
                        m_ShadowShader->SetMat4("u_ObjectToWorldMatrix", cmd.Transform);
                        RenderCommand::DrawIndexedRange(
                            cmd.MeshData->GetVertexArray(),
                            cmd.SubMeshPtr->IndexOffset,
                            cmd.SubMeshPtr->IndexCount
                        );
                    }
                }

                // 渲染透明物体（颜色衰减，乘法混合）
                RenderCommand::SetDepthWrite(false);
                RenderCommand::SetColorMask(true, true, true, true);
                RenderCommand::SetBlendMode(BlendMode::Zero_SrcColor);

                m_ShadowShader->SetInt("u_AlphaTestEnabled", 1);
                m_ShadowShader->SetInt("u_TranslucentShadowEnabled", 1);
                m_ShadowShader->SetFloat("u_AlphaTestThreshold", 0.5f);

                for (const DrawCommand& cmd : *context.TransparentDrawCommands)
                {
                    m_ShadowShader->SetMat4("u_ObjectToWorldMatrix", cmd.Transform);

                    glm::vec4 albedo = cmd.MaterialData->GetFloat4("u_Albedo");
                    m_ShadowShader->SetFloat4("u_Albedo", albedo);

                    Ref<Texture2D> albedoMap = cmd.MaterialData->GetTexture("u_AlbedoMap");
                    if (albedoMap)
                    {
                        albedoMap->Bind(0);
                    }
                    else
                    {
                        defaultWhiteTexture->Bind(0);
                    }
                    m_ShadowShader->SetInt("u_AlbedoMap", 0);

                    RenderCommand::DrawIndexedRange(
                        cmd.MeshData->GetVertexArray(),
                        cmd.SubMeshPtr->IndexOffset,
                        cmd.SubMeshPtr->IndexCount
                    );
                }
            }

            m_TranslucentShadowFBO->Unbind();
        }

        } // end if (context.ShadowEnabled)

        // ======== 阶段 3：聚光灯 + 点光源阴影（Shadow Atlas） ========
        bool hasAtlasShadows = (context.ShadowData.SpotLightShadowCount > 0 || context.ShadowData.PointLightShadowCount > 0);
        if (hasAtlasShadows && hasOpaque)
        {
            // 重置 Atlas 帧状态
            m_ShadowAtlas.BeginFrame();

            // 绑定 Atlas FBO
            m_ShadowAtlas.GetFramebuffer()->Bind();

            // 清除整个 Atlas 深度为 1.0（最远）
            RenderCommand::SetViewport(0, 0, m_ShadowAtlas.GetAtlasSize(), m_ShadowAtlas.GetAtlasSize());
            RenderCommand::Clear();

            // 渲染聚光灯阴影
            if (context.ShadowData.SpotLightShadowCount > 0)
            {
                RenderSpotLightShadows(context);
            }

            // 渲染点光源阴影
            if (context.ShadowData.PointLightShadowCount > 0)
            {
                RenderPointLightShadows(context);
            }

            // 解绑 Atlas FBO
            m_ShadowAtlas.GetFramebuffer()->Unbind();
        }

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
    }

    void ShadowPass::RenderSpotLightShadows(const RenderContext& context)
    {
        // 启用 Scissor Test（防止渲染溢出到相邻 Tile）
        RenderCommand::SetScissorTest(true);
        RenderCommand::SetCullMode(CullMode::Off);

        m_ShadowShader->Bind();
        m_ShadowShader->SetInt("u_AlphaTestEnabled", 0);
        m_ShadowShader->SetInt("u_TranslucentShadowEnabled", 0);

        // 逐聚光灯渲染
        for (int i = 0; i < context.ShadowData.SpotLightShadowCount; ++i)
        {
            const auto& spotShadow = context.ShadowData.SpotLights[i];
            int tileIdx = m_ShadowAtlas.GetSpotLightTileIndex(i);
            const ShadowAtlasTile& tile = m_ShadowAtlas.GetTile(tileIdx);

            // 激活 Tile（设置 LightSpaceMatrix）
            m_ShadowAtlas.ActivateSpotTile(i, spotShadow.LightSpaceMatrix);

            // 设置视口和裁剪区域到 Tile 范围
            RenderCommand::SetViewport(tile.X, tile.Y, tile.Width, tile.Height);
            RenderCommand::SetScissor(tile.X, tile.Y, tile.Width, tile.Height);

            // 清除当前 Tile 区域的深度
            RenderCommand::Clear();

            // 设置 Light Space Matrix
            m_ShadowShader->SetMat4("u_LightSpaceMatrix", spotShadow.LightSpaceMatrix);

            // 渲染所有不透明物体
            for (const DrawCommand& cmd : *context.OpaqueDrawCommands)
            {
                m_ShadowShader->SetMat4("u_ObjectToWorldMatrix", cmd.Transform);

                RenderCommand::DrawIndexedRange(
                    cmd.MeshData->GetVertexArray(),
                    cmd.SubMeshPtr->IndexOffset,
                    cmd.SubMeshPtr->IndexCount
                );
            }
        }

        // 关闭 Scissor Test
        RenderCommand::SetScissorTest(false);
    }

    void ShadowPass::RenderPointLightShadows(const RenderContext& context)
    {
        // 启用 Scissor Test（防止渲染溢出到相邻 Tile）
        RenderCommand::SetScissorTest(true);
        RenderCommand::SetCullMode(CullMode::Off);

        // 使用点光源专用 Shader（线性深度）
        m_PointShadowShader->Bind();

        // 逐点光源渲染
        for (int i = 0; i < context.ShadowData.PointLightShadowCount; ++i)
        {
            const auto& pointShadow = context.ShadowData.PointLights[i];

            // 设置光源位置和远平面（所有 6 面共享）
            m_PointShadowShader->SetFloat3("u_LightPos", pointShadow.LightPos);
            m_PointShadowShader->SetFloat("u_FarPlane", pointShadow.FarPlane);

            // 渲染 6 个面
            for (int face = 0; face < 6; ++face)
            {
                int tileIdx = m_ShadowAtlas.GetPointLightTileStart(i) + face;
                const ShadowAtlasTile& tile = m_ShadowAtlas.GetTile(tileIdx);

                // 激活 Tile
                m_ShadowAtlas.ActivatePointTile(i, face, pointShadow.LightSpaceMatrices[face]);

                // 设置视口和裁剪区域
                RenderCommand::SetViewport(tile.X, tile.Y, tile.Width, tile.Height);
                RenderCommand::SetScissor(tile.X, tile.Y, tile.Width, tile.Height);

                // 清除当前 Tile 深度
                RenderCommand::Clear();

                // 设置 Light Space Matrix
                m_PointShadowShader->SetMat4("u_LightSpaceMatrix", pointShadow.LightSpaceMatrices[face]);

                // 渲染所有不透明物体
                for (const DrawCommand& cmd : *context.OpaqueDrawCommands)
                {
                    m_PointShadowShader->SetMat4("u_ObjectToWorldMatrix", cmd.Transform);

                    RenderCommand::DrawIndexedRange(
                        cmd.MeshData->GetVertexArray(),
                        cmd.SubMeshPtr->IndexOffset,
                        cmd.SubMeshPtr->IndexCount
                    );
                }
            }
        }

        // 关闭 Scissor Test
        RenderCommand::SetScissorTest(false);
    }

    uint32_t ShadowPass::GetShadowAtlasTextureID() const
    {
        return m_ShadowAtlas.GetAtlasTextureID();
    }

    uint32_t ShadowPass::GetShadowMapTextureID() const
    {
        return m_CSMFramebuffer->GetDepthArrayTextureID();
    }

    uint32_t ShadowPass::GetTranslucentShadowMapTextureID() const
    {
        return m_TranslucentShadowFBO->GetColorArrayTextureID(0);
    }

    void ShadowPass::OnDebugGUI()
    {
        UI::Draw::HorizontalLine();
        UI::PropertyCheckbox("Cascade Visualization", m_DebugCSMVisualize);

        // 显示当前 Shadow Map 信息（只读）
        std::string resolution = std::to_string(m_ShadowMapResolution) + "x" + std::to_string(m_ShadowMapResolution);
        UI::PropertyReadOnlyString("Resolution", resolution.c_str());
        
        std::string cascades = std::to_string(m_CascadeCount) + " cascades";
        UI::PropertyReadOnlyString("Cascades", cascades.c_str());
    }
}