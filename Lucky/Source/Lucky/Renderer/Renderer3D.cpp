#include "lcpch.h"
#include "Renderer3D.h"

#include "Shader.h"
#include "UniformBuffer.h"
#include "RenderCommand.h"
#include "ScreenQuad.h"
#include "Framebuffer.h"

#include <glad/glad.h>
#include <glm/ext/matrix_transform.hpp>
#include <unordered_set>

namespace Lucky
{
    /// <summary>
    /// 绘制命令：描述一次 DrawCall 所需的全部信息
    /// </summary>
    struct DrawCommand
    {
        glm::mat4 Transform;            // 模型变换矩阵
        Ref<Mesh> MeshData;             // 网格引用
        const SubMesh* SubMeshPtr;      // SubMesh 指针（指向 Mesh 内部数据，生命周期由 Mesh 保证）
        Ref<Material> MaterialData;     // 材质引用
        uint64_t SortKey = 0;           // 排序键
        float DistanceToCamera = 0.0f;  // 到相机的距离
        int EntityID = -1;              // Entity ID（用于鼠标拾取，-1 表示无效）
    };
    
    /// <summary>
    /// 描边绘制命令：仅包含 Silhouette 渲染所需的最小数据
    /// 从 DrawCommand 中提取，职责分离，避免 Outline Pass 依赖完整的 DrawCommand
    /// </summary>
    struct OutlineDrawCommand
    {
        glm::mat4 Transform;            // 模型变换矩阵
        Ref<Mesh> MeshData;             // 网格引用（通过 Ref 保证生命周期）
        const SubMesh* SubMeshPtr;      // SubMesh 指针（生命周期由 MeshData 的 Ref 保证）
    };
    
    /// <summary>
    /// 渲染器数据
    /// </summary>
    struct Renderer3DData
    {
        static constexpr uint32_t MaxTextureSlots = 32; // 最大纹理槽数
        
        Ref<ShaderLibrary> ShaderLib;   // 着色器库 TODO Move to Renderer.h
        
        Ref<Shader> InternalErrorShader;        // 内部错误着色器
        Ref<Shader> EntityIDShader;             // Entity ID 内部拾取着色器
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
        
        // ======== Outline Pass 资源（临时内联） ========
        Ref<Framebuffer> SilhouetteFBO;             // Silhouette FBO：选中物体轮廓掩码
        Ref<Shader> SilhouetteShader;               // Silhouette Shader：纯白色输出
        Ref<Shader> OutlineCompositeShader;         // 描边合成 Shader：边缘检测 + 叠加
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
        s_Data.EntityIDShader = s_Data.ShaderLib->Get("EntityID");
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
        
        // ======== Outline Pass 初始化 ========
        
        // 加载 Outline 相关 Shader
        s_Data.ShaderLib->Load("Assets/Shaders/Outline/Silhouette");
        s_Data.ShaderLib->Load("Assets/Shaders/Outline/OutlineComposite");
        s_Data.SilhouetteShader = s_Data.ShaderLib->Get("Silhouette");
        s_Data.OutlineCompositeShader = s_Data.ShaderLib->Get("OutlineComposite");
        
        // 创建 Silhouette FBO（初始大小 1280×720，后续在 Resize 时同步）
        FramebufferSpecification silhouetteSpec;
        silhouetteSpec.Width = 1280;
        silhouetteSpec.Height = 720;
        silhouetteSpec.Attachments = {
            FramebufferTextureFormat::RGBA8     // 轮廓掩码（白色 = 选中，黑色 = 未选中）
            // 不需要深度附件：描边穿透遮挡物（与 Unity 行为一致）
        };
        s_Data.SilhouetteFBO = Framebuffer::Create(silhouetteSpec);
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
    
        // ---- 批量绘制不透明物体 ----
        uint32_t lastShaderID = 0;  // 跟踪上一次绑定的 Shader
    
        for (const DrawCommand& cmd : s_Data.OpaqueDrawCommands)
        {
            // 绑定 Shader（仅在 Shader 变化时绑定）
            uint32_t currentShaderID = cmd.MaterialData->GetShader()->GetRendererID();
            if (currentShaderID != lastShaderID)
            {
                cmd.MaterialData->GetShader()->Bind();
                lastShaderID = currentShaderID;
            }
        
            // 设置引擎内部 uniform
            cmd.MaterialData->GetShader()->SetMat4("u_ObjectToWorldMatrix", cmd.Transform);
        
            // 应用材质属性 TODO 优化 相同材质跳过
            cmd.MaterialData->Apply();
        
            // 绘制
            RenderCommand::DrawIndexedRange(
                cmd.MeshData->GetVertexArray(),
                cmd.SubMeshPtr->IndexOffset,
                cmd.SubMeshPtr->IndexCount
            );
        
        s_Data.Stats.DrawCalls++;
            s_Data.Stats.TriangleCount += cmd.SubMeshPtr->IndexCount / 3;
        }

        // ======== Pass 2: Picking Pass（Entity ID 拾取） ========
        
        // 切换 glDrawBuffers：只写入 Attachment 1（Entity ID）
        GLenum pickBuffers[] = { GL_NONE, GL_COLOR_ATTACHMENT1 };
        glDrawBuffers(2, pickBuffers);
        
        // 关闭深度写入，保持深度测试（复用 Pass 1 的深度缓冲区）
        glDepthMask(GL_FALSE);
        glDepthFunc(GL_LEQUAL);
        
        // 绑定 Picking Shader
        s_Data.EntityIDShader->Bind();
        
        for (const DrawCommand& cmd : s_Data.OpaqueDrawCommands)
        {
            // 设置模型变换矩阵
            s_Data.EntityIDShader->SetMat4("u_ObjectToWorldMatrix", cmd.Transform);
            // 设置 Entity ID
            s_Data.EntityIDShader->SetInt("u_EntityID", cmd.EntityID);
            
            // 绘制
            RenderCommand::DrawIndexedRange(
                cmd.MeshData->GetVertexArray(),
                cmd.SubMeshPtr->IndexOffset,
                cmd.SubMeshPtr->IndexCount
            );
        }
        
        // 恢复 glDrawBuffers 和深度状态
        // 只启用 Attachment 0（颜色），禁用 Attachment 1（EntityID）
        // 防止后续 Gizmo 渲染时向 EntityID 缓冲区写入未定义数据
        GLenum normalBuffers[] = { GL_COLOR_ATTACHMENT0, GL_NONE };
        glDrawBuffers(2, normalBuffers);
        glDepthMask(GL_TRUE);
        glDepthFunc(GL_LESS);

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
        if (s_Data.SilhouetteFBO)
        {
            s_Data.SilhouetteFBO->Resize(width, height);
        }
    }

    void Renderer3D::RenderOutline()
    {
        // ======== Outline Pass（选中物体描边） ========
        if (s_Data.OutlineEnabled && !s_Data.OutlineEntityIDs.empty())
        {
            // ---- 阶段 1：渲染选中物体的 Silhouette ----
            
            // 绑定 Silhouette FBO
            s_Data.SilhouetteFBO->Bind();
            
            // 清除为黑色（未选中区域 = 透明黑色）
            RenderCommand::SetClearColor({ 0.0f, 0.0f, 0.0f, 0.0f });
            RenderCommand::Clear();
            
            // 绑定 Silhouette Shader
            s_Data.SilhouetteShader->Bind();
            
            // 禁用深度测试（描边穿透遮挡物，Silhouette FBO 无深度附件）
            glDisable(GL_DEPTH_TEST);
            
            // 遍历 OutlineDrawCommands（已在 EndScene() 中从 OpaqueDrawCommands 提取）
            for (const OutlineDrawCommand& cmd : s_Data.OutlineDrawCommands)
            {
                s_Data.SilhouetteShader->SetMat4("u_ObjectToWorldMatrix", cmd.Transform);
                
                RenderCommand::DrawIndexedRange(
                    cmd.MeshData->GetVertexArray(),
                    cmd.SubMeshPtr->IndexOffset,
                    cmd.SubMeshPtr->IndexCount
                );
            }
            
            s_Data.SilhouetteFBO->Unbind();
            
            // ---- 阶段 2：边缘检测 + 描边合成 ----
            
            // 重新绑定主 FBO
            s_Data.TargetFramebuffer->Bind();
            
            // 只写入 Attachment 0（颜色），不写入 Attachment 1（EntityID）
            GLenum outlineBuffers[] = { GL_COLOR_ATTACHMENT0, GL_NONE };
            glDrawBuffers(2, outlineBuffers);
            
            // 禁用深度测试（全屏 Quad 不需要深度测试）
            glDisable(GL_DEPTH_TEST);
            
            // 启用 Alpha 混合（描边颜色与场景颜色混合）
            glEnable(GL_BLEND);
            glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
            
            // 绑定 OutlineComposite Shader
            s_Data.OutlineCompositeShader->Bind();
            
            // 绑定 Silhouette 纹理到纹理单元 0
            glActiveTexture(GL_TEXTURE0);
            glBindTexture(GL_TEXTURE_2D, s_Data.SilhouetteFBO->GetColorAttachmentRendererID(0));
            s_Data.OutlineCompositeShader->SetInt("u_SilhouetteTexture", 0);
            
            // 设置描边参数
            s_Data.OutlineCompositeShader->SetFloat4("u_OutlineColor", s_Data.OutlineColor);
            s_Data.OutlineCompositeShader->SetFloat("u_OutlineWidth", s_Data.OutlineWidth);
            
            // 绘制全屏四边形
            ScreenQuad::Draw();
            
        // 恢复渲染状态
            glEnable(GL_DEPTH_TEST);
            glDepthFunc(GL_LESS);
            glDisable(GL_BLEND);
        }
        
        // 清空描边命令列表
        s_Data.OutlineDrawCommands.clear();
    }
}
