#pragma once

#include "RenderPass.h"
#include "RenderContext.h"

#include <vector>

namespace Lucky
{
    /// <summary>
    /// 渲染管线：管理和执行所有 RenderPass
    /// 按注册顺序依次执行每个 Pass
    /// </summary>
    class RenderPipeline
    {
    public:
        /// <summary>
        /// 初始化管线（初始化所有已注册的 Pass）
        /// </summary>
        void Init();
        
        /// <summary>
        /// 释放资源
        /// </summary>
        void Shutdown();
        
        /// <summary>
        /// 添加 RenderPass（按添加顺序执行）
        /// </summary>
        void AddPass(const Ref<RenderPass>& pass);
        
        /// <summary>
        /// 移除 RenderPass
        /// </summary>
        void RemovePass(const std::string& name);
        
        /// <summary>
        /// 获取指定类型的 Pass
        /// </summary>
        template<typename T>
        Ref<T> GetPass() const
        {
            for (const auto& pass : m_Passes)
            {
                auto casted = std::dynamic_pointer_cast<T>(pass);
                if (casted)
                {
                    return casted;
                }
            }
            return nullptr;
        }
        
        /// <summary>
        /// 执行所有已启用的 Pass
        /// </summary>
        /// <param name="context">渲染上下文</param>
        void Execute(const RenderContext& context);
        
        /// <summary>
        /// 执行指定分组中所有已启用的 Pass
        /// 按注册顺序依次执行属于该分组的 Pass，跳过未启用的 Pass
        /// </summary>
        /// <param name="group">分组名称（对应 RenderPass::GetGroup()）</param>
        /// <param name="context">渲染上下文</param>
        void ExecuteGroup(const std::string& group, const RenderContext& context);
        
        /// <summary>
        /// 调整大小（通知所有 Pass）
        /// </summary>
        void Resize(uint32_t width, uint32_t height);
        
    private:
        std::vector<Ref<RenderPass>> m_Passes;  // Pass 列表（按执行顺序排列）
    };
}
