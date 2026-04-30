#pragma once

#include "Lucky/Renderer/RenderPass.h"

namespace Lucky
{
    /// <summary>
    /// 透明物体渲染 Pass
    /// 按 DistanceToCamera 从远到近排序 + 禁用深度写入 + 启用 Alpha 混合
    /// 在 OpaquePass 之后执行，确保透明物体被不透明物体正确遮挡
    /// 属于 "Main" 分组
    /// </summary>
    class TransparentPass : public RenderPass
    {
    public:
        void Init() override {}  // 无需初始化资源（复用 HDR FBO）
        void Execute(const RenderContext& context) override;
        const std::string& GetName() const override { static std::string name = "TransparentPass"; return name; }
        const std::string& GetGroup() const override { static std::string group = "Main"; return group; }
    };
}
