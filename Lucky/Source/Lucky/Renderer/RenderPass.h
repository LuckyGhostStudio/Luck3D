#pragma once

#include "Lucky/Core/Base.h"

#include <string>

namespace Lucky
{
    // 前向声明
    struct RenderContext;
    
    /// <summary>
    /// 渲染 Pass 基类
    /// 每个 Pass 负责渲染管线中的一个阶段
    /// 不同 Pass 从 RenderContext 中获取各自需要的数据
    /// </summary>
    class RenderPass
    {
    public:
        virtual ~RenderPass() = default;
        
        /// <summary>
        /// 初始化 Pass（创建 FBO、加载 Shader 等）
        /// 在 RenderPipeline::Init() 中调用，仅调用一次
        /// </summary>
        virtual void Init() = 0;
        
        /// <summary>
        /// 执行 Pass（设置渲染状态 + 渲染物体 + 恢复状态）
        /// 每帧调用，通过 RenderContext 获取所需数据
        /// </summary>
        /// <param name="context">渲染上下文（包含相机、光源、DrawCommand 列表、Outline 数据等）</param>
        virtual void Execute(const RenderContext& context) = 0;
        
        /// <summary>
        /// 调整大小（FBO Resize 等）
        /// </summary>
        virtual void Resize(uint32_t width, uint32_t height) {}
        
        /// <summary>
        /// 获取 Pass 名称（用于调试和日志）
        /// </summary>
        virtual const std::string& GetName() const = 0;
        
        /// <summary>
        /// 是否启用此 Pass
        /// </summary>
        bool Enabled = true;
    };
}
