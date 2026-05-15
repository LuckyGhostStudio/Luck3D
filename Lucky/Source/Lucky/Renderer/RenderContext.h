#pragma once

#include "Lucky/Core/Base.h"
#include "Framebuffer.h"
#include "Mesh.h"
#include "Material.h"
#include "Renderer3D.h"

#include <glm/glm.hpp>
#include <vector>
#include <unordered_set>

namespace Lucky
{
    /// <summary>
    /// 环境光来源
    /// </summary>
    enum class AmbientSource
    {
        Skybox = 0,     // 从天空盒 IBL 获取环境光（默认）
        Color = 1       // 使用纯色环境光
    };

    /// <summary>
    /// 环境设置参数（对标 Unity Lighting 面板 Environment 区域）
    /// 由 LightingPanel 编辑，Scene 每帧传递给 Renderer3D
    /// </summary>
    struct EnvironmentSettings
    {
        // ---- Skybox ----
        Ref<Material> SkyboxMaterial;                           // 天空盒材质（nullptr 不渲染天空盒）
        
        // ---- Environment Lighting ----
        AmbientSource Source = AmbientSource::Skybox;           // 环境光来源（默认 Skybox）
        glm::vec3 AmbientColor = glm::vec3(0.212f, 0.227f, 0.259f); // 纯色环境光颜色（Source=Color 时使用）
        float DiffuseIntensity = 1.0f;                          // IBL 漫反射强度（Source=Skybox 时使用）

        // ---- Environment Reflections ----
        float SpecularIntensity = 1.0f;                         // IBL 镜面反射强度（对应 Unity 的 Intensity Multiplier）
        int ReflectionResolution = 128;                         // 反射 Cubemap 分辨率（Prefilter Map）
    };

    /// <summary>
    /// Tonemapping 模式枚举
    /// </summary>
    enum class TonemapMode
    {
        Reinhard = 0,       // Reinhard
        ACES = 1,           // ACES Filmic（默认）
        Uncharted2 = 2      // Uncharted 2
    };

    /// <summary>
    /// 后处理参数（从 PostProcessVolumeComponent 收集）
    /// </summary>
    struct PostProcessSettings
    {
        // Tonemapping
        TonemapMode Tonemap = TonemapMode::ACES;  // 默认 ACES
        float Exposure = 1.0f;

        // Bloom
        bool BloomEnabled = false;
        float BloomThreshold = 1.0f;
        float BloomIntensity = 1.0f;
        int BloomIterations = 5;

        // FXAA
        bool FXAAEnabled = false;

        // Vignette
        bool VignetteEnabled = false;
        float VignetteIntensity = 0.5f;
        float VignetteSmoothness = 2.0f;
    };
    /// <summary>
    /// 绘制命令：描述一次 DrawCall 所需的全部信息
    /// 从 Renderer3D::DrawMesh() 中收集，在 EndScene() 中排序后传递给各 Pass
    /// </summary>
    struct DrawCommand
    {
        glm::mat4 Transform;            // 模型变换矩阵
        Ref<Mesh> MeshData;             // 网格引用
        const SubMesh* SubMeshPtr;      // SubMesh 指针（指向 Mesh 内部数据，仅在当前帧的 EndScene() 作用域内有效，生命周期由 MeshData 的 Ref 保证）
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
        const SubMesh* SubMeshPtr;      // SubMesh 指针（指向 Mesh 内部数据，仅在当前帧的 RenderOutline() 作用域内有效，生命周期由 MeshData 的 Ref 保证）
    };
    
    /// <summary>
    /// 渲染上下文：包含一帧渲染所需的所有数据
    /// 由 Renderer3D 在 EndScene() / RenderOutline() 中构建，传递给 RenderPipeline
    /// 各 Pass 从中获取各自需要的数据
    /// </summary>
    struct RenderContext
    {
        // ---- DrawCommand 列表（已排序） ----
        const std::vector<DrawCommand>* OpaqueDrawCommands = nullptr;       // 不透明物体绘制命令（已按 SortKey 排序）
        const std::vector<DrawCommand>* TransparentDrawCommands = nullptr;  // 透明物体绘制命令（已按距离从远到近排序）
        
        // ---- Outline 数据 ----
        const std::vector<OutlineDrawCommand>* OutlineDrawCommands = nullptr;   // 描边绘制命令
        const std::unordered_set<int>* OutlineEntityIDs = nullptr;              // 需要描边的 EntityID 集合
        glm::vec4 OutlineColor = glm::vec4(1.0f, 0.4f, 0.0f, 1.0f);           // 描边颜色
        float OutlineWidth = 2.0f;                                              // 描边宽度（像素）
        bool OutlineEnabled = true;                                             // 是否启用描边
        
        // ---- FBO 引用 ----
        Ref<Framebuffer> TargetFramebuffer;     // 主 FBO（OpaquePass / PickingPass / OutlineCompositePass 使用）
        
        // ---- 清屏颜色（由 SceneViewportPanel 设置，传递给 HDR FBO 清屏） ----
        glm::vec4 ClearColor = glm::vec4(0.0f, 0.0f, 0.0f, 1.0f);
        
        // ---- 阴影数据 ----
        bool ShadowEnabled = false;                     // 是否启用阴影
        float ShadowBias = 0.005f;                      // 阴影偏移（减少 Shadow Acne）
        float ShadowStrength = 1.0f;                    // 阴影强度 [0, 1]
        ShadowType ShadowShadowType = ShadowType::None; // 阴影类型（Hard/Soft）
        
        // ---- CSM 数据 ----
        int CascadeCount = 4;                                       // 级联数量
        glm::mat4 CascadeLightSpaceMatrices[4];                     // 每级 Light Space Matrix
        float CascadeFarPlanes[4] = { 0.0f };                       // 每级远平面距离（视图空间 z 值）
        uint32_t CascadeShadowMapArrayTextureID = 0;                // CSM Texture2DArray 深度纹理 ID
        int ShadowMapResolution = 2048;                             // 每级 Shadow Map 分辨率
        glm::mat4 CameraViewMatrix = glm::mat4(1.0f);               // 相机视图矩阵（用于计算片段的视图空间深度）
        
        // ---- Translucent Shadow Map 数据（仅 Cascade 0） ----
        uint32_t TranslucentShadowMapTextureID = 0;     // Translucent Shadow Map 颜色纹理 ID
        bool TranslucentShadowEnabled = false;          // 是否启用 Translucent Shadow Map
        
        // ---- 天空盒数据（Material 驱动） ----
        Ref<Material> SkyboxMaterial;                              // 天空盒材质（nullptr 表示不渲染天空盒）
        glm::mat4 SkyboxViewMatrix = glm::mat4(1.0f);              // 相机 View 矩阵（SkyboxPass 用于移除平移分量）
        glm::mat4 SkyboxProjectionMatrix = glm::mat4(1.0f);        // 相机 Projection 矩阵
        
        // ---- HDR / 后处理数据 ----
        Ref<Framebuffer> HDR_FBO;               // HDR FBO（由 PostProcessPass 提供，Main 分组 Pass 渲染到此 FBO）
        PostProcessSettings PostProcess;        // 后处理参数
        
        // ---- IBL 数据 ----
        bool IBLEnabled = false;                    // 是否启用 IBL
        uint32_t IrradianceMapID = 0;               // Irradiance Cubemap 纹理 ID
        uint32_t PrefilterMapID = 0;                // Prefiltered Environment Cubemap 纹理 ID
        uint32_t BRDFLUTID = 0;                     // BRDF LUT Texture2D 纹理 ID
        float PrefilterMaxMipLevel = 4.0f;          // Prefiltered Map 最大 Mip Level
        
        // ---- 环境设置 ----
        AmbientSource EnvironmentSource = AmbientSource::Skybox;    // 环境光来源
        glm::vec3 AmbientColor = glm::vec3(0.1f, 0.1f, 0.1f);     // 纯色环境光颜色
        float IBLDiffuseIntensity = 1.0f;                           // IBL 漫反射强度
        float IBLSpecularIntensity = 1.0f;                          // IBL 镜面反射强度
        
        // ======== Shadow Atlas 数据（为多光源阴影提供基础设施） ========
        uint32_t ShadowAtlasTextureID = 0;          // Shadow Atlas 深度纹理 ID
        uint32_t ShadowAtlasSize = 4096;            // Atlas 纹理尺寸

        /// <summary>
        /// Shader 端需要的每光源阴影数据
        /// 由 Renderer3D::EndScene() 从 ShadowAtlas 中提取，传递给 OpaquePass/TransparentPass
        /// </summary>
        struct ShaderShadowData
        {
            // ---- 方向光阴影（最多 2 个方向光 × 4 级联） ----
            int DirLightShadowCount = 0;                        // 投射阴影的方向光数量
            struct DirLightShadow
            {
                int CascadeCount = 4;
                glm::mat4 LightSpaceMatrices[4];                // 每级 Light Space Matrix
                glm::vec4 AtlasScaleBias[4];                    // 每级在 Atlas 中的 UV 变换
                float CascadeFarPlanes[4] = { 0.0f };           // 每级远平面距离（视图空间）
                float ShadowBias = 0.0003f;
                float ShadowStrength = 1.0f;
                int ShadowType = 1;                             // 1=Hard, 2=Soft
            };
            DirLightShadow DirLights[2];                        // 最多 2 个方向光投射阴影

            // ---- 聚光灯阴影（最多 4 个） ----
            int SpotLightShadowCount = 0;
            struct SpotLightShadow
            {
                int LightIndex = -1;                            // 在 SpotLights[] 中的索引
                glm::mat4 LightSpaceMatrix;                     // 光源空间 VP 矩阵
                glm::vec4 AtlasScaleBias;                       // 在 Atlas 中的 UV 变换
                float ShadowBias = 0.001f;
                float ShadowStrength = 1.0f;
                int ShadowType = 1;
            };
            SpotLightShadow SpotLights[4];

            // ---- 点光源阴影（最多 2 个 × 6 面） ----
            int PointLightShadowCount = 0;
            struct PointLightShadow
            {
                int LightIndex = -1;                            // 在 PointLights[] 中的索引
                glm::vec3 LightPos = glm::vec3(0.0f);           // 点光源世界空间位置
                glm::mat4 LightSpaceMatrices[6];                // 6 面 Light Space Matrix
                glm::vec4 AtlasScaleBias[6];                    // 6 面在 Atlas 中的 UV 变换
                float ShadowBias = 0.05f;
                float ShadowStrength = 1.0f;
                int ShadowType = 1;
                float FarPlane = 25.0f;                         // 点光源阴影远平面（= Range）
            };
            PointLightShadow PointLights[2];
        };
        ShaderShadowData ShadowData;

        // ---- 调试标志 ----
        bool DebugCSMVisualize = false;         // CSM 级联颜色可视化（由 ShadowPass 控制）
        
        // ---- 统计数据（可写） ----
        Renderer3D::Statistics* Stats = nullptr;    // 渲染统计（DrawCalls、TriangleCount）
    };
}
