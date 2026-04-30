#include "lcpch.h"
#include "ShadowPass.h"
#include "Lucky/Renderer/RenderContext.h"
#include "Lucky/Renderer/RenderCommand.h"
#include "Lucky/Renderer/Renderer3D.h"
#include "Lucky/Renderer/Material.h"
#include "Lucky/Renderer/Texture.h"

namespace Lucky
{
    void ShadowPass::Init()
    {
        m_ShadowShader = Renderer3D::GetShaderLibrary()->Get("Shadow");

        // 创建 Shadow Map FBO（深度纹理 + Translucent Shadow Map 颜色纹理）
        // Color Attachment 0: RGBA8 - Translucent Shadow Map（透明物体颜色衰减）
        // Depth Attachment: DEPTH_COMPONENT - 标准 Shadow Map（深度）
        FramebufferSpecification spec;
        spec.Width = m_ShadowMapResolution;
        spec.Height = m_ShadowMapResolution;
        spec.Attachments = {
            FramebufferTextureFormat::RGBA8,            // Translucent Shadow Map
            FramebufferTextureFormat::DEPTH_COMPONENT   // 标准 Shadow Map
        };
        m_ShadowMapFBO = Framebuffer::Create(spec);
    }

    void ShadowPass::Execute(const RenderContext& context)
    {
        // 条件执行：仅当阴影启用且有可渲染物体时执行
        bool hasOpaque = context.OpaqueDrawCommands && !context.OpaqueDrawCommands->empty();
        bool hasTransparent = context.TransparentDrawCommands && !context.TransparentDrawCommands->empty();

        if (!context.ShadowEnabled || (!hasOpaque && !hasTransparent))
        {
            return;
        }

        // ---- 绑定 Shadow Map FBO ----
        m_ShadowMapFBO->Bind();
        RenderCommand::SetViewport(0, 0, m_ShadowMapResolution, m_ShadowMapResolution);

        // 清除深度（1.0）和颜色（白色 = 无衰减）
        RenderCommand::SetClearColor(glm::vec4(1.0f, 1.0f, 1.0f, 1.0f));
        RenderCommand::Clear();

        // ---- 设置渲染状态：关闭面剔除（双面渲染，避免薄物体/近距离阴影缺失） ----
        RenderCommand::SetCullMode(CullMode::Off);

        // ---- 绑定 Shader 并设置 Light Space Matrix ----
        m_ShadowShader->Bind();
        m_ShadowShader->SetMat4("u_LightSpaceMatrix", context.LightSpaceMatrix);

        // ---- 遍历不透明物体 DrawCommand 列表 ----
        if (hasOpaque)
        {
            // 不透明物体: 只写深度, 不写颜色（不影响 Translucent Shadow Map）
            RenderCommand::SetColorMask(false, false, false, false);
            m_ShadowShader->SetInt("u_AlphaTestEnabled", 0);
            m_ShadowShader->SetInt("u_TranslucentShadowEnabled", 0);

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

        // ---- 遍历透明物体 DrawCommand 列表（Dithered Shadow + Translucent Shadow Map） ----
        // 透明物体: 写深度（Dithered）+ 写颜色（Translucent Shadow Map 乘法混合）
        if (hasTransparent)
        {
            // 启用颜色写入 + 乘法混合: Dst = Dst * Src
            // 每个透明物体的透射颜色与已有值相乘, 累积衰减
            RenderCommand::SetColorMask(true, true, true, true);
            RenderCommand::SetBlendMode(BlendMode::Zero_SrcColor);

            m_ShadowShader->SetInt("u_AlphaTestEnabled", 1);
            m_ShadowShader->SetInt("u_TranslucentShadowEnabled", 1);
            m_ShadowShader->SetFloat("u_AlphaTestThreshold", 0.5f);

            // 获取默认白色纹理（当材质没有 AlbedoMap 时使用）
            const auto& defaultWhiteTexture = Renderer3D::GetDefaultTexture(TextureDefault::White);

            for (const DrawCommand& cmd : *context.TransparentDrawCommands)
            {
                m_ShadowShader->SetMat4("u_ObjectToWorldMatrix", cmd.Transform);

                // 传递材质的 Albedo 颜色（包含 Alpha）
                glm::vec4 albedo = cmd.MaterialData->GetFloat4("u_Albedo");
                m_ShadowShader->SetFloat4("u_Albedo", albedo);

                // 绑定材质的 AlbedoMap 纹理（无纹理时使用默认白色纹理）
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

        // ---- 恢复渲染状态 ----
        RenderCommand::SetBlendMode(BlendMode::None);
        RenderCommand::SetColorMask(true, true, true, true);
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

    uint32_t ShadowPass::GetTranslucentShadowMapTextureID() const
    {
        return m_ShadowMapFBO->GetColorAttachmentRendererID(0);
    }
}