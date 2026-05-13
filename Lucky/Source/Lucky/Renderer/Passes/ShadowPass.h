#pragma once

#include "Lucky/Renderer/RenderPass.h"
#include "Lucky/Renderer/Shader.h"
#include "Lucky/Renderer/Framebuffer.h"

namespace Lucky
{
    /// <summary>
    /// 阴影 Pass：从光源视角渲染场景深度到 Shadow Map（CSM 版本）
    /// 使用 Texture2DArray 存储多级联深度纹理
    /// 所有级联额外生成 Translucent Shadow Map Array（透明物体颜色衰减）
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

    private:
        /// <summary>
        /// 重新创建 FBO（当分辨率或级联数量变化时调用）
        /// </summary>
        void RecreateFBOs();

        Ref<Framebuffer> m_CSMFramebuffer;              // CSM Texture2DArray FBO（多层深度纹理）
        Ref<Framebuffer> m_TranslucentShadowFBO;        // Translucent Shadow Map FBO（RGBA8 Array 颜色 + 深度 Array，所有级联）
        Ref<Shader> m_ShadowShader;                     // Shadow Pass Shader
        uint32_t m_ShadowMapResolution = 2048;          // Shadow Map 分辨率
        int m_CascadeCount = 4;                         // 当前级联数量

        // ---- 调试选项 ----
        bool m_DebugCSMVisualize = false;               // CSM 级联颜色可视化开关
    };
}
