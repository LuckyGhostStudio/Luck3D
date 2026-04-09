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
        
        Ref<ShaderLibrary> ShaderLib;   // 着色器库 TODO Move to Renderer.h
        
        Ref<Shader> InternalErrorShader;        // 内部错误着色器
        Ref<Shader> StandardShader;             // 默认着色器
        Ref<Material> InternalErrorMaterial;    // 内部错误材质（材质丢失时使用：材质被意外删除等情况）
        Ref<Material> DefaultMaterial;          // 默认材质
        
        Ref<Texture2D> WhiteTexture;            // 白色纹理 0 号
        Ref<Texture2D> DefaultNormalTexture;    // 默认法线纹理 1 号
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
        s_Data.ShaderLib = CreateRef<ShaderLibrary>();  // 创建着色器库
        
        s_Data.ShaderLib->Load("Assets/Shaders/InternalError"); // 加载内部错误着色器
        s_Data.ShaderLib->Load("Assets/Shaders/Standard");      // 加载默认着色器
        
        s_Data.InternalErrorShader = s_Data.ShaderLib->Get("InternalError");
        s_Data.StandardShader = s_Data.ShaderLib->Get("Standard");
        
        s_Data.InternalErrorMaterial = CreateRef<Material>("InternalError Material", s_Data.InternalErrorShader);   // 创建内部错误材质
        s_Data.DefaultMaterial = CreateRef<Material>("Default Material", s_Data.StandardShader);                    // 创建默认材质

        // 创建白色纹理
        uint32_t whiteTextureData = 0xffffffff;                              // 255 白色
        s_Data.WhiteTexture = Texture2D::Create(1, 1);                      // 创建宽高为 1 的纹理
        s_Data.WhiteTexture->SetData(&whiteTextureData, sizeof(uint32_t));   // 设置纹理数据 size = 1 * 1 * 4 == sizeof(uint32_t)
        s_Data.TextureSlots[0] = s_Data.WhiteTexture;                       // 0 号纹理槽为白色纹理（默认）

        // 创建默认法线纹理（1×1，颜色 (128, 128, 255, 255)，解码后为 (0,0,1)）
        uint32_t normalData = 0xFFFF8080;  // RGBA: (128, 128, 255, 255)
        s_Data.DefaultNormalTexture = Texture2D::Create(1, 1);
        s_Data.DefaultNormalTexture->SetData(&normalData, sizeof(uint32_t));
        s_Data.TextureSlots[1] = s_Data.DefaultNormalTexture;
        
        // TODO 默认材质参数保存到 .mat 中
        
        // PBR 默认参数
        s_Data.DefaultMaterial->SetFloat4("u_Albedo", glm::vec4(0.8f, 0.8f, 0.8f, 1.0f));
        s_Data.DefaultMaterial->SetFloat("u_Metallic", 0.0f);
        s_Data.DefaultMaterial->SetFloat("u_Roughness", 0.5f);
        s_Data.DefaultMaterial->SetFloat("u_AO", 1.0f);
        s_Data.DefaultMaterial->SetFloat3("u_Emission", glm::vec3(0.0f));
        s_Data.DefaultMaterial->SetFloat("u_EmissionIntensity", 1.0f);
        
        s_Data.CameraUniformBuffer = UniformBuffer::Create(sizeof(Renderer3DData::CameraData), 0);  // 创建相机 Uniform 缓冲区
        s_Data.LightUniformBuffer = UniformBuffer::Create(sizeof(Renderer3DData::LightData), 1);    // 创建光照 Uniform 缓冲区
    }

    void Renderer3D::Shutdown()
    {
        
    }

    void Renderer3D::BeginScene(const EditorCamera& camera, const DirectionalLightData& lightData)
    {
        // 设置 Camera Uniform 缓冲区数据
        s_Data.CameraBuffer.ViewProjectionMatrix = camera.GetViewProjectionMatrix();
        s_Data.CameraBuffer.Position = camera.GetPosition();
        s_Data.CameraUniformBuffer->SetData(&s_Data.CameraBuffer, sizeof(Renderer3DData::CameraData));
        
        // 设置 Light Uniform 缓冲区数据
        s_Data.LightBuffer.Direction = lightData.Direction;
        s_Data.LightBuffer.Color = lightData.Color;
        s_Data.LightBuffer.Intensity = lightData.Intensity;
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
        for (const SubMesh& sm : mesh->GetSubMeshes())
        {
            // 获取该 SubMesh 对应的材质
            Ref<Material> material = nullptr;
            if (sm.MaterialIndex < materials.size())
            {
                material = materials[sm.MaterialIndex];
            }
            
            // 当前 SubMesh 材质不存在 使用内部错误材质（表示材质丢失）
            if (!material || !material->GetShader())
            {
                material = s_Data.InternalErrorMaterial;
            }
            
            // 绑定 Shader
            material->GetShader()->Bind();

            // 设置引擎内部 uniform
            material->GetShader()->SetMat4("u_ObjectToWorldMatrix", transform);

            // 绑定默认纹理到所有 PBR 纹理槽位 TODO Temp
            s_Data.WhiteTexture->Bind(0);          // u_AlbedoMap       白色，乘以 u_Albedo = u_Albedo
            s_Data.DefaultNormalTexture->Bind(1);  // u_NormalMap       法线蓝，解码后 = (0,0,1)
            s_Data.WhiteTexture->Bind(2);          // u_MetallicMap     白色，.r = 1.0
            s_Data.WhiteTexture->Bind(3);          // u_RoughnessMap    白色，.r = 1.0
            s_Data.WhiteTexture->Bind(4);          // u_AOMap           白色，.r = 1.0
            s_Data.WhiteTexture->Bind(5);          // u_EmissionMap     白色，乘以 u_Emission = u_Emission
            
            // 应用材质属性（上传所有材质参数到 GPU）用户可编辑的 uniform
            material->Apply();
            
            // 绘制索引
            RenderCommand::DrawIndexedRange(mesh->GetVertexArray(), sm.IndexOffset, sm.IndexCount);
            
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

    Ref<ShaderLibrary>& Renderer3D::GetShaderLibrary()
    {
        return s_Data.ShaderLib;
    }

    Ref<Material>& Renderer3D::GetInternalErrorMaterial()
    {
        return s_Data.InternalErrorMaterial;
    }

    Ref<Material>& Renderer3D::GetDefaultMaterial()
    {
        return s_Data.DefaultMaterial;
    }

    const Ref<Texture2D>& Renderer3D::GetWhiteTexture()
    {
        return s_Data.WhiteTexture;
    }
}
