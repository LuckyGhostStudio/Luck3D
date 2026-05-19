#pragma once

#include "Lucky/Renderer/RenderPass.h"
#include "Lucky/Renderer/Shader.h"
#include "Lucky/Renderer/Framebuffer.h"

namespace Lucky
{
    /// <summary>
    /// 调试可视化模式枚举
    /// 当前仅支持 CSM 级联可视化，未来可扩展（Normal / UV / Overdraw 等）
    /// </summary>
    enum class DebugVisualizeMode
    {
        None = 0,           // 关闭调试
        CSMCascades = 1,    // CSM 级联颜色叠加
        // 预留扩展：Normal、UV、Overdraw、LightingOnly...
    };

    /// <summary>
    /// 调试可视化 Pass：在 HDR 空间叠加调试信息（级联颜色等）
    /// 属于 "Debug" 分组，在 Main 之后、PostProcess 之前执行
    /// 用户层 Shader 完全不感知调试逻辑（横切关注点由管线注入）
    /// 
    /// 关闭时（Mode == None）Execute 立即返回，零开销
    /// UI 入口：SceneViewportPanel 工具栏（决策 D3）
    /// </summary>
    class DebugVisualizePass : public RenderPass
    {
    public:
        void Init() override;
        void Execute(const RenderContext& context) override;
        void Resize(uint32_t width, uint32_t height) override;

        const std::string& GetName() const override
        {
            static std::string name = "DebugVisualizePass";
            return name;
        }

        const std::string& GetGroup() const override
        {
            static std::string group = "Debug";
            return group;
        }

        /// <summary>
        /// 当前调试模式（外部读写）
        /// </summary>
        DebugVisualizeMode GetMode() const { return m_Mode; }
        void SetMode(DebugVisualizeMode mode) { m_Mode = mode; }

    private:
        /// <summary>
        /// 执行 CSM 级联可视化（输入 HDR_FBO，经 m_PingFBO 中转后写回 HDR_FBO）
        /// </summary>
        void ExecuteCSMCascades(const RenderContext& context);

        DebugVisualizeMode m_Mode = DebugVisualizeMode::None;   // 当前模式
        Ref<Shader> m_CSMVisualizeShader;                       // CSM 级联可视化 Shader
        Ref<Framebuffer> m_PingFBO;                             // 中转 FBO（避免同纹理读写）
    };
}
