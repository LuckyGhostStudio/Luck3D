#include "lcpch.h"
#include "UniformBuffer.h"

#include <glad/glad.h>

namespace Lucky
{
    Ref<UniformBuffer> UniformBuffer::Create(uint32_t size, uint32_t binding)
    {
        return CreateRef<UniformBuffer>(size, binding);
    }

    UniformBuffer::UniformBuffer(uint32_t size, uint32_t binding)
    {
        glCreateBuffers(1, &m_RendererID);  // 创建缓冲区

        glNamedBufferData(m_RendererID, size, nullptr, GL_DYNAMIC_DRAW);    // 初始化缓冲区
        glBindBufferBase(GL_UNIFORM_BUFFER, binding, m_RendererID);         // 绑定 Uniform 缓冲区到 binding
    }

    UniformBuffer::~UniformBuffer()
    {
        glDeleteBuffers(1, &m_RendererID);
    }

    void UniformBuffer::SetData(const void* data, uint32_t size, uint32_t offset)
    {
        glNamedBufferSubData(m_RendererID, offset, size, data); // 更新缓冲区数据
    }
}