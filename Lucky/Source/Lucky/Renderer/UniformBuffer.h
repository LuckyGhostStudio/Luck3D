#pragma once

#include "Lucky/Core/Base.h"

namespace Lucky
{
    /// <summary>
    /// Uniform 缓冲区
    /// </summary>
    class UniformBuffer
    {
    public:
        /// <summary>
        /// 创建 Uniform 缓冲区
        /// </summary>
        /// <param name="size">大小（字节）</param>
        /// <param name="binding">绑定点索引</param>
        /// <returns></returns>
        static Ref<UniformBuffer> Create(uint32_t size, uint32_t binding);
        
        UniformBuffer(uint32_t size, uint32_t binding);

        ~UniformBuffer();

        /// <summary>
        /// 设置数据
        /// </summary>
        /// <param name="data">数据</param>
        /// <param name="size">大小（字节）</param>
        /// <param name="offset">偏移量</param>
        void SetData(const void* data, uint32_t size, uint32_t offset = 0);

        /// <summary>
        /// 将本 UBO 重新绑定到自己的 binding slot
        /// 多个 UBO 实例可能共用同一 binding slot（例如多个 SceneRenderer 各自的 CameraUBO 都用 binding=0），
        /// 谁调用 Bind 谁就把该 binding slot 指向自己。用户端每次 SetData 前调用一次即可
        /// </summary>
        void Bind() const;

        uint32_t GetRendererID() const { return m_RendererID; }
    private:
        uint32_t m_RendererID = 0;
        uint32_t m_Binding = 0;     // 绑定点索引
    };
}