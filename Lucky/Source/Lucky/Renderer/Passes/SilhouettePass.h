#pragma once

#include "Lucky/Renderer/RenderPass.h"
#include "Lucky/Renderer/Shader.h"
#include "Lucky/Renderer/Framebuffer.h"

namespace Lucky
{
    /// <summary>
    /// 轮廓掩码 Pass：将选中物体渲染为纯白色到独立的 Silhouette FBO
    /// 输出纹理供 OutlineCompositePass 进行边缘检测
    /// </summary>
    class SilhouettePass : public RenderPass
    {
    public:
        void Init() override;
        void Execute(const RenderContext& context) override;
        void Resize(uint32_t width, uint32_t height) override;
        const std::string& GetName() const override { static std::string name = "SilhouettePass"; return name; }
        
        /// <summary>
        /// 获取 Silhouette FBO 的颜色附件纹理 ID（供 OutlineCompositePass 使用）
        /// </summary>
        uint32_t GetSilhouetteTextureID() const;
        
    private:
        Ref<Framebuffer> m_SilhouetteFBO;       // Silhouette FBO（RGBA8，白色 = 选中，黑色 = 未选中）
        Ref<Shader> m_SilhouetteShader;         // Silhouette Shader（纯白色输出）
    };
}
