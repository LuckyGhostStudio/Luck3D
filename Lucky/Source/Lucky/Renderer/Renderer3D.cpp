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

        std::vector<Vertex> MeshVertexBufferData;   // 顶点缓冲区数据：最终要渲染的顶点数据
        
        // 全局默认纹理表 只在初始化时修改一次
        std::unordered_map<TextureDefault, Ref<Texture2D>> DefaultTextures;
        
        Renderer3D::Statistics Stats;   // 统计数据

        /// <summary>
        /// 相机 UBO 数据
        /// </summary>
        struct CameraUBOData
        {
            glm::mat4 ViewProjectionMatrix; // VP 矩阵
            glm::vec3 Position;             // 相机位置
            char padding[4];                // 填充到 16 字节对齐
        };

        /// <summary>
        /// 光照 UBO 数据
        /// </summary>
        struct LightUBOData
        {
            int DirectionalLightCount;  // 方向光数量
            int PointLightCount;        // 点光源数量
            int SpotLightCount;         // 聚光灯数量
            char padding[4];            // 填充到 16 字节对齐
            
            DirectionalLightData DirectionalLights[s_MaxDirectionalLights]; // 方向光数组
            PointLightData PointLights[s_MaxPointLights];                   // 点光源数组
            SpotLightData SpotLights[s_MaxSpotLights];                      // 聚光灯数组
        };
        
        CameraUBOData CameraBuffer; // 相机 UBO 数据
        LightUBOData LightBuffer;   // 光照 UBO 数据

        Ref<UniformBuffer> CameraUniformBuffer; // 相机 Uniform 缓冲区
        Ref<UniformBuffer> LightUniformBuffer;  // 光照 Uniform 缓冲区
    };

    static Renderer3DData s_Data;   // 渲染器数据

    void Renderer3D::Init()
    {
        s_Data.ShaderLib = CreateRef<ShaderLibrary>();  // 创建着色器库
        
        // 加载内部着色器
        s_Data.ShaderLib->Load("Assets/Shaders/InternalError"); // 内部错误着色器
        s_Data.ShaderLib->Load("Assets/Shaders/Standard");      // 默认着色器
        
        s_Data.InternalErrorShader = s_Data.ShaderLib->Get("InternalError");
        s_Data.StandardShader = s_Data.ShaderLib->Get("Standard");
        
        // 创建内部材质
        s_Data.InternalErrorMaterial = CreateRef<Material>("InternalError Material", s_Data.InternalErrorShader);   // 内部错误材质
        s_Data.DefaultMaterial = CreateRef<Material>("Default Material", s_Data.StandardShader);                    // 默认材质

        // 创建全局默认纹理
        // White: (255, 255, 255, 255)
        uint32_t whiteData = 0xFFFFFFFF;
        s_Data.DefaultTextures[TextureDefault::White] = Texture2D::Create(1, 1);
        s_Data.DefaultTextures[TextureDefault::White]->SetData(&whiteData, sizeof(uint32_t));

        // Black: (0, 0, 0, 255)
        uint32_t blackData = 0xFF000000;
        s_Data.DefaultTextures[TextureDefault::Black] = Texture2D::Create(1, 1);
        s_Data.DefaultTextures[TextureDefault::Black]->SetData(&blackData, sizeof(uint32_t));

        // Normal: (128, 128, 255, 255)
        uint32_t normalData = 0xFFFF8080;
        s_Data.DefaultTextures[TextureDefault::Normal] = Texture2D::Create(1, 1);
        s_Data.DefaultTextures[TextureDefault::Normal]->SetData(&normalData, sizeof(uint32_t));
        
        // TODO 默认材质参数保存到 .mat 中
        
        // PBR 默认参数
        s_Data.DefaultMaterial->SetFloat4("u_Albedo", glm::vec4(0.8f, 0.8f, 0.8f, 1.0f));
        s_Data.DefaultMaterial->SetFloat("u_Metallic", 0.0f);
        s_Data.DefaultMaterial->SetFloat("u_Roughness", 0.5f);
        s_Data.DefaultMaterial->SetFloat("u_AO", 1.0f);
        s_Data.DefaultMaterial->SetFloat3("u_Emission", glm::vec3(0.0f));
        s_Data.DefaultMaterial->SetFloat("u_EmissionIntensity", 1.0f);
        
        s_Data.CameraUniformBuffer = UniformBuffer::Create(sizeof(Renderer3DData::CameraUBOData), 0);  // 创建相机 Uniform 缓冲区
        s_Data.LightUniformBuffer = UniformBuffer::Create(sizeof(Renderer3DData::LightUBOData), 1);    // 创建光照 Uniform 缓冲区
    }

    void Renderer3D::Shutdown()
    {
        
    }

    void Renderer3D::BeginScene(const EditorCamera& camera, const SceneLightData& lightData)
    {
        // 设置 Camera Uniform 缓冲区数据
        s_Data.CameraBuffer.ViewProjectionMatrix = camera.GetViewProjectionMatrix();
        s_Data.CameraBuffer.Position = camera.GetPosition();
        s_Data.CameraUniformBuffer->SetData(&s_Data.CameraBuffer, sizeof(Renderer3DData::CameraUBOData));
        
        // 设置 Light Uniform 缓冲区数据
        s_Data.LightBuffer.DirectionalLightCount = lightData.DirectionalLightCount;
        s_Data.LightBuffer.PointLightCount = lightData.PointLightCount;
        s_Data.LightBuffer.SpotLightCount = lightData.SpotLightCount;
    
        for (int i = 0; i < lightData.DirectionalLightCount; ++i)
        {
            s_Data.LightBuffer.DirectionalLights[i] = lightData.DirectionalLights[i];
        }
        for (int i = 0; i < lightData.PointLightCount; ++i)
        {
            s_Data.LightBuffer.PointLights[i] = lightData.PointLights[i];
        }
        for (int i = 0; i < lightData.SpotLightCount; ++i)
        {
            s_Data.LightBuffer.SpotLights[i] = lightData.SpotLights[i];
        }
        s_Data.LightUniformBuffer->SetData(&s_Data.LightBuffer, sizeof(Renderer3DData::LightUBOData));
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

    const Ref<Texture2D>& Renderer3D::GetDefaultTexture(TextureDefault type)
    {
        auto it = s_Data.DefaultTextures.find(type);
        if (it != s_Data.DefaultTextures.end())
        {
            return it->second;
        }
        
        return s_Data.DefaultTextures[TextureDefault::White];   // 默认白色
    }
}
