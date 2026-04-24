#pragma once

#include "Lucky/Renderer/RenderPass.h"
#include "Lucky/Renderer/Shader.h"

namespace Lucky
{
    /// <summary>
    /// 拾取 Pass：将每个物体的 Entity ID 写入 R32I 整数纹理
    /// 用于鼠标点击拾取物体
    /// 复用 OpaquePass 的深度缓冲区（GL_LEQUAL）
    /// </summary>
    class PickingPass : public RenderPass
    {
    public:
        void Init() override;
        void Execute(const RenderContext& context) override;
        const std::string& GetName() const override { static std::string name = "PickingPass"; return name; }
        
    private:
        Ref<Shader> m_EntityIDShader;   // EntityID 专用 Shader
    };
}
