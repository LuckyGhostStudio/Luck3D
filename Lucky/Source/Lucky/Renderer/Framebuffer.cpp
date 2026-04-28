#include "lcpch.h"
#include "Framebuffer.h"

#include <glad/glad.h>

namespace Lucky
{
    static uint32_t s_MaxFramebufferSize = 8192;    // 帧缓冲区最大大小

    namespace Utils
    {
        /// <summary>
        /// 纹理采样目标
        /// </summary>
        /// <param name="multisampled">是否多重采样</param>
        /// <returns>纹理类型</returns>
        static GLenum TextureTarget(bool multisampled)
        {
            return multisampled ? GL_TEXTURE_2D_MULTISAMPLE : GL_TEXTURE_2D;    // Texture2D多重采样 Texture2D
        }

        /// <summary>
        /// 创建缓冲区纹理
        /// </summary>
        /// <param name="multisampled">是否多重采样</param>
        /// <param name="outID">输出纹理ID</param>
        /// <param name="count">纹理数量</param>
        static void CreateTextures(bool multisampled, uint32_t* outID, uint32_t count)
        {
            glCreateTextures(TextureTarget(multisampled), count, outID);    // 创建纹理
        }

        /// <summary>
        /// 绑定纹理
        /// </summary>
        /// <param name="multisampled">是否多重采样</param>
        /// <param name="id">纹理id</param>
        static void BindTexture(bool multisampled, uint32_t id)
        {
            glBindTexture(TextureTarget(multisampled), id); // 绑定纹理
        }

        /// <summary>
        /// 附加颜色纹理
        /// </summary>
        /// <param name="id">颜色缓冲区id</param>
        /// <param name="samples">采样数量</param>
        /// <param name="internalFormat">内部格式</param>
        /// <param name="format">外部格式</param>
        /// <param name="width">宽</param>
        /// <param name="height">高</param>
        /// <param name="index">颜色缓冲区id索引</param>
        static void AttachColorTexture(uint32_t id, int samples, GLenum internalFormat, GLenum format, uint32_t width, uint32_t height, int index)
        {
            bool multisampled = samples > 1;    //是否是多重采样

            if (multisampled)
            {
                glTexImage2DMultisample(GL_TEXTURE_2D_MULTISAMPLE, samples, internalFormat, width, height, GL_FALSE);   // 多重采样纹理图像
            }
            else
            {
                // 纹理图像：- - 内部格式（存储格式）- - - 外部格式（访问格式）
                glTexImage2D(GL_TEXTURE_2D, 0, internalFormat, width, height, 0, format, GL_UNSIGNED_BYTE, nullptr);
                // 设置纹理参数
                glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);       // 缩小过滤器 线性插值
                glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);       // 放大过滤器 线性插值
                glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);
                glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
                glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
            }

            // 颜色纹理附加到帧缓冲区
            glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0 + index, TextureTarget(multisampled), id, 0);
        }

        /// <summary>
        /// 添加深度纹理
        /// </summary>
        /// <param name="id">深度缓冲区id</param>
        /// <param name="samples">采样数量</param>
        /// <param name="format">格式</param>
        /// <param name="attachmentType">附件类型</param>
        /// <param name="width">宽</param>
        /// <param name="height">高</param>
        static void AttachDepthTexture(uint32_t id, int samples, GLenum format, GLenum attachmentType, uint32_t width, uint32_t height)
        {
            bool multisampled = samples > 1;    //是否是多重采样

            if (multisampled)
            {
                glTexImage2DMultisample(GL_TEXTURE_2D_MULTISAMPLE, samples, format, width, height, GL_FALSE);   // 多重采样纹理图像
            }
            else
            {
                // 深度缓冲区纹理存储 24位深度缓冲区 8位模板缓冲区
                glTexStorage2D(GL_TEXTURE_2D, 1, format, width, height);
                // 设置纹理参数
                glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);    // 缩小过滤器 线性插值
                glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);    // 放大过滤器 线性插值
                glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);
                glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
                glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
            }

            // 深度纹理附加到帧缓冲区
            glFramebufferTexture2D(GL_FRAMEBUFFER, attachmentType, TextureTarget(multisampled), id, 0);
        }

        /// <summary>
        /// 缓冲区纹理是否是深度格式
        /// </summary>
        /// <param name="format">格式</param>
        /// <returns></returns>
        static bool IsDepthFormat(FramebufferTextureFormat format)
        {
            switch (format)
            {
                case FramebufferTextureFormat::DEFPTH24STENCIL8: return true;
                case FramebufferTextureFormat::DEPTH_COMPONENT:  return true;
            }

            return false;
        }

        /// <summary>
        /// 帧缓冲区纹理格式转 GL
        /// </summary>
        /// <param name="format">帧缓冲区纹理格式</param>
        /// <returns>GL 纹理格式</returns>
        static GLenum FramebufferTextureFormatToGL(FramebufferTextureFormat format)
        {
            switch (format)
            {
                case FramebufferTextureFormat::RGBA8:       return GL_RGBA8;
                case FramebufferTextureFormat::RED_INTEGER: return GL_RED_INTEGER;
            }

            LF_CORE_ASSERT(false, "No format");

            return 0;
        }
    }

    Ref<Framebuffer> Framebuffer::Create(const FramebufferSpecification& spec)
    {
        return CreateRef<Framebuffer>(spec);
    }

    Framebuffer::Framebuffer(const FramebufferSpecification& spec)
        : m_Specification(spec)
    {
        // 遍历帧缓冲区规范附件列表
        for (auto format : spec.Attachments.Attachments)
        {
            // 不是深度格式
            if (!Utils::IsDepthFormat(format.TextureFormat))
            {
                m_ColorAttachmentSpecs.emplace_back(format);    // 添加到颜色缓冲区规范列表
            }
            else
            {
                m_DepthAttachmentSpec = format; // 深度缓冲区规范格式
            }
        }

        Invalidate();
    }

    Framebuffer::~Framebuffer()
    {
        glDeleteFramebuffers(1, &m_RendererID);     // 删除帧缓冲区
        glDeleteTextures(m_ColorAttachments.size(), m_ColorAttachments.data()); // 删除颜色缓冲区
        glDeleteTextures(1, &m_DepthAttachment);    // 删除深度缓冲区 
    }

    void Framebuffer::Invalidate()
    {
        // 帧缓冲区存在
        if (m_RendererID)
        {
            glDeleteFramebuffers(1, &m_RendererID);     // 删除帧缓冲区
            glDeleteTextures(m_ColorAttachments.size(), m_ColorAttachments.data()); // 删除颜色缓冲区
            glDeleteTextures(1, &m_DepthAttachment);    // 删除深度缓冲区

            m_ColorAttachments.clear(); // 清空颜色缓冲区 id 列表
            m_DepthAttachment = 0;      // 清零深度缓冲区 id
        }

        glCreateFramebuffers(1, &m_RendererID);             // 创建帧缓冲区
        glBindFramebuffer(GL_FRAMEBUFFER, m_RendererID);    // 绑定帧缓冲区

        bool multisampled = m_Specification.Samples > 1;    // 是否是多重采样

        // 颜色缓冲区附件存在
        if (m_ColorAttachmentSpecs.size())
        {
            m_ColorAttachments.resize(m_ColorAttachmentSpecs.size());                                   // 设置颜色缓冲区 ID 列表大小
            Utils::CreateTextures(multisampled, m_ColorAttachments.data(), m_ColorAttachments.size());  // 创建颜色缓冲区纹理

            // 遍历颜色缓冲区 ID 列表
            for (size_t i = 0; i < m_ColorAttachments.size(); i++)
            {
                Utils::BindTexture(multisampled, m_ColorAttachments[i]);    // 绑定颜色缓冲区纹理
                
                // 附加颜色纹理
                switch (m_ColorAttachmentSpecs[i].TextureFormat)
                {
                    // RGBA8
                    case FramebufferTextureFormat::RGBA8:
                        Utils::AttachColorTexture(m_ColorAttachments[i], m_Specification.Samples, GL_RGBA8, GL_RGBA, m_Specification.Width, m_Specification.Height, i);
                        break;
                    // 红色整型
                    case FramebufferTextureFormat::RED_INTEGER:
                        Utils::AttachColorTexture(m_ColorAttachments[i], m_Specification.Samples, GL_R32I, GL_RED_INTEGER, m_Specification.Width, m_Specification.Height, i);
                        break;
                }
            }
        }

        // 深度缓冲区附件存在
        if (m_DepthAttachmentSpec.TextureFormat != FramebufferTextureFormat::None)
        {
            Utils::CreateTextures(multisampled, &m_DepthAttachment, 1); // 创建深度缓冲区纹理
            Utils::BindTexture(multisampled, m_DepthAttachment);        // 绑定深度缓冲区纹理

            switch (m_DepthAttachmentSpec.TextureFormat)
            {
                case FramebufferTextureFormat::DEFPTH24STENCIL8:
                    Utils::AttachDepthTexture(m_DepthAttachment, m_Specification.Samples, GL_DEPTH24_STENCIL8, GL_DEPTH_STENCIL_ATTACHMENT, m_Specification.Width, m_Specification.Height);
                    break;
                case FramebufferTextureFormat::DEPTH_COMPONENT:
                {
                    // 纯深度纹理（用于 Shadow Map，可在 Fragment Shader 中采样）
                    glTexImage2D(GL_TEXTURE_2D, 0, GL_DEPTH_COMPONENT24, m_Specification.Width, m_Specification.Height, 0, GL_DEPTH_COMPONENT, GL_FLOAT, nullptr);
                    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
                    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
                    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_BORDER);
                    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_BORDER);
                    // 边界颜色为白色（深度 1.0），超出 Shadow Map 范围的区域不在阴影中
                    float borderColor[] = { 1.0f, 1.0f, 1.0f, 1.0f };
                    glTexParameterfv(GL_TEXTURE_2D, GL_TEXTURE_BORDER_COLOR, borderColor);
                    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_2D, m_DepthAttachment, 0);
                    break;
                }
            }
        }

        if (m_ColorAttachments.size() > 1)
        {
            LF_CORE_ASSERT(m_ColorAttachments.size() <= 4, "m_ColorAttachments.size() > 4");    // 颜色缓冲区不超过 4

            GLenum buffers[4] = { GL_COLOR_ATTACHMENT0, GL_COLOR_ATTACHMENT1, GL_COLOR_ATTACHMENT2, GL_COLOR_ATTACHMENT3 };
            glDrawBuffers(m_ColorAttachments.size(), buffers);  // 绘制缓冲区
        }
        // 颜色缓冲区空
        else if (m_ColorAttachments.empty())
        {
            glDrawBuffer(GL_NONE);  // Depth 不绘制
        }

        // 检查帧缓冲区状态
        LF_CORE_ASSERT(glCheckFramebufferStatus(GL_FRAMEBUFFER) == GL_FRAMEBUFFER_COMPLETE, "Framebuffer is incomplete！");

        glBindFramebuffer(GL_FRAMEBUFFER, 0);   // 解除绑定
    }

    void Framebuffer::Bind() const
    {
        glBindFramebuffer(GL_FRAMEBUFFER, m_RendererID);    // 绑定帧缓冲区

        glViewport(0, 0, m_Specification.Width, m_Specification.Height);    // 设置视口大小
    }

    void Framebuffer::Unbind() const
    {
        glBindFramebuffer(GL_FRAMEBUFFER, 0);   // 解除绑定
    }

    void Framebuffer::Resize(uint32_t width, uint32_t height)
    {
        if (width == 0 || height == 0 || width > s_MaxFramebufferSize || height > s_MaxFramebufferSize)
        {
            LF_CORE_WARN("Attempted to resize framebuffer to {0}, {1}", width, height);
            return;
        }

        m_Specification.Width = width;
        m_Specification.Height = height;

        Invalidate();
    }

    int Framebuffer::GetPixel(uint32_t attachmentIndex, int x, int y)
    {
        LF_CORE_ASSERT(attachmentIndex < m_ColorAttachments.size(), "index 越界！");

        glReadBuffer(GL_COLOR_ATTACHMENT0 + attachmentIndex);   // 读第 attachmentIndex 个颜色缓冲区

        int pixelData;
        glReadPixels(x, y, 1, 1, GL_RED_INTEGER, GL_INT, &pixelData);   // 读 x,y 位置的像素 返回 int 类型像素数据

        return pixelData;   // 输出到 attachmentIndex 颜色缓冲区的数据
    }

    void Framebuffer::ClearAttachment(uint32_t attachmentIndex, int value)
    {
        LF_CORE_ASSERT(attachmentIndex < m_ColorAttachments.size(), "index 越界！");

        auto& spec = m_ColorAttachmentSpecs[attachmentIndex];   // attachmentIndex 号颜色缓冲区规范

        // 清除 attachmentIndex 号颜色缓冲区的值为 value
        glClearTexImage(
            m_ColorAttachments[attachmentIndex],
            0,
            Utils::FramebufferTextureFormatToGL(spec.TextureFormat),
            GL_INT,
            &(value)
        );
    }
}