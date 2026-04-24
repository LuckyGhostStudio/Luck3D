#pragma once

#include "Lucky/Renderer/RenderPass.h"

namespace Lucky
{
    /// <summary>
    /// 不透明物体渲染 Pass
    /// 按 SortKey 排序 + Shader 聚合绑定 + 批量绘制
    /// </summary>
    class OpaquePass : public RenderPass
    {
    public:
        void Init() override {}  // 无需初始化资源（复用外部主 FBO）
        void Execute(const RenderContext& context) override;
        const std::string& GetName() const override { static std::string name = "OpaquePass"; return name; }
        const std::string& GetGroup() const override { static std::string group = "Main"; return group; }
    };
}
