#include "lcpch.h"
#include "Renderer3D.h"

#include "Shader.h"
#include "UniformBuffer.h"
#include "RenderCommand.h"

#include <glm/ext/matrix_transform.hpp>

namespace Lucky
{
    /// <summary>
    /// 渲染器数据
    /// </summary>
    struct Renderer3DData
    {
        static const uint32_t MaxTextureSlots = 32; // 最大纹理槽数
        
        Ref<Shader> MeshShader;         // 着色器
        Ref<Texture2D> WhiteTexture;    // 白色纹理

        std::vector<Vertex> MeshVertexBufferData;   // 顶点缓冲区数据：最终要渲染的顶点数据

        std::array<Ref<Texture2D>, MaxTextureSlots> TextureSlots;   // 纹理槽列表 存储纹理
        uint32_t TextureSlotIndex = 1;                              // 纹理槽索引 0 = White

        Renderer3D::Statistics Stats;   // 统计数据

        /// <summary>
        /// 相机数据
        /// </summary>
        struct CameraData
        {
            glm::mat4 ViewProjectionMatrix; // VP 矩阵
        };

        CameraData CameraBuffer;
        Ref<UniformBuffer> CameraUniformBuffer; // 相机 Uniform 缓冲区
    };

    static Renderer3DData s_Data;   // 渲染器数据

    void Renderer3D::Init()
    {
        s_Data.WhiteTexture = Texture2D::Create(1, 1);                      // 创建宽高为 1 的纹理
        uint32_t whitTextureData = 0xffffffff;                              // 255 白色
        s_Data.WhiteTexture->SetData(&whitTextureData, sizeof(uint32_t));   // 设置纹理数据 size = 1 * 1 * 4 == sizeof(uint32_t)
        s_Data.TextureSlots[0] = s_Data.WhiteTexture;                       // 0 号纹理槽为白色纹理（默认）
        
        s_Data.MeshShader = CreateRef<Shader>("Assets/Shaders/TextureShader");  // 创建着色器

        s_Data.CameraUniformBuffer = UniformBuffer::Create(sizeof(Renderer3DData::CameraData), 0);  // 创建相机 Uniform 缓冲区
    }

    void Renderer3D::Shutdown()
    {
        
    }

    void Renderer3D::BeginScene(const EditorCamera& camera)
    {
        s_Data.CameraBuffer.ViewProjectionMatrix = camera.GetViewProjectionMatrix();                    // 设置 VP 矩阵
        s_Data.CameraUniformBuffer->SetData(&s_Data.CameraBuffer, sizeof(Renderer3DData::CameraData));  // 设置 Uniform 缓冲区数据
    }

    void Renderer3D::EndScene()
    {
        
    }

    void Renderer3D::DrawMesh(const glm::mat4& transform, Ref<Mesh>& mesh)
    {
        glm::mat3 normalMatrix = glm::mat3(glm::transpose(glm::inverse(transform)));    // 法向量做 M 变换 M 取逆矩阵的转置 防止 normal 在缩放时被拉伸

        s_Data.MeshVertexBufferData.clear();
        for (const Vertex& vertex : mesh->GetVertices())
        {
            Vertex v = vertex;
            v.Position = transform * glm::vec4(v.Position, 1.0f);
            v.Normal = normalMatrix * v.Normal;
            
            s_Data.MeshVertexBufferData.push_back(v);   // 添加顶点缓冲区数据
        }
        
        uint32_t dataSize = sizeof(Vertex) * static_cast<uint32_t>(s_Data.MeshVertexBufferData.size()); // 计算顶点缓冲区数据大小（字节）
        
        mesh->SetVertexBufferData(s_Data.MeshVertexBufferData.data(), dataSize);    // 设置顶点缓冲区数据
        
        // 绘制每个子网格
        for (uint32_t i = 0; i < mesh->GetSubMeshCount(); ++i)
        {
            SubMesh sm = mesh->GetSubMesh(i);

            s_Data.MeshShader->Bind();  // 绑定着色器 TODO 绑定每个子网格对应材质的着色器
            
            RenderCommand::DrawIndexed(mesh->GetVertexArray(), sm.IndexOffset, sm.IndexCount);
            
            s_Data.Stats.DrawCalls++;                           // 记录 DC 数量
            s_Data.Stats.TriangleCount += sm.IndexCount / 3;    // 记录三角形数量
        }
    }

    Renderer3D::Statistics Renderer3D::GetStats()
    {
        return s_Data.Stats;
    }

    void Renderer3D::ResetStats()
    {
        memset(&s_Data.Stats, 0, sizeof(Statistics));
    }
}