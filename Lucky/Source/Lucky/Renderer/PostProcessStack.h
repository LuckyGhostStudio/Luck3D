#pragma once

#include "PostProcessEffect.h"

namespace Lucky
{
    /// <summary>
    /// 后处理栈：管理和执行所有后处理效果
    /// 由 PostProcessPass 持有，负责效果链的管理和 Ping-Pong FBO 的执行
    /// </summary>
    class PostProcessStack
    {
    public:
        /// <summary>
        /// 初始化后处理栈（创建 Ping-Pong FBO）
        /// </summary>
        /// <param name="width">视口宽度</param>
        /// <param name="height">视口高度</param>
        void Init(uint32_t width, uint32_t height);

        /// <summary>
        /// 释放资源
        /// </summary>
        void Shutdown();

        /// <summary>
        /// 添加后处理效果
        /// </summary>
        /// <param name="effect">后处理效果</param>
        void AddEffect(const Ref<PostProcessEffect>& effect);

        /// <summary>
        /// 移除后处理效果
        /// </summary>
        /// <param name="name">效果名称</param>
        void RemoveEffect(const std::string& name);

        /// <summary>
        /// 获取指定类型的后处理效果
        /// </summary>
        template<typename T>
        Ref<T> GetEffect() const
        {
            for (const auto& effect : m_Effects)
            {
                auto casted = std::dynamic_pointer_cast<T>(effect);
                if (casted)
                {
                    return casted;
                }
            }
            return nullptr;
        }

        /// <summary>
        /// 执行指定空间的所有启用效果
        /// </summary>
        /// <param name="sourceTexture">输入纹理 ID</param>
        /// <param name="space">执行空间（HDR 或 LDR）</param>
        /// <param name="width">视口宽度</param>
        /// <param name="height">视口高度</param>
        /// <returns>最终输出纹理 ID</returns>
        uint32_t Execute(uint32_t sourceTexture, PostProcessSpace space, uint32_t width, uint32_t height);

        /// <summary>
        /// 调整大小
        /// </summary>
        /// <param name="width">视口宽度</param>
        /// <param name="height">视口高度</param>
        void Resize(uint32_t width, uint32_t height);

        /// <summary>
        /// 获取所有效果列表
        /// </summary>
        const std::vector<Ref<PostProcessEffect>>& GetEffects() const { return m_Effects; }

        /// <summary>
        /// 检查指定空间是否有启用的效果
        /// </summary>
        bool HasEnabledEffects(PostProcessSpace space) const
        {
            for (const auto& effect : m_Effects)
            {
                if (effect->Enabled && effect->GetSpace() == space)
                {
                    return true;
                }
            }
            return false;
        }

    private:
        std::vector<Ref<PostProcessEffect>> m_Effects;
        Ref<Framebuffer> m_PingFBO;     // Ping-Pong FBO A（RGBA16F）
        Ref<Framebuffer> m_PongFBO;     // Ping-Pong FBO B（RGBA16F）
    };
}