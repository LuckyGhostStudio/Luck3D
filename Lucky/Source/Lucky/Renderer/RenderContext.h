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
        glm::mat4 LightSpaceMatrix = glm::mat4(1.0f);   // 光源空间矩阵（正交投影 × 光源视图）
        uint32_t ShadowMapTextureID = 0;                // Shadow Map 纹理 ID（供 OpaquePass 绑定）
        float ShadowBias = 0.005f;                      // 阴影偏移（减少 Shadow Acne）
        float ShadowStrength = 1.0f;                    // 阴影强度 [0, 1]
        ShadowType ShadowShadowType = ShadowType::None; // 阴影类型（Hard/Soft）
        
        // ---- HDR / 后处理数据 ----
        Ref<Framebuffer> HDR_FBO;               // HDR FBO（由 PostProcessPass 提供，Main 分组 Pass 渲染到此 FBO）
        PostProcessSettings PostProcess;        // 后处理参数
        
        // ---- 统计数据（可写） ----
        Renderer3D::Statistics* Stats = nullptr;    // 渲染统计（DrawCalls、TriangleCount）
    };
}
