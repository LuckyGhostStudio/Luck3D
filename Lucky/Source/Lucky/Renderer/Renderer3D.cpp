#include "lcpch.h"
#include "Renderer3D.h"
#include "RenderContext.h"
#include "RenderPipeline.h"

#include "Shader.h"
#include "UniformBuffer.h"
#include "RenderCommand.h"
#include "Framebuffer.h"

#include "Passes/OpaquePass.h"
#include "Passes/PickingPass.h"
#include "Passes/SilhouettePass.h"
#include "Passes/OutlineCompositePass.h"

#include <glm/ext/matrix_transform.hpp>
#include <unordered_set>

namespace Lucky
{
    /// <summary>
    /// 渲染器数据
    /// </summary>
    struct Renderer3DData
    {
        static constexpr uint32_t MaxTextureSlots = 32; // 最大纹理槽数
        
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
        
        std::vector<DrawCommand> OpaqueDrawCommands;    // 不透明物体绘制命令列表
        std::vector<OutlineDrawCommand> OutlineDrawCommands; // 描边专用绘制命令列表（从 OpaqueDrawCommands 中提取）
        glm::vec3 CameraPosition;                       // 缓存相机位置（用于计算距离）
        
        // ======== 渲染管线 ========
        RenderPipeline Pipeline;                        // 渲染管线（管理所有 RenderPass）
        
        // ======== Outline 数据（由外部设置，通过 RenderContext 传递给 Pass） ========
        Ref<Framebuffer> TargetFramebuffer;         // 主 FBO 引用（描边合成后重新绑定）
        std::unordered_set<int> OutlineEntityIDs;   // 需要描边的所有 EntityID 集合（空集合表示无选中）
        
        // 描边参数
        glm::vec4 OutlineColor = glm::vec4(1.0f, 0.4f, 0.0f, 1.0f);  // 描边颜色（默认橙色 #FF6600）
        float OutlineWidth = 2.0f;                                      // 描边宽度（像素）
        bool OutlineEnabled = true;                                     // 是否启用描边
    };

    static Renderer3DData s_Data;   // 渲染器数据

    void Renderer3D::Init()
    {
        s_Data.ShaderLib = CreateRef<ShaderLibrary>();  // 创建着色器库
        
        // 加载内部着色器
        s_Data.ShaderLib->Load("Assets/Shaders/InternalError"); // 内部错误着色器
        s_Data.ShaderLib->Load("Assets/Shaders/EntityID");      // Entity ID 拾取着色器
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
        
        // ======== 加载 Outline 相关 Shader ========
        s_Data.ShaderLib->Load("Assets/Shaders/Outline/Silhouette");
        s_Data.ShaderLib->Load("Assets/Shaders/Outline/OutlineComposite");
        
        // ======== 创建渲染管线 ========
        auto opaquePass = CreateRef<OpaquePass>();
        auto pickingPass = CreateRef<PickingPass>();
        auto silhouettePass = CreateRef<SilhouettePass>();
        auto outlineCompositePass = CreateRef<OutlineCompositePass>();
        
        // 设置 Pass 之间的依赖
        outlineCompositePass->SetSilhouettePass(silhouettePass);
        
        // 按顺序添加 Pass
        // 注意：OpaquePass 和 PickingPass 在 EndScene() 中执行
        //       SilhouettePass 和 OutlineCompositePass 在 RenderOutline() 中执行
        s_Data.Pipeline.AddPass(opaquePass);
        s_Data.Pipeline.AddPass(pickingPass);
        s_Data.Pipeline.AddPass(silhouettePass);
        s_Data.Pipeline.AddPass(outlineCompositePass);
        
        s_Data.Pipeline.Init();
    }

    void Renderer3D::Shutdown()
    {
        s_Data.Pipeline.Shutdown();
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
        
        // 清空绘制命令列表
        s_Data.OpaqueDrawCommands.clear();
    
        // 缓存相机位置
        s_Data.CameraPosition = camera.GetPosition();
    }

    void Renderer3D::EndScene()
    {
        // ---- 排序不透明物体 ----
        std::sort(s_Data.OpaqueDrawCommands.begin(), s_Data.OpaqueDrawCommands.end(), [](const DrawCommand& a, const DrawCommand& b)
        {
            return a.SortKey < b.SortKey;   // 按 SortKey 升序（聚合相同 Shader）
        });
        
        // ---- 构建 RenderContext ----
        RenderContext context;
        context.OpaqueDrawCommands = &s_Data.OpaqueDrawCommands;
        context.TargetFramebuffer = s_Data.TargetFramebuffer;
        context.Stats = &s_Data.Stats;
        
        // ---- 执行 OpaquePass + PickingPass ----
        s_Data.Pipeline.GetPass<OpaquePass>()->Execute(context);
        s_Data.Pipeline.GetPass<PickingPass>()->Execute(context);

        // ======== 提取描边物体到独立列表 ========
        // 将描边所需的最小几何数据从 OpaqueDrawCommands 中提取到 OutlineDrawCommands
        // 使 OpaqueDrawCommands 的生命周期在 EndScene() 结束时终止，职责清晰
        s_Data.OutlineDrawCommands.clear();
        if (!s_Data.OutlineEntityIDs.empty())
        {
            for (const DrawCommand& cmd : s_Data.OpaqueDrawCommands)
            {
                if (s_Data.OutlineEntityIDs.count(cmd.EntityID))
                {
                    OutlineDrawCommand outlineCmd;
                    outlineCmd.Transform = cmd.Transform;
                    outlineCmd.MeshData = cmd.MeshData;    
                    outlineCmd.SubMeshPtr = cmd.SubMeshPtr;

                    s_Data.OutlineDrawCommands.push_back(outlineCmd);
                }
            }
        }
        // 立即清空 OpaqueDrawCommands，生命周期在 EndScene() 结束
        s_Data.OpaqueDrawCommands.clear();
    }

    void Renderer3D::DrawMesh(const glm::mat4& transform, Ref<Mesh>& mesh, const std::vector<Ref<Material>>& materials, int entityID)
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
        
        // 计算物体到相机的距离
        glm::vec3 objPos = glm::vec3(transform[3]);
        float distToCamera = glm::length(s_Data.CameraPosition - objPos);
        
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
            
            // 计算排序键
            uint64_t shaderID = material->GetShader()->GetRendererID();
            uint64_t sortKey = (shaderID & 0xFFFF) << 48;  // 高 16 位：Shader ID
        
            // 构建 DrawCommand
            DrawCommand cmd;
            cmd.Transform = transform;
            cmd.MeshData = mesh;
            cmd.SubMeshPtr = &sm;
            cmd.MaterialData = material;
            cmd.SortKey = sortKey;
            cmd.DistanceToCamera = distToCamera;
            cmd.EntityID = entityID;
        
            // 加入不透明绘制命令列表
            s_Data.OpaqueDrawCommands.push_back(cmd);
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

    void Renderer3D::SetTargetFramebuffer(const Ref<Framebuffer>& framebuffer)
    {
        s_Data.TargetFramebuffer = framebuffer;
    }

    void Renderer3D::SetOutlineEntities(const std::unordered_set<int>& entityIDs)
    {
        s_Data.OutlineEntityIDs = entityIDs;
    }

    void Renderer3D::SetOutlineColor(const glm::vec4& color)
    {
        s_Data.OutlineColor = color;
    }

    void Renderer3D::ResizeSilhouetteFBO(uint32_t width, uint32_t height)
    {
        // 通过 Pipeline 获取 SilhouettePass 并调用 Resize
        auto silhouettePass = s_Data.Pipeline.GetPass<SilhouettePass>();
        if (silhouettePass)
        {
            silhouettePass->Resize(width, height);
        }
    }

    RenderPipeline& Renderer3D::GetPipeline()
    {
        return s_Data.Pipeline;
    }

    void Renderer3D::RenderOutline()
    {
        // ---- 构建 Outline RenderContext ----
        RenderContext context;
        context.OutlineDrawCommands = &s_Data.OutlineDrawCommands;
        context.OutlineEntityIDs = &s_Data.OutlineEntityIDs;
        context.OutlineColor = s_Data.OutlineColor;
        context.OutlineWidth = s_Data.OutlineWidth;
        context.OutlineEnabled = s_Data.OutlineEnabled;
        context.TargetFramebuffer = s_Data.TargetFramebuffer;
        
        // ---- 执行 SilhouettePass + OutlineCompositePass ----
        s_Data.Pipeline.GetPass<SilhouettePass>()->Execute(context);
        s_Data.Pipeline.GetPass<OutlineCompositePass>()->Execute(context);
        
        // 清空描边命令列表
        s_Data.OutlineDrawCommands.clear();
    }
}
