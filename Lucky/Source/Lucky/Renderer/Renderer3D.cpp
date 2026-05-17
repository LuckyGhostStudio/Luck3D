#include "lcpch.h"
#include "Renderer3D.h"

#include "RenderContext.h"
#include "RenderPipeline.h"

#include "Shader.h"
#include "UniformBuffer.h"
#include "Framebuffer.h"

#include "Passes/ShadowPass.h"
#include "Passes/OpaquePass.h"
#include "ShadowAtlas.h"
#include "Passes/SkyboxPass.h"
#include "Passes/TransparentPass.h"
#include "Passes/PickingPass.h"
#include "Passes/PostProcessPass.h"
#include "Passes/SilhouettePass.h"
#include "Passes/OutlineCompositePass.h"

#include "Effects/BloomEffect.h"
#include "Effects/FXAAEffect.h"
#include "Effects/VignetteEffect.h"

#include "IBLPrecompute.h"

#include "Lucky/Asset/AssetManager.h"

#include <glm/ext/matrix_transform.hpp>
#include <glm/ext/matrix_clip_space.hpp>

#include <limits>
#include <filesystem>

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
        Ref<Shader> SkyboxShader;               // 默认天空盒着色器
        Ref<Material> InternalErrorMaterial;    // 内部错误材质（材质丢失时使用：材质被意外删除等情况）
        Ref<Material> DefaultMaterial;          // 默认材质

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
        
        std::vector<DrawCommand> OpaqueDrawCommands;          // 不透明物体绘制命令列表
        std::vector<DrawCommand> TransparentDrawCommands;     // 透明物体绘制命令列表
        std::vector<OutlineDrawCommand> OutlineDrawCommands;  // 描边专用绘制命令列表（从 OpaqueDrawCommands 中提取）
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
        
        // ======== 阴影数据 ========
        bool ShadowEnabled = false;                     // 是否启用阴影
        float ShadowBias = 0.005f;                      // 阴影偏移（从组件读取）
        float ShadowStrength = 1.0f;                    // 阴影强度（从组件读取）
        ShadowType ShadowShadowType = ShadowType::None; // 阴影类型（从组件读取）
        
        // ---- CSM 数据 ----
        int CascadeCount = 4;                                                    // 级联数量
        glm::mat4 CascadeLightSpaceMatrices[s_MaxCascadeCount];                  // 每级 Light Space Matrix
        float CascadeFarPlanes[s_MaxCascadeCount] = { 0.0f };                    // 每级远平面距离（视图空间）
        int ShadowMapResolution = 2048;                                          // 每级 Shadow Map 分辨率
        
        // ---- 聚光灯阴影数据 ----
        struct SpotShadowCacheData
        {
            int LightIndex = -1;                            // 在 SpotLights[] 中的索引
            glm::mat4 LightSpaceMatrix = glm::mat4(1.0f);   // 光源空间 VP 矩阵
            float ShadowBias = 0.001f;                      // 阴影偏移
            float ShadowStrength = 1.0f;                    // 阴影强度
            int ShadowType = 1;                             // 1=Hard, 2=Soft
        };
        SpotShadowCacheData SpotShadowData[ShadowAtlas::s_MaxSpotLightShadows];
        int SpotShadowCount = 0;
        
        // ---- 点光源阴影数据 ----
        struct PointShadowCacheData
        {
            int LightIndex = -1;                            // 在 PointLights[] 中的索引
            glm::vec3 LightPos = glm::vec3(0.0f);           // 点光源世界空间位置
            float FarPlane = 25.0f;                         // 远平面距离（= Range）
            glm::mat4 LightSpaceMatrices[6];                // 6 面 Light Space Matrix
            float ShadowBias = 0.05f;                       // 阴影偏移
            float ShadowStrength = 1.0f;                    // 阴影强度
            int ShadowType = 1;                             // 1=Hard, 2=Soft
        };
        PointShadowCacheData PointShadowData[ShadowAtlas::s_MaxPointLightShadows];
        int PointShadowCount = 0;
        
        // ======== 清屏颜色 ========
        glm::vec4 ClearColor = glm::vec4(0.0f, 0.0f, 0.0f, 1.0f);  // 视口清屏颜色（由外部设置）
        
        // ======== 相机矩阵缓存（SkyboxPass 需要） ========
        glm::mat4 CameraViewMatrix = glm::mat4(1.0f);           // 相机 View 矩阵
        glm::mat4 CameraProjectionMatrix = glm::mat4(1.0f);     // 相机 Projection 矩阵
        
        // ======== 后处理参数 ========
        PostProcessSettings PostProcess;    // 后处理参数（由 Scene 收集 Volume 后传入）
        
        // ======== 环境设置 ========
        EnvironmentSettings Environment;    // 环境设置参数（由 Scene 传入）
    };

    static Renderer3DData s_Data;   // 渲染器数据

    void Renderer3D::Init()
    {
        s_Data.ShaderLib = CreateRef<ShaderLibrary>();  // 创建着色器库
        
        // 加载引擎内部着色器（路径包含 /Internal/，自动标记为 Internal，不在 Inspector 中显示）
        s_Data.ShaderLib->Load("Assets/Shaders/Internal/InternalError");                // 内部错误着色器
        s_Data.ShaderLib->Load("Assets/Shaders/Internal/EntityID");                     // Entity ID 拾取着色器
        s_Data.ShaderLib->Load("Assets/Shaders/Internal/Outline/Silhouette");           // 描边轮廓着色器
        s_Data.ShaderLib->Load("Assets/Shaders/Internal/Outline/OutlineComposite");     // 描边合成着色器
        s_Data.ShaderLib->Load("Assets/Shaders/Internal/Shadow/Shadow");                // 阴影着色器
        s_Data.ShaderLib->Load("Assets/Shaders/Internal/Shadow/PointShadow");           // 点光源阴影着色器（线性深度）
        s_Data.ShaderLib->Load("Assets/Shaders/Internal/PostProcess/Tonemapping");      // Tonemapping 着色器
        s_Data.ShaderLib->Load("Assets/Shaders/Internal/PostProcess/BrightExtract");    // 亮度提取着色器（Bloom）
        s_Data.ShaderLib->Load("Assets/Shaders/Internal/PostProcess/GaussianBlur");     // 高斯模糊着色器（Bloom）
        s_Data.ShaderLib->Load("Assets/Shaders/Internal/PostProcess/BloomComposite");   // Bloom 合成着色器
        s_Data.ShaderLib->Load("Assets/Shaders/Internal/PostProcess/FXAA");             // FXAA 着色器
        s_Data.ShaderLib->Load("Assets/Shaders/Internal/PostProcess/Vignette");         // Vignette 着色器
        s_Data.ShaderLib->Load("Assets/Shaders/Internal/IBL/BRDFIntegration");          // BRDF LUT 生成着色器
        s_Data.ShaderLib->Load("Assets/Shaders/Internal/IBL/IrradianceConvolution");    // Irradiance 卷积着色器
        s_Data.ShaderLib->Load("Assets/Shaders/Internal/IBL/PrefilterConvolution");     // Prefilter 卷积着色器

        // 加载用户可见着色器
        s_Data.ShaderLib->Load("Assets/Shaders/Standard");  // 默认着色器
        s_Data.ShaderLib->Load("Assets/Shaders/Skybox");    // 默认天空盒着色器

        s_Data.InternalErrorShader = s_Data.ShaderLib->Get("InternalError");
        s_Data.StandardShader = s_Data.ShaderLib->Get("Standard");
        s_Data.SkyboxShader = s_Data.ShaderLib->Get("Skybox");
        
        // 创建内部材质
        s_Data.InternalErrorMaterial = CreateRef<Material>("InternalError", s_Data.InternalErrorShader);    // 内部错误材质
        s_Data.DefaultMaterial = CreateRef<Material>("Default-Material", s_Data.StandardShader);            // 默认材质
        s_Data.Environment.SkyboxMaterial = CreateRef<Material>("Default-Skybox", s_Data.ShaderLib->Get("Skybox"));     // 默认天空盒材质
        
        // PBR 默认参数
        s_Data.DefaultMaterial->SetFloat4("u_Albedo", glm::vec4(0.8f, 0.8f, 0.8f, 1.0f));
        s_Data.DefaultMaterial->SetFloat("u_Metallic", 0.0f);
        s_Data.DefaultMaterial->SetFloat("u_Roughness", 0.5f);
        s_Data.DefaultMaterial->SetFloat("u_AO", 1.0f);
        s_Data.DefaultMaterial->SetFloat3("u_Emission", glm::vec3(0.0f));
        s_Data.DefaultMaterial->SetFloat("u_EmissionIntensity", 1.0f);
        
        // ======== 天空盒测试（硬编码加载 6 面 Cubemap） ========
        // 天空盒纹理路径（放置在 Assets/Textures/Skybox/ 目录下）
        const std::string skyboxDir = "Assets/Textures/Skybox/";
        std::array skyboxFaces = {
            skyboxDir + "right.jpg",    // +X
            skyboxDir + "left.jpg",     // -X
            skyboxDir + "up.jpg",       // +Y
            skyboxDir + "down.jpg",     // -Y
            skyboxDir + "front.jpg",    // +Z
            skyboxDir + "back.jpg"      // -Z
        };
        
        // 检查天空盒纹理文件是否存在
        Ref<TextureCube> skyboxCubemap = nullptr;
        if (std::filesystem::exists(skyboxFaces[0]))
        {
            // 加载 Cubemap 纹理l
            skyboxCubemap = TextureCube::Create(skyboxFaces);
            //skyboxCubemap = TextureCube::CreateFromHDR("Assets/Textures/Skybox/forest.hdr");

            // 设置 Cubemap 纹理到材质
            s_Data.Environment.SkyboxMaterial->SetTextureCube("u_SkyboxMap", skyboxCubemap);
            s_Data.Environment.SkyboxMaterial->SetFloat("u_Exposure", 1.0f);
            s_Data.Environment.SkyboxMaterial->SetFloat4("u_Tint", glm::vec4(1.0f));
            
            LF_INFO("Skybox loaded successfully from: {0}", skyboxDir);
        }
        else
        {
            LF_WARN("Skybox textures not found at: {0} (skipping skybox)", skyboxDir);
        }
        
        // 创建内部材质资产
        AssetManager::CreateAsset(s_Data.DefaultMaterial, "Assets/Internal/Materials/Default-Material.lmat");
        AssetManager::CreateAsset(s_Data.Environment.SkyboxMaterial, "Assets/Internal/Materials/Default-Skybox.lmat");

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
        
        s_Data.CameraUniformBuffer = UniformBuffer::Create(sizeof(Renderer3DData::CameraUBOData), 0);  // 创建相机 Uniform 缓冲区
        s_Data.LightUniformBuffer = UniformBuffer::Create(sizeof(Renderer3DData::LightUBOData), 1);    // 创建光照 Uniform 缓冲区
        
        // ======== 创建渲染管线 ========
        auto shadowPass = CreateRef<ShadowPass>();
        auto opaquePass = CreateRef<OpaquePass>();
        auto skyboxPass = CreateRef<SkyboxPass>();
        auto transparentPass = CreateRef<TransparentPass>();
        auto pickingPass = CreateRef<PickingPass>();
        auto postProcessPass = CreateRef<PostProcessPass>();
        auto silhouettePass = CreateRef<SilhouettePass>();
        auto outlineCompositePass = CreateRef<OutlineCompositePass>();
        
        // 设置 Pass 之间的依赖
        outlineCompositePass->SetSilhouettePass(silhouettePass);
        
        // 按顺序添加 Pass（执行顺序：Shadow -> Main -> PostProcess -> Outline）
        // 注意：ShadowPass 和 OpaquePass/PickingPass 在 EndScene() 中执行
        //       PostProcessPass 在 EndScene() 中执行（Main 之后）
        //       SilhouettePass 和 OutlineCompositePass 在 RenderOutline() 中执行
        s_Data.Pipeline.AddPass(shadowPass);
        s_Data.Pipeline.AddPass(opaquePass);
        s_Data.Pipeline.AddPass(skyboxPass);
        s_Data.Pipeline.AddPass(transparentPass);
        s_Data.Pipeline.AddPass(pickingPass);
        s_Data.Pipeline.AddPass(postProcessPass);
        s_Data.Pipeline.AddPass(silhouettePass);
        s_Data.Pipeline.AddPass(outlineCompositePass);
        
        s_Data.Pipeline.Init();
        
        // ======== 初始化 IBL 预计算系统 ========
        IBLPrecompute::Init();
        
        // 天空盒加载成功后，生成 IBL 数据（使用默认分辨率）
        if (skyboxCubemap)
        {
            IBLPrecompute::GenerateFromCubemap(skyboxCubemap->GetRendererID(), s_Data.Environment.ReflectionResolution);
        }
        
        // ======== 注册后处理效果到 PostProcessStack ========
        auto bloomEffect = CreateRef<BloomEffect>();
        bloomEffect->Order = 0;
        bloomEffect->Enabled = false;   // 默认禁用，由 Volume 控制
        
        auto vignetteEffect = CreateRef<VignetteEffect>();
        vignetteEffect->Order = 10;
        vignetteEffect->Enabled = false;
        
        auto fxaaEffect = CreateRef<FXAAEffect>();
        fxaaEffect->Order = 0;
        fxaaEffect->Enabled = false;
        
        postProcessPass->GetPostProcessStack().AddEffect(bloomEffect);
        postProcessPass->GetPostProcessStack().AddEffect(vignetteEffect);
        postProcessPass->GetPostProcessStack().AddEffect(fxaaEffect);
    }

    void Renderer3D::Shutdown()
    {
        IBLPrecompute::Shutdown();
        s_Data.Pipeline.Shutdown();
    }

    /// <summary>
    /// 计算点光源 6 面的 Light Space Matrix
    /// 使用 90° FOV 透视投影，覆盖 Cubemap 的每个面
    /// </summary>
    static std::array<glm::mat4, 6> CalcPointLightMatrices(const glm::vec3& lightPos, float farPlane)
    {
        float nearPlane = 0.1f;
        glm::mat4 projection = glm::perspective(glm::radians(90.0f), 1.0f, nearPlane, farPlane);

        std::array<glm::mat4, 6> matrices;

        // +X：看向右方
        matrices[0] = projection * glm::lookAt(lightPos, lightPos + glm::vec3( 1.0f, 0.0f, 0.0f), glm::vec3(0.0f, -1.0f, 0.0f));
        // -X：看向左方
        matrices[1] = projection * glm::lookAt(lightPos, lightPos + glm::vec3(-1.0f, 0.0f, 0.0f), glm::vec3(0.0f, -1.0f, 0.0f));
        // +Y：看向上方
        matrices[2] = projection * glm::lookAt(lightPos, lightPos + glm::vec3( 0.0f, 1.0f, 0.0f), glm::vec3(0.0f, 0.0f, 1.0f));
        // -Y：看向下方
        matrices[3] = projection * glm::lookAt(lightPos, lightPos + glm::vec3( 0.0f,-1.0f, 0.0f), glm::vec3(0.0f, 0.0f, -1.0f));
        // +Z：看向前方
        matrices[4] = projection * glm::lookAt(lightPos, lightPos + glm::vec3( 0.0f, 0.0f, 1.0f), glm::vec3(0.0f, -1.0f, 0.0f));
        // -Z：看向后方
        matrices[5] = projection * glm::lookAt(lightPos, lightPos + glm::vec3( 0.0f, 0.0f,-1.0f), glm::vec3(0.0f, -1.0f, 0.0f));

        return matrices;
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
        
        // ======== CSM 计算（替换原有的固定 orthoSize 计算） ========
        s_Data.ShadowEnabled = false;
        if (lightData.DirectionalLightCount > 0 && lightData.DirLightShadowType != ShadowType::None)
        {
            s_Data.ShadowEnabled = true;
            s_Data.ShadowBias = lightData.DirLightShadowBias;
            s_Data.ShadowStrength = lightData.DirLightShadowStrength;
            s_Data.ShadowShadowType = lightData.DirLightShadowType;
            s_Data.CascadeCount = lightData.CascadeCount;
            s_Data.ShadowMapResolution = lightData.ShadowMapResolution;

            glm::vec3 lightDir = glm::normalize(lightData.DirectionalLights[0].Direction);

            // 计算每级的远平面距离
            float cameraNear = camera.GetNear();
            
            float cascadeNearPlanes[s_MaxCascadeCount];
            float cascadeFarPlanes[s_MaxCascadeCount];
            
            for (int i = 0; i < s_Data.CascadeCount; ++i)
            {
                cascadeNearPlanes[i] = (i == 0) ? cameraNear : cascadeFarPlanes[i - 1];
                cascadeFarPlanes[i] = cameraNear + lightData.ShadowDistance * lightData.CascadeSplits[i];
                s_Data.CascadeFarPlanes[i] = cascadeFarPlanes[i];
            }

            // 计算每级的 Light Space Matrix
            float fov = camera.GetFOV();
            float aspectRatio = camera.GetAspectRatio();
            glm::mat4 cameraView = camera.GetViewMatrix();

            for (int i = 0; i < s_Data.CascadeCount; ++i)
            {
                // 1. 计算子视锥体的 8 个角点（世界空间）
                glm::mat4 subProjection = glm::perspective(glm::radians(fov), aspectRatio, cascadeNearPlanes[i], cascadeFarPlanes[i]);
                glm::mat4 invVP = glm::inverse(subProjection * cameraView);

                std::array<glm::vec3, 8> corners;
                int index = 0;
                for (int x = 0; x <= 1; ++x)
                {
                    for (int y = 0; y <= 1; ++y)
                    {
                        for (int z = 0; z <= 1; ++z)
                        {
                            glm::vec4 pt = invVP * glm::vec4(
                                2.0f * x - 1.0f,
                                2.0f * y - 1.0f,
                                2.0f * z - 1.0f,
                                1.0f
                            );
                            corners[index++] = glm::vec3(pt) / pt.w;
                        }
                    }
                }

                // 2. 计算子视锥体中心
                glm::vec3 center(0.0f);
                for (const auto& corner : corners)
                {
                    center += corner;
                }
                center /= 8.0f;

                // 3. 构建光源视图矩阵
                // 修复：当光照方向接近垂直时，up 向量 (0,1,0) 与视线方向平行会导致 lookAt 矩阵退化（NaN）
                glm::vec3 up = glm::vec3(0.0f, 1.0f, 0.0f);
                if (std::abs(glm::dot(lightDir, up)) > 0.99f)
                {
                    up = glm::vec3(0.0f, 0.0f, 1.0f);  // 光照方向接近垂直时使用 Z 轴作为 up
                }
                // 修复：偏移量从固定 50.0f 改为基于子视锥体包围球半径动态计算
                float radius = 0.0f;
                for (const auto& corner : corners)
                {
                    radius = std::max(radius, glm::length(corner - center));
                }
                glm::vec3 lightPos = center - lightDir * (radius + 50.0f);
                glm::mat4 lightView = glm::lookAt(lightPos, center, up);

                // 4. 计算光源空间 AABB
                float minX = std::numeric_limits<float>::max();
                float maxX = std::numeric_limits<float>::lowest();
                float minY = std::numeric_limits<float>::max();
                float maxY = std::numeric_limits<float>::lowest();
                float minZ = std::numeric_limits<float>::max();
                float maxZ = std::numeric_limits<float>::lowest();

                for (const auto& corner : corners)
                {
                    glm::vec4 lightSpaceCorner = lightView * glm::vec4(corner, 1.0f);
                    minX = std::min(minX, lightSpaceCorner.x);
                    maxX = std::max(maxX, lightSpaceCorner.x);
                    minY = std::min(minY, lightSpaceCorner.y);
                    maxY = std::max(maxY, lightSpaceCorner.y);
                    minZ = std::min(minZ, lightSpaceCorner.z);
                    maxZ = std::max(maxZ, lightSpaceCorner.z);
                }

                // 5. 扩展 Z 范围（确保光源"背后"的物体也能投射阴影）
                // 在 glm::lookAt 生成的视图矩阵中，物体在光源前方时 Z 值为负。
                // 扩展 minZ（更负方向）以捕获视锥体外的投影物体。
                float zRange = maxZ - minZ;
                float zPadding = std::max(zRange * 2.0f, 50.0f);
                minZ -= zPadding;
                maxZ += zPadding * 0.1f;

                // 6. 构建正交投影矩阵
                // 关键修复：glm::ortho 使用 orthoRH_NO（右手坐标系，NDC Z 范围 [-1,1]）。
                // 在该模式下，zNear/zFar 参数表示沿 -Z 轴的距离（正值 = 相机前方）。
                // 光源空间中物体 Z 值为负（在光源前方），所以需要取反：
                //   ortho_near = -maxZ（最近的物体，Z 值最大即最接近 0 的负值）
                //   ortho_far  = -minZ（最远的物体，Z 值最小即最负的值）
                glm::mat4 lightProjection = glm::ortho(minX, maxX, minY, maxY, -maxZ, -minZ);

                s_Data.CascadeLightSpaceMatrices[i] = lightProjection * lightView;
            }

        }
        
        // ======== 点光源阴影矩阵计算 ========
        s_Data.PointShadowCount = 0;
        for (int i = 0; i < lightData.PointLightCount && s_Data.PointShadowCount < ShadowAtlas::s_MaxPointLightShadows; ++i)
        {
            if (lightData.PointLightShadows[i].Shadows != ShadowType::None)
            {
                const PointLightData& pointLight = lightData.PointLights[i];
                float farPlane = pointLight.Range;
                glm::vec3 lightPos = pointLight.Position;

                // 计算 6 面 Light Space Matrix
                std::array<glm::mat4, 6> matrices = CalcPointLightMatrices(lightPos, farPlane);

                s_Data.PointShadowData[s_Data.PointShadowCount].LightIndex = i;
                s_Data.PointShadowData[s_Data.PointShadowCount].LightPos = lightPos;
                s_Data.PointShadowData[s_Data.PointShadowCount].FarPlane = farPlane;
                s_Data.PointShadowData[s_Data.PointShadowCount].ShadowBias = lightData.PointLightShadows[i].ShadowBias;
                s_Data.PointShadowData[s_Data.PointShadowCount].ShadowStrength = lightData.PointLightShadows[i].ShadowStrength;
                s_Data.PointShadowData[s_Data.PointShadowCount].ShadowType = static_cast<int>(lightData.PointLightShadows[i].Shadows);

                for (int face = 0; face < 6; ++face)
                {
                    s_Data.PointShadowData[s_Data.PointShadowCount].LightSpaceMatrices[face] = matrices[face];
                }

                ++s_Data.PointShadowCount;
            }
        }

        // ======== 聚光灯阴影矩阵计算 ========
        s_Data.SpotShadowCount = 0;
        for (int i = 0; i < lightData.SpotLightCount && s_Data.SpotShadowCount < ShadowAtlas::s_MaxSpotLightShadows; ++i)
        {
            if (lightData.SpotLightShadows[i].Shadows != ShadowType::None)
            {
                const SpotLightData& spotLight = lightData.SpotLights[i];
                
                // 计算聚光灯 Light Space Matrix（透视投影）
                float halfFov = glm::acos(spotLight.OuterCutoff);
                float fov = halfFov * 2.0f;
                float aspectRatio = 1.0f;
                float nearPlane = 0.1f;
                float farPlane = spotLight.Range;
                
                glm::mat4 projection = glm::perspective(fov, aspectRatio, nearPlane, farPlane);
                
                glm::vec3 direction = glm::normalize(spotLight.Direction);
                glm::vec3 target = spotLight.Position + direction;
                
                glm::vec3 up = glm::vec3(0.0f, 1.0f, 0.0f);
                if (std::abs(glm::dot(direction, up)) > 0.99f)
                {
                    up = glm::vec3(0.0f, 0.0f, 1.0f);
                }
                
                glm::mat4 view = glm::lookAt(spotLight.Position, target, up);
                glm::mat4 lightSpaceMatrix = projection * view;
                
                s_Data.SpotShadowData[s_Data.SpotShadowCount].LightIndex = i;
                s_Data.SpotShadowData[s_Data.SpotShadowCount].LightSpaceMatrix = lightSpaceMatrix;
                s_Data.SpotShadowData[s_Data.SpotShadowCount].ShadowBias = lightData.SpotLightShadows[i].ShadowBias;
                s_Data.SpotShadowData[s_Data.SpotShadowCount].ShadowStrength = lightData.SpotLightShadows[i].ShadowStrength;
                s_Data.SpotShadowData[s_Data.SpotShadowCount].ShadowType = static_cast<int>(lightData.SpotLightShadows[i].Shadows);
                
                ++s_Data.SpotShadowCount;
            }
        }
        
        // 清空绘制命令列表
        s_Data.OpaqueDrawCommands.clear();
        s_Data.TransparentDrawCommands.clear();
    
        // 缓存相机位置
        s_Data.CameraPosition = camera.GetPosition();
        
        // 缓存相机矩阵（SkyboxPass 需要 View/Projection 矩阵）
        s_Data.CameraViewMatrix = camera.GetViewMatrix();
        s_Data.CameraProjectionMatrix = camera.GetProjectionMatrix();
    }

    void Renderer3D::EndScene()
    {
        // ---- 排序不透明物体（按 SortKey 升序，聚合相同 Shader） ----
        std::sort(s_Data.OpaqueDrawCommands.begin(), s_Data.OpaqueDrawCommands.end(), [](const DrawCommand& a, const DrawCommand& b)
        {
            return a.SortKey < b.SortKey;
        });
        
        // ---- 排序透明物体（按距离降序，从远到近） ----
        std::sort(s_Data.TransparentDrawCommands.begin(), s_Data.TransparentDrawCommands.end(), [](const DrawCommand& a, const DrawCommand& b)
        {
            return a.DistanceToCamera > b.DistanceToCamera;  // 远的先画
        });
        
        // ---- 构建 RenderContext（包含阴影数据） ----
        RenderContext context;
        context.OpaqueDrawCommands = &s_Data.OpaqueDrawCommands;
        context.TransparentDrawCommands = &s_Data.TransparentDrawCommands;
        context.TargetFramebuffer = s_Data.TargetFramebuffer;
        context.ClearColor = s_Data.ClearColor;
        context.Stats = &s_Data.Stats;
        
        // 阴影数据
        context.ShadowEnabled = s_Data.ShadowEnabled;
        context.ShadowBias = s_Data.ShadowBias;
        context.ShadowStrength = s_Data.ShadowStrength;
        context.ShadowShadowType = s_Data.ShadowShadowType;
        
        // CSM 数据
        context.CascadeCount = s_Data.CascadeCount;
        context.ShadowMapResolution = s_Data.ShadowMapResolution;
        context.CameraViewMatrix = s_Data.CameraViewMatrix;
        for (int i = 0; i < s_Data.CascadeCount; ++i)
        {
            context.CascadeLightSpaceMatrices[i] = s_Data.CascadeLightSpaceMatrices[i];
            context.CascadeFarPlanes[i] = s_Data.CascadeFarPlanes[i];
        }
        
        // 获取 Shadow Map 纹理 ID（ShadowPass 持有 FBO）
        auto shadowPass = s_Data.Pipeline.GetPass<ShadowPass>();
        if (shadowPass)
        {
            context.CascadeShadowMapArrayTextureID = shadowPass->GetShadowMapTextureID();
            context.TranslucentShadowMapTextureID = shadowPass->GetTranslucentShadowMapTextureID();
            // 仅当场景中存在透明物体时才启用 Translucent Shadow Map，
            // 避免删除最后一个透明物体后残留阴影数据被采样
            bool hasTransparentObjects = context.TransparentDrawCommands && !context.TransparentDrawCommands->empty();
            context.TranslucentShadowEnabled = hasTransparentObjects;
            context.DebugCSMVisualize = shadowPass->IsDebugCSMVisualize();
            
            // Shadow Atlas 数据
            context.ShadowAtlasTextureID = shadowPass->GetShadowAtlasTextureID();
            context.ShadowAtlasSize = shadowPass->GetShadowAtlasSize();
        }
        
        // ---- 聚光灯阴影数据 ----
        context.ShadowData.SpotLightShadowCount = s_Data.SpotShadowCount;
        for (int i = 0; i < s_Data.SpotShadowCount; ++i)
        {
            context.ShadowData.SpotLights[i].LightIndex = s_Data.SpotShadowData[i].LightIndex;
            context.ShadowData.SpotLights[i].LightSpaceMatrix = s_Data.SpotShadowData[i].LightSpaceMatrix;
            context.ShadowData.SpotLights[i].ShadowBias = s_Data.SpotShadowData[i].ShadowBias;
            context.ShadowData.SpotLights[i].ShadowStrength = s_Data.SpotShadowData[i].ShadowStrength;
            context.ShadowData.SpotLights[i].ShadowType = s_Data.SpotShadowData[i].ShadowType;
            
            // 从 ShadowAtlas 获取 Tile 的 ViewportScaleBias
            if (shadowPass)
            {
                int tileIdx = shadowPass->GetShadowAtlas().GetSpotLightTileIndex(i);
                context.ShadowData.SpotLights[i].AtlasScaleBias = shadowPass->GetShadowAtlas().GetTile(tileIdx).ViewportScaleBias;
            }
        }
        
        // ---- 点光源阴影数据 ----
        context.ShadowData.PointLightShadowCount = s_Data.PointShadowCount;
        for (int i = 0; i < s_Data.PointShadowCount; ++i)
        {
            context.ShadowData.PointLights[i].LightIndex = s_Data.PointShadowData[i].LightIndex;
            context.ShadowData.PointLights[i].LightPos = s_Data.PointShadowData[i].LightPos;
            context.ShadowData.PointLights[i].FarPlane = s_Data.PointShadowData[i].FarPlane;
            context.ShadowData.PointLights[i].ShadowBias = s_Data.PointShadowData[i].ShadowBias;
            context.ShadowData.PointLights[i].ShadowStrength = s_Data.PointShadowData[i].ShadowStrength;
            context.ShadowData.PointLights[i].ShadowType = s_Data.PointShadowData[i].ShadowType;

            for (int face = 0; face < 6; ++face)
            {
                context.ShadowData.PointLights[i].LightSpaceMatrices[face] = s_Data.PointShadowData[i].LightSpaceMatrices[face];

                // 从 ShadowAtlas 获取 Tile 的 ViewportScaleBias
                if (shadowPass)
                {
                    int tileIdx = shadowPass->GetShadowAtlas().GetPointLightTileStart(i) + face;
                    context.ShadowData.PointLights[i].AtlasScaleBias[face] = shadowPass->GetShadowAtlas().GetTile(tileIdx).ViewportScaleBias;
                }
            }
        }
        
        // 天空盒数据
        context.SkyboxMaterial = s_Data.Environment.SkyboxMaterial;
        context.SkyboxViewMatrix = s_Data.CameraViewMatrix;
        context.SkyboxProjectionMatrix = s_Data.CameraProjectionMatrix;
        
        // HDR / 后处理数据
        auto postProcessPass = s_Data.Pipeline.GetPass<PostProcessPass>();
        if (postProcessPass)
        {
            context.HDR_FBO = postProcessPass->GetHDR_FBO();
        }
        context.PostProcess = s_Data.PostProcess;
        
        // IBL 数据
        const IBLData& iblData = IBLPrecompute::GetIBLData();
        context.IBLEnabled = iblData.Valid;
        context.IrradianceMapID = iblData.IrradianceMapID;
        context.PrefilterMapID = iblData.PrefilterMapID;
        context.BRDFLUTID = iblData.BRDFLUTID;
        context.PrefilterMaxMipLevel = static_cast<float>(iblData.PrefilterMaxMipLevel);
        
        // 环境设置
        context.EnvironmentSource = s_Data.Environment.Source;
        context.AmbientColor = s_Data.Environment.AmbientColor;
        context.IBLDiffuseIntensity = s_Data.Environment.DiffuseIntensity;
        context.IBLSpecularIntensity = s_Data.Environment.SpecularIntensity;
        
        // ---- 执行 Shadow 分组（ShadowPass） ----
        s_Data.Pipeline.ExecuteGroup("Shadow", context);
        
        // ---- 执行 Main 分组（OpaquePass + PickingPass -> HDR FBO） ----
        s_Data.Pipeline.ExecuteGroup("Main", context);
        
        // ---- 执行 PostProcess 分组（PostProcessPass -> 主 FBO） ----
        s_Data.Pipeline.ExecuteGroup("PostProcess", context);

        // ======== 提取描边物体到独立列表 ========
        // 将描边所需的最小几何数据从 OpaqueDrawCommands / TransparentDrawCommands 中提取到 OutlineDrawCommands
        // 使 DrawCommands 的生命周期在 EndScene() 结束时终止，职责清晰
        s_Data.OutlineDrawCommands.clear();
        if (!s_Data.OutlineEntityIDs.empty())
        {
            // 从不透明物体中提取
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
            // 从透明物体中提取（透明物体也需要描边）
            for (const DrawCommand& cmd : s_Data.TransparentDrawCommands)
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
        // 立即清空 DrawCommands，生命周期在 EndScene() 结束
        s_Data.OpaqueDrawCommands.clear();
        s_Data.TransparentDrawCommands.clear();
    }

    void Renderer3D::DrawMesh(const glm::mat4& transform, Ref<Mesh>& mesh, const std::vector<Ref<Material>>& materials, int entityID)
    {
        // 准备顶点数据（直接使用 Mesh 内部数据，避免冗余拷贝）
        const auto& vertices = mesh->GetVertices();
        uint32_t dataSize = sizeof(Vertex) * static_cast<uint32_t>(vertices.size());
        mesh->SetVertexBufferData(vertices.data(), dataSize);
        
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
        
            // 根据材质透明度分流到对应的绘制命令列表
            if (material->IsTransparent())
            {
                s_Data.TransparentDrawCommands.push_back(cmd);
            }
            else
            {
                s_Data.OpaqueDrawCommands.push_back(cmd);
            }
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

    Ref<Material>& Renderer3D::GetSkyboxMaterial()
    {
        return s_Data.Environment.SkyboxMaterial;
    }

    void Renderer3D::SetSkyboxMaterial(const Ref<Material>& skyboxMaterial)
    {
        s_Data.Environment.SkyboxMaterial = skyboxMaterial;
        
        // 从天空盒材质中获取 Cubemap 纹理，重新生成 IBL 数据
        if (skyboxMaterial)
        {
            Ref<TextureCube> cubemap = skyboxMaterial->GetTextureCube("u_SkyboxMap");
            if (cubemap)
            {
                IBLPrecompute::GenerateFromCubemap(cubemap->GetRendererID(), s_Data.Environment.ReflectionResolution);
                LF_CORE_INFO("IBL data regenerated for new skybox");
                return;
            }
        }
        
        // 天空盒为空，IBL 数据标记为无效
        LF_CORE_WARN("SetSkyboxMaterial: No valid cubemap found, IBL disabled");
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

    void Renderer3D::SetClearColor(const glm::vec4& color)
    {
        s_Data.ClearColor = color;
    }

    void Renderer3D::SetOutlineEntities(const std::unordered_set<int>& entityIDs)
    {
        s_Data.OutlineEntityIDs = entityIDs;
    }

    void Renderer3D::SetOutlineColor(const glm::vec4& color)
    {
        s_Data.OutlineColor = color;
    }

    void Renderer3D::ResizePipeline(uint32_t width, uint32_t height)
    {
        s_Data.Pipeline.Resize(width, height);
    }

    RenderPipeline& Renderer3D::GetPipeline()
    {
        return s_Data.Pipeline;
    }

    void Renderer3D::SetPostProcessSettings(const PostProcessSettings& settings)
    {
        s_Data.PostProcess = settings;
        
        // 同步效果参数到 PostProcessStack 中的各个 Effect
        auto postProcessPass = s_Data.Pipeline.GetPass<PostProcessPass>();
        if (postProcessPass)
        {
            auto& stack = postProcessPass->GetPostProcessStack();
            
            // 同步 Bloom 参数
            auto bloom = stack.GetEffect<BloomEffect>();
            if (bloom)
            {
                bloom->Enabled = settings.BloomEnabled;
                bloom->Threshold = settings.BloomThreshold;
                bloom->Intensity = settings.BloomIntensity;
                bloom->Iterations = settings.BloomIterations;
            }
            
            // 同步 FXAA 参数
            auto fxaa = stack.GetEffect<FXAAEffect>();
            if (fxaa)
            {
                fxaa->Enabled = settings.FXAAEnabled;
            }
            
            // 同步 Vignette 参数
            auto vignette = stack.GetEffect<VignetteEffect>();
            if (vignette)
            {
                vignette->Enabled = settings.VignetteEnabled;
                vignette->VignetteIntensity = settings.VignetteIntensity;
                vignette->VignetteSmoothness = settings.VignetteSmoothness;
            }
        }
    }

    void Renderer3D::SetEnvironmentSettings(const EnvironmentSettings& settings)
    {
        bool needRegenerateIBL = false;
        
        // 检测 SkyboxMaterial 是否变化
        if (s_Data.Environment.SkyboxMaterial != settings.SkyboxMaterial)
        {
            needRegenerateIBL = true;
        }
        
        // 检测 Prefilter Map 分辨率是否变化
        if (s_Data.Environment.ReflectionResolution != settings.ReflectionResolution)
        {
            needRegenerateIBL = true;
        }
        
        // 需要重新生成 IBL 数据
        if (needRegenerateIBL)
        {
            const Ref<Material>& skyboxMat = settings.SkyboxMaterial;
            if (skyboxMat)
            {
                Ref<TextureCube> cubemap = skyboxMat->GetTextureCube("u_SkyboxMap");
                if (cubemap)
                {
                    IBLPrecompute::GenerateFromCubemap(cubemap->GetRendererID(), settings.ReflectionResolution);
                    LF_CORE_INFO("IBL data regenerated (skybox or resolution changed)");
                }
            }
        }
        
        s_Data.Environment = settings;
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
        
        // ---- 执行 Outline 分组（SilhouettePass + OutlineCompositePass） ----
        s_Data.Pipeline.ExecuteGroup("Outline", context);
        
        // 清空描边命令列表
        s_Data.OutlineDrawCommands.clear();
    }
}
