#pragma once

#include "Lucky/Renderer/RenderPass.h"
#include "Lucky/Renderer/Shader.h"

namespace Lucky
{
    class SilhouettePass;  // 前向声明
    
    /// <summary>
    /// 描边合成 Pass：读取 Silhouette 纹理，进行边缘检测，将描边颜色 Alpha 混合叠加到主 FBO
    /// </summary>
    class OutlineCompositePass : public RenderPass
    {
    public:
        void Init() override;
        void Execute(const RenderContext& context) override;
        const std::string& GetName() const override { static std::string name = "OutlineCompositePass"; return name; }
        
        /// <summary>
        /// 设置 SilhouettePass 引用（用于获取 Silhouette 纹理）
        /// </summary>
        void SetSilhouettePass(const Ref<SilhouettePass>& silhouettePass) { m_SilhouettePass = silhouettePass; }
        
    private:
        Ref<Shader> m_OutlineCompositeShader;       // 描边合成 Shader（边缘检测 + 叠加）
        Ref<SilhouettePass> m_SilhouettePass;       // SilhouettePass 引用（获取输出纹理）
    };
}
