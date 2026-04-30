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
    // 天空盒 Cube 顶点数据（仅位置，36 个顶点，无索引）
    static const float s_SkyboxVertices[] = {
        // +X face
         1.0f, -1.0f, -1.0f,
         1.0f, -1.0f,  1.0f,
         1.0f,  1.0f,  1.0f,
         1.0f,  1.0f,  1.0f,
         1.0f,  1.0f, -1.0f,
         1.0f, -1.0f, -1.0f,
        // -X face
        -1.0f, -1.0f,  1.0f,
        -1.0f, -1.0f, -1.0f,
        -1.0f,  1.0f, -1.0f,
        -1.0f,  1.0f, -1.0f,
        -1.0f,  1.0f,  1.0f,
        -1.0f, -1.0f,  1.0f,
        // +Y face
        -1.0f,  1.0f, -1.0f,
         1.0f,  1.0f, -1.0f,
         1.0f,  1.0f,  1.0f,
         1.0f,  1.0f,  1.0f,
        -1.0f,  1.0f,  1.0f,
        -1.0f,  1.0f, -1.0f,
        // -Y face
        -1.0f, -1.0f,  1.0f,
         1.0f, -1.0f,  1.0f,
         1.0f, -1.0f, -1.0f,
         1.0f, -1.0f, -1.0f,
        -1.0f, -1.0f, -1.0f,
        -1.0f, -1.0f,  1.0f,
        // +Z face
        -1.0f, -1.0f,  1.0f,
        -1.0f,  1.0f,  1.0f,
         1.0f,  1.0f,  1.0f,
         1.0f,  1.0f,  1.0f,
         1.0f, -1.0f,  1.0f,
        -1.0f, -1.0f,  1.0f,
        // -Z face
         1.0f, -1.0f, -1.0f,
         1.0f,  1.0f, -1.0f,
        -1.0f,  1.0f, -1.0f,
        -1.0f,  1.0f, -1.0f,
        -1.0f, -1.0f, -1.0f,
         1.0f, -1.0f, -1.0f
    };
    
    void SkyboxPass::Init()
    {
        // ---- 创建天空盒 Cube VAO（仅位置属性） ----
        m_CubeVAO = VertexArray::Create();
        
        m_CubeVBO = VertexBuffer::Create((float*)s_SkyboxVertices, sizeof(s_SkyboxVertices));
        m_CubeVBO->SetLayout({
            { ShaderDataType::Float3, "a_Position" }
        });
        
        m_CubeVAO->AddVertexBuffer(m_CubeVBO);
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
        RenderCommand::SetDepthWrite(false);                         // 不写入深度
        RenderCommand::SetCullMode(CullMode::Front);                 // 剔除正面（渲染内表面）
        
        // ---- 通过 Material 绑定 Shader 并上传材质属性 ----
        auto& material = context.SkyboxMaterial;
        auto shader = material->GetShader();
        shader->Bind();
        material->Apply();  // 上传所有 Uniform（u_SkyboxMap, u_Rotation, u_Exposure, u_Tint）
        
        // ---- 计算并设置天空盒 VP 矩阵（移除 View 平移分量） ----
        // u_SkyboxVP 由 SkyboxPass 自动计算，不作为材质可编辑参数
        glm::mat4 viewNoTranslation = glm::mat4(glm::mat3(context.SkyboxViewMatrix));
        glm::mat4 skyboxVP = context.SkyboxProjectionMatrix * viewNoTranslation;
        shader->SetMat4("u_SkyboxVP", skyboxVP);
        
        // ---- 绘制天空盒 Cube ----
        RenderCommand::DrawArrays(m_CubeVAO, 36);
        
        // ---- 恢复默认渲染状态 ----
        RenderCommand::SetDepthFunc(DepthCompareFunc::Less);
        RenderCommand::SetDepthWrite(true);
        RenderCommand::SetCullMode(CullMode::Back);
        
        // 更新统计
        if (context.Stats)
        {
            context.Stats->DrawCalls++;
            context.Stats->TriangleCount += 12;  // Cube = 12 个三角形
        }
    }
}
