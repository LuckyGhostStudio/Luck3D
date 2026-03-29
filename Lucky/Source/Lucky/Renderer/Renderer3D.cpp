#include "lcpch.h"
#include "Renderer3D.h"

#include <backends/imgui_impl_opengl3_loader.h>

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
        
        Ref<ShaderLibrary> ShaderLibrary;  // 着色器库
        
        Ref<Shader> DefaultShader;      // 默认着色器
        Ref<Material> DefaultMaterial;  // 默认材质
        
        Ref<Texture2D> WhiteTexture;    // 白色纹理 0 号
        // TODO 其他默认纹理
        
        std::array<Ref<Texture2D>, MaxTextureSlots> TextureSlots;   // 纹理槽列表 存储纹理
        uint32_t TextureSlotIndex = 1;                              // 纹理槽索引 0 = White

        std::vector<Vertex> MeshVertexBufferData;   // 顶点缓冲区数据：最终要渲染的顶点数据
        
        Renderer3D::Statistics Stats;   // 统计数据

        /// <summary>
        /// 相机数据
        /// </summary>
        struct CameraData
        {
            glm::mat4 ViewProjectionMatrix; // VP 矩阵
            glm::vec3 Position;             // 相机位置
            char padding[4];                // 填充到 16 字节对齐
        };
        
        /// <summary>
        /// 光照数据 目前仅支持一个方向光，后续可扩展为支持多光源
        /// </summary>
        struct LightData
        {
            float Intensity;        // 光照强度 TODO 该成员在第一位时才能正确渲染
            char padding3[12];      // 填充到 16 字节对齐
            glm::vec3 Direction;    // 光照方向（世界空间）
            char padding1[4];       // 填充到 16 字节对齐
            glm::vec3 Color;        // 光照颜色
            char padding2[4];       // 填充到 16 字节对齐
        };

        CameraData CameraBuffer;
        LightData LightBuffer;
        Ref<UniformBuffer> CameraUniformBuffer; // 相机 Uniform 缓冲区
        Ref<UniformBuffer> LightUniformBuffer;  // 光照 Uniform 缓冲区
    };

    static Renderer3DData s_Data;   // 渲染器数据

    void Renderer3D::Init()
    {
        s_Data.ShaderLibrary = CreateRef<ShaderLibrary>();  // 创建着色器库
        s_Data.DefaultShader = s_Data.ShaderLibrary->Load("Assets/Shaders/TextureShader");  // 加载默认着色器
        
        s_Data.WhiteTexture = Texture2D::Create(1, 1);                      // 创建宽高为 1 的纹理
        uint32_t whitTextureData = 0xffffffff;                              // 255 白色
        s_Data.WhiteTexture->SetData(&whitTextureData, sizeof(uint32_t));   // 设置纹理数据 size = 1 * 1 * 4 == sizeof(uint32_t)
        s_Data.TextureSlots[0] = s_Data.WhiteTexture;                       // 0 号纹理槽为白色纹理（默认）
        
        s_Data.TextureSlots[1] = Texture2D::Create("Assets/Textures/Texture_Gloss.png");  // 测试纹理

        s_Data.CameraUniformBuffer = UniformBuffer::Create(sizeof(Renderer3DData::CameraData), 0);  // 创建相机 Uniform 缓冲区
        s_Data.LightUniformBuffer = UniformBuffer::Create(sizeof(Renderer3DData::LightData), 1);    // 创建光照 Uniform 缓冲区
        
        s_Data.DefaultMaterial = CreateRef<Material>("Default Material", s_Data.DefaultShader);  // 创建默认材质
        
        s_Data.DefaultMaterial->SetFloat3("u_AmbientCoeff", glm::vec3(0.2f));
        s_Data.DefaultMaterial->SetFloat3("u_DiffuseCoeff", glm::vec3(0.8f));
        s_Data.DefaultMaterial->SetFloat3("u_SpecularCoeff", glm::vec3(0.5f));
        s_Data.DefaultMaterial->SetFloat("u_Shininess", 32.0f);
        s_Data.DefaultMaterial->SetInt("u_TextureIndex", 0);
    }

    void Renderer3D::Shutdown()
    {
        
    }

    void Renderer3D::BeginScene(const EditorCamera& camera)
    {
        // 设置 Camera Uniform 缓冲区数据
        s_Data.CameraBuffer.ViewProjectionMatrix = camera.GetViewProjectionMatrix();
        s_Data.CameraBuffer.Position = camera.GetPosition();
        s_Data.CameraUniformBuffer->SetData(&s_Data.CameraBuffer, sizeof(Renderer3DData::CameraData));
        
        // 设置 Light Uniform 缓冲区数据
        s_Data.LightBuffer.Direction = glm::vec3(-0.8f, -1.0f, -0.5f);
        s_Data.LightBuffer.Color = glm::vec3(1.0f, 1.0f, 1.0f);
        s_Data.LightBuffer.Intensity = 1.0f;
        s_Data.LightUniformBuffer->SetData(&s_Data.LightBuffer, sizeof(Renderer3DData::LightData));
    }

    void Renderer3D::EndScene()
    {
        
    }

    void Renderer3D::DrawMesh(const glm::mat4& transform, Ref<Mesh>& mesh, const std::vector<Ref<Material>>& materials)
    {
        // 准备顶点数据
        s_Data.MeshVertexBufferData.clear();
        for (const Vertex& vertex : mesh->GetVertices())
        {
            Vertex v = vertex;
            
            s_Data.MeshVertexBufferData.push_back(v);   // 添加顶点缓冲区数据 TODO 可以直接使用 mesh->GetVertices()
        }
        
        uint32_t dataSize = sizeof(Vertex) * static_cast<uint32_t>(s_Data.MeshVertexBufferData.size()); // 计算顶点缓冲区数据大小（字节）
        
        mesh->SetVertexBufferData(s_Data.MeshVertexBufferData.data(), dataSize);    // 设置顶点缓冲区数据
        
        // 绘制每个 SubMesh
        for (uint32_t i = 0; i < mesh->GetSubMeshCount(); ++i)
        {
            SubMesh sm = mesh->GetSubMesh(i);

            // 获取该 SubMesh 对应的材质
            Ref<Material> material = nullptr;
            if (sm.MaterialIndex < materials.size())
            {
                material = materials[sm.MaterialIndex];
            }
            
            // 当前 SubMesh 材质不存在 使用默认材质
            if (!material || !material->GetShader())
            {
                material = s_Data.DefaultMaterial;
            }
            
            // 绑定 Shader
            material->GetShader()->Bind();

            // 设置引擎内部 uniform
            material->GetShader()->SetMat4("u_ObjectToWorldMatrix", transform);
            
            // 绑定纹理（默认白色纹理到 0 号槽位）
            s_Data.WhiteTexture->Bind(0);
            
            // 应用材质属性（上传所有材质参数到 GPU）用户可编辑的 uniform
            material->Apply();
            
            // 绘制索引
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

    Ref<ShaderLibrary> Renderer3D::GetShaderLibrary()
    {
        return s_Data.ShaderLibrary;
    }
}
