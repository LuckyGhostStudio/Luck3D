#include "lcpch.h"
#include "SkyboxPass.h"
#include "Lucky/Renderer/RenderContext.h"
#include "Lucky/Renderer/RenderCommand.h"
#include "Lucky/Renderer/RenderState.h"
#include "Lucky/Renderer/Material.h"

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

namespace Lucky
{
    void SkyboxPass::Init()
    {
        // 空 VAO：顶点在 Shader 中硬编码，通过 gl_VertexID 索引
        m_QuadVAO = VertexArray::Create();
    }
    
    void SkyboxPass::Execute(const RenderContext& context)
    {
        // 如果没有设置天空盒材质，跳过
        if (!context.SkyboxMaterial)
        {
            return;
        }
        
        // ---- 确保渲染目标正确（HDR FBO 应由 OpaquePass 绑定） ----
        if (context.HDR_FBO)
        {
            context.HDR_FBO->Bind();
        }
        
        // 确保 DrawBuffers 状态正确：只写入颜色附件 0
        uint32_t drawBuffers[] = { DrawBuffer::Attachment0, DrawBuffer::None };
        RenderCommand::SetDrawBuffers(drawBuffers, 2);
        
        // ---- 设置渲染状态 ----
        RenderCommand::SetDepthFunc(DepthCompareFunc::LessEqual);   // 深度 = 1.0 通过测试
        RenderCommand::SetDepthWrite(false);                        // 不写入深度
        RenderCommand::SetCullMode(CullMode::Back);                 // 全屏 Quad 无所谓，保持默认
        
        // ---- 通过 Material 绑定 Shader 并上传材质属性 ----
        auto& material = context.SkyboxMaterial;
        auto shader = material->GetShader();
        shader->Bind();
        material->Apply();  // 上传 u_SkyboxMap / u_Rotation / u_Exposure / u_Tint
        
        // ---- 计算并上传 inverse(VP)：View 移除平移分量，Projection 使用相机原始投影 ----
        // 由 SkyboxPass 自动计算，不作为材质可编辑参数
        glm::mat4 viewNoTranslation = glm::mat4(glm::mat3(context.SkyboxViewMatrix));
        glm::mat4 skyboxVP = context.SkyboxProjectionMatrix * viewNoTranslation;
        glm::mat4 inverseVP = glm::inverse(skyboxVP);
        shader->SetMat4("u_InverseVP", inverseVP);
        
        // ---- 绘制全屏 Quad（顶点由 Shader 内 gl_VertexID 索引硬编码顶点数组生成） ----
        RenderCommand::DrawArrays(m_QuadVAO, 6);
        
        // 更新统计
        if (context.Stats)
        {
            context.Stats->DrawCalls++;
            context.Stats->TriangleCount += 2;  // 全屏 Quad = 2 个三角形
        }
    }
}
