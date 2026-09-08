#pragma once

#include "Framebuffer.h"
#include "CameraRenderData.h"
#include "LightRenderData.h"
#include "RenderContext.h"
#include "RenderPipeline.h"
#include "UniformBuffer.h"
#include "ShadowAtlas.h"

#include "Lucky/Core/Base.h"

#include <glm/glm.hpp>

#include <cstdint>
#include <cstring>
#include <unordered_set>
#include <vector>

namespace Lucky
{
    class Mesh;
    class Material;
    class Texture2D;

    /// <summary>
    /// 相机 UBO 数据
    /// </summary>
    struct CameraUBOData
    {
        glm::mat4 ViewProjectionMatrix;     // VP 矩阵
        glm::mat4 InvProjectionMatrix;      // 投影矩阵的逆
        glm::vec3 Position;                 // 相机位置
        char padding[4];                    // 填充到 16 字节对齐
    };

    /// <summary>
    /// 光照 UBO 数据
    /// </summary>
    struct LightUBOData
    {
        int DirectionalLightCount;
        int PointLightCount;
        int SpotLightCount;
        char padding[4];

        DirectionalLightData DirectionalLights[s_MaxDirectionalLights];
        PointLightData PointLights[s_MaxPointLights];
        SpotLightData SpotLights[s_MaxSpotLights];
    };

    /// <summary>
    /// 聚光灯阴影缓存数据（每 SceneRenderer 独立）
    /// </summary>
    struct SpotShadowCacheData
    {
        int LightIndex = -1;                            // 在 SpotLights[] 中的索引
        glm::mat4 LightSpaceMatrix = glm::mat4(1.0f);   // 光源空间 VP 矩阵
        float ShadowBias = 0.001f;                      // 阴影偏移
        float ShadowStrength = 1.0f;                    // 阴影强度
        int ShadowType = 1;                             // 1=Hard, 2=Soft
    };

    /// <summary>
    /// 点光源阴影缓存数据（每 SceneRenderer 独立）
    /// </summary>
    struct PointShadowCacheData
    {
        int LightIndex = -1;                            // 在 PointLights[] 中的索引
        glm::vec3 LightPos = glm::vec3(0.0f);           // 世界空间位置
        float FarPlane = 25.0f;                         // 远平面距离（= Range）
        glm::mat4 LightSpaceMatrices[6];                // 6 面 Light Space Matrix
        float ShadowBias = 0.05f;                       // 阴影偏移
        float ShadowStrength = 1.0f;                    // 阴影强度
        int ShadowType = 1;                             // 1=Hard, 2=Soft
    };

    /// <summary>
    /// SceneRenderer 构造参数：决定 FBO 附件与 Pass 组合
    /// </summary>
    struct SceneRendererSpec
    {
        uint32_t Width = 1280;                  // 初始视口宽
        uint32_t Height = 720;                  // 初始视口高

        bool EnableShadow = true;               // 启用 ShadowPass
        bool EnablePicking = false;             // 附加 RED_INTEGER 拾取附件；启用 PickingPass
        bool EnableOutline = false;             // 启用 SilhouettePass + OutlineCompositePass；RenderOutline() 可用
        bool EnableDebugVisualize = false;      // 启用 DebugVisualizePass（CSM 级联可视化等）
        bool EnablePostProcess = true;          // 启用 PostProcessPass（HDR FBO + Tonemapping + Bloom/FXAA/Vignette）
    };

    /// <summary>
    /// 场景渲染器：可实例化的渲染管线执行器
    /// 每个视口面板持有一个独立实例；持有完整的一次渲染所需状态
    /// </summary>
    class SceneRenderer
    {
    public:
        using Statistics = RendererStats;   // 兼容旧名：SceneRenderer::Statistics

        SceneRenderer() = default;
        ~SceneRenderer() = default;

        void Init(const SceneRendererSpec& spec);
        void Shutdown();

        void OnViewportResize(uint32_t width, uint32_t height);

        void BeginScene(const CameraRenderData& cam, const LightRenderData& lightData);
        void SubmitMesh(const glm::mat4& transform, Ref<Mesh>& mesh, const std::vector<Ref<Material>>& materials, int entityID = -1);
        void SubmitSprite(const glm::mat4& transform,
                          const Ref<Texture2D>& texture,
                          const glm::vec4& color,
                          bool flipX,
                          bool flipY,
                          const glm::vec4& uvRect,
                          float tilingFactor,
                          const Ref<Material>& material,
                          int sortingOrder,
                          int entityID = -1);
        void EndScene();
        void RenderOutline();

        void SetClearColor(const glm::vec4& color) { m_ClearColor = color; }
        void SetOutlineEntities(const std::unordered_set<int>& entityIDs) { m_OutlineEntityIDs = entityIDs; }
        void SetOutlineColor(const glm::vec4& color) { m_OutlineColor = color; }
        void SetPostProcessSettings(const PostProcessSettings& settings);
        void SetEnvironmentSettings(const EnvironmentSettings& settings);

        const Ref<Framebuffer>& GetFramebuffer() const { return m_Framebuffer; }
        uint32_t GetFinalColorAttachmentID() const;

        /// <summary>
        /// 读取拾取附件像素（仅 Spec.EnablePicking == true 时可用）
        /// </summary>
        int ReadPixelEntityID(int x, int y) const;

        const RenderPipeline& GetPipeline() const { return m_Pipeline; }
        RenderPipeline& GetPipeline() { return m_Pipeline; }

        const Statistics& GetStats() const { return m_Stats; }
        void ResetStats() { m_Stats = {}; }

        const SceneRendererSpec& GetSpec() const { return m_Spec; }

        /// <summary>
        /// 设置"主 SceneRenderer"（供 RenderPipelinePanel 读取 Stats 等调试信息使用）
        /// 通常由 SceneViewportPanel 在 ctor 中调用；dtor 中传 nullptr 清空
        /// </summary>
        static void SetPrimary(SceneRenderer* renderer) { s_Primary = renderer; }
        static SceneRenderer* GetPrimary() { return s_Primary; }
    private:
        /// <summary>
        /// 按 Spec 布尔开关构造 Pass 组合，写入 m_Pipeline
        /// </summary>
        void BuildPipeline();

        /// <summary>
        /// 从 DrawCommands 中提取选中实体到 m_OutlineDrawCommands
        /// 在 EndScene() 末尾调用，为后续 RenderOutline() 做准备
        /// </summary>
        void ExtractOutlineDrawCommands();

    private:
        static SceneRenderer* s_Primary;    // 主 SceneRenderer 指针（调试面板读取用）

        SceneRendererSpec m_Spec;

        Ref<Framebuffer> m_Framebuffer;                 // 目标 FBO（内部持有）
        RenderPipeline m_Pipeline;                      // 独立的 Pass 组合

        // ---- UBO ----
        Ref<UniformBuffer> m_CameraUniformBuffer;
        Ref<UniformBuffer> m_LightUniformBuffer;
        CameraUBOData m_CameraBuffer{};
        LightUBOData m_LightBuffer{};

        // ---- 相机缓存 ----
        glm::vec3 m_CameraPosition{ 0.0f };
        glm::mat4 m_CameraViewMatrix{ 1.0f };
        glm::mat4 m_CameraProjectionMatrix{ 1.0f };

        // ---- DrawCommand 队列 ----
        std::vector<DrawCommand> m_OpaqueDrawCommands;
        std::vector<DrawCommand> m_TransparentDrawCommands;
        std::vector<SpriteDrawCommand> m_SpriteDrawCommands;
        std::vector<OutlineDrawCommand> m_OutlineDrawCommands;

        // ---- Outline 参数 ----
        std::unordered_set<int> m_OutlineEntityIDs;
        glm::vec4 m_OutlineColor = glm::vec4(1.0f, 0.4f, 0.0f, 1.0f);
        float m_OutlineWidth = 2.0f;
        bool m_OutlineEnabled = true;

        // ---- 清屏色 ----
        glm::vec4 m_ClearColor = glm::vec4(0.0f, 0.0f, 0.0f, 1.0f);

        // ---- 方向光阴影参数（BeginScene 中从 LightRenderData 计算） ----
        bool m_ShadowEnabled = false;
        float m_ShadowBias = 0.005f;
        float m_ShadowStrength = 1.0f;
        ShadowType m_ShadowShadowType = ShadowType::None;
        int m_CascadeCount = 4;
        glm::mat4 m_CascadeLightSpaceMatrices[s_MaxCascadeCount];
        float m_CascadeFarPlanes[s_MaxCascadeCount] = { 0.0f };
        int m_ShadowMapResolution = 2048;

        // ---- 聚光灯 / 点光源阴影缓存 ----
        SpotShadowCacheData m_SpotShadowData[ShadowAtlas::s_MaxSpotLightShadows];
        int m_SpotShadowCount = 0;
        PointShadowCacheData m_PointShadowData[ShadowAtlas::s_MaxPointLightShadows];
        int m_PointShadowCount = 0;

        // ---- 后处理与环境（当前渲染当次使用的副本） ----
        PostProcessSettings m_PostProcess;
        EnvironmentSettings m_Environment;

        // ---- 统计 ----
        Statistics m_Stats;
    };
}
