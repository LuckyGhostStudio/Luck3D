#include "lcpch.h"
#include "OutlineCompositePass.h"
#include "SilhouettePass.h"
#include "Lucky/Renderer/RenderContext.h"
#include "Lucky/Renderer/RenderCommand.h"
#include "Lucky/Renderer/RenderState.h"
#include "Lucky/Renderer/Renderer3D.h"
#include "Lucky/Renderer/ScreenQuad.h"

namespace Lucky
{
    void OutlineCompositePass::Init()
    {
        m_OutlineCompositeShader = Renderer3D::GetShaderLibrary()->Get("OutlineComposite");
    }
    
    void OutlineCompositePass::Execute(const RenderContext& context)
    {
        // 条件执行：仅当描边启用且有描边物体时执行
        if (!context.OutlineEnabled || !context.OutlineDrawCommands || context.OutlineDrawCommands->empty())
        {
            return;
        }
        
        // ---- 重新绑定主 FBO ----
        context.TargetFramebuffer->Bind();
        
        // 只写入 Attachment 0（颜色），不写入 Attachment 1（EntityID）
        uint32_t outlineBuffers[] = { DrawBuffer::Attachment0, DrawBuffer::None };
        RenderCommand::SetDrawBuffers(outlineBuffers, 2);
        
        // 禁用深度测试（全屏 Quad 不需要深度测试）
        RenderCommand::SetDepthTest(false);
        
        // 启用 Alpha 混合（描边颜色与场景颜色混合）
        RenderCommand::SetBlendMode(BlendMode::SrcAlpha_OneMinusSrcAlpha);
        
        // ---- 绑定 Shader 和纹理 ----
        m_OutlineCompositeShader->Bind();
        
        // 绑定 Silhouette 纹理到纹理单元 0
        RenderCommand::BindTextureUnit(0, m_SilhouettePass->GetSilhouetteTextureID());
        m_OutlineCompositeShader->SetInt("u_SilhouetteTexture", 0);
        
        // 设置描边参数
        m_OutlineCompositeShader->SetFloat4("u_OutlineColor", context.OutlineColor);
        m_OutlineCompositeShader->SetFloat("u_OutlineWidth", context.OutlineWidth);
        
        // ---- 绘制全屏四边形 ----
        ScreenQuad::Draw();
        
        // ---- 恢复渲染状态 ----
        RenderCommand::SetDepthTest(true);
        RenderCommand::SetBlendMode(BlendMode::None);
    }
}
