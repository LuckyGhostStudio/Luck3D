#pragma once

#include "Lucky/Renderer/RenderPass.h"
#include "Lucky/Renderer/Shader.h"
#include "Lucky/Renderer/Framebuffer.h"
#include "Lucky/Renderer/ShadowAtlas.h"

namespace Lucky
{
    /// <summary>
    /// 阴影 Pass：从所有投射阴影的光源视角渲染场景深度
    /// 支持：方向光 CSM（Texture2DArray）+ 聚光灯（Shadow Atlas）
    /// 属于 "Shadow" 分组，在 OpaquePass 之前执行
    /// </summary>
    class ShadowPass : public RenderPass
    {
    public:
        void Init() override;
        void Execute(const RenderContext& context) override;
        void Resize(uint32_t width, uint32_t height) override;
        const std::string& GetName() const override { static std::string name = "ShadowPass"; return name; }
        const std::string& GetGroup() const override { static std::string group = "Shadow"; return group; }

        /// <summary>
        /// 绘制调试 GUI（CSM 级联可视化开关等）
        /// </summary>
        void OnDebugGUI() override;

        /// <summary>
        /// 获取 CSM 级联可视化开关状态（供 OpaquePass 读取并传递给 Shader）
        /// </summary>
        bool IsDebugCSMVisualize() const { return m_DebugCSMVisualize; }

        /// <summary>
        /// 获取 CSM Texture2DArray 深度纹理 ID（包含所有级联的深度数据）
        /// 供 OpaquePass/TransparentPass 绑定到纹理单元，Shader 使用 sampler2DArray 采样
        /// </summary>
        uint32_t GetShadowMapTextureID() const;

        /// <summary>
        /// 获取 Translucent Shadow Map Array 颜色纹理 ID（所有级联的透明物体颜色衰减）
        /// </summary>
        uint32_t GetTranslucentShadowMapTextureID() const;

        /// <summary>
        /// 获取 Shadow Atlas 管理器（供 Renderer3D 查询 Tile 信息）
        /// </summary>
        const ShadowAtlas& GetShadowAtlas() const { return m_ShadowAtlas; }

        /// <summary>
        /// 获取 Shadow Atlas 深度纹理 ID
        /// </summary>
        uint32_t GetShadowAtlasTextureID() const;

        /// <summary>
        /// 获取 Shadow Atlas 纹理尺寸
        /// </summary>
        uint32_t GetShadowAtlasSize() const { return m_ShadowAtlas.GetAtlasSize(); }

    private:
        /// <summary>
        /// 重新创建 CSM FBO（当分辨率或级联数量变化时调用）
        /// </summary>
        void RecreateFBOs();

        /// <summary>
        /// 渲染所有聚光灯阴影到 Shadow Atlas
        /// </summary>
        void RenderSpotLightShadows(const RenderContext& context);

        // ---- CSM 资源（方向光，保留现有实现） ----
        Ref<Framebuffer> m_CSMFramebuffer;              // CSM Texture2DArray FBO（多层深度纹理）
        Ref<Framebuffer> m_TranslucentShadowFBO;        // Translucent Shadow Map FBO（RGBA8 Array 颜色 + 深度 Array，所有级联）
        Ref<Shader> m_ShadowShader;                     // Shadow Pass Shader
        uint32_t m_ShadowMapResolution = 2048;          // Shadow Map 分辨率
        int m_CascadeCount = 4;                         // 当前级联数量

        // ---- Shadow Atlas（聚光灯 + 后续点光源） ----
        ShadowAtlas m_ShadowAtlas;                      // Shadow Atlas 管理器

        // ---- 调试选项 ----
        bool m_DebugCSMVisualize = false;               // CSM 级联颜色可视化开关
    };
}
