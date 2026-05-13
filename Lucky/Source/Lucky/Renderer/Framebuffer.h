#pragma once

#include "Lucky/Core/Base.h"

namespace Lucky
{
    /// <summary>
    /// 帧缓冲区纹理格式
    /// </summary>
    enum class FramebufferTextureFormat
    {
        None = 0,

        RGBA8,                      // 颜色 RGBA
        RGBA16F,                    // HDR 浮点颜色（半精度浮点，用于 HDR 渲染）
        RED_INTEGER,                // 红色整型

        DEFPTH24STENCIL8,           // 深度模板（深度24位 + 模板8位，不可采样）
        DEPTH_COMPONENT,            // 纯深度纹理（24位深度，可采样，用于 Shadow Map）
        DEPTH_COMPONENT_ARRAY,      // 深度纹理数组（GL_TEXTURE_2D_ARRAY，用于 CSM）
        RGBA8_ARRAY,                // 颜色纹理数组（GL_TEXTURE_2D_ARRAY，RGBA8，用于 Translucent Shadow Map CSM）

        Depth = DEFPTH24STENCIL8    // 默认值
    };

    /// <summary>
    /// 帧缓冲区纹理规范
    /// </summary>
    struct FramebufferTextureSpecification
    {
        FramebufferTextureSpecification() = default;
        FramebufferTextureSpecification(FramebufferTextureFormat format) :TextureFormat(format) {}
        
        FramebufferTextureFormat TextureFormat = FramebufferTextureFormat::None;    // 纹理格式
    };

    /// <summary>
    /// 帧缓冲区附件规范
    /// </summary>
    struct FramebufferAttachmentSpecification
    {
        FramebufferAttachmentSpecification() = default;
        FramebufferAttachmentSpecification(std::initializer_list<FramebufferTextureSpecification> attachments)
            :Attachments(attachments) {}
        
        std::vector<FramebufferTextureSpecification> Attachments;   // 附件列表
    };

    /// <summary>
    /// 帧缓冲区规范
    /// </summary>
    struct FramebufferSpecification
    {
        uint32_t Width;     // 帧缓冲区宽
        uint32_t Height;    // 帧缓冲区高

        FramebufferAttachmentSpecification Attachments; // 帧缓冲区所有附件

        uint32_t Samples = 1;       // 采样数
        uint32_t Layers = 1;        // 纹理层数（Texture2DArray 使用，默认 1 = 普通 2D 纹理）

        bool SwapChainTarget = false;   // 是否要渲染到屏幕
    };

    /// <summary>
    /// 帧缓冲区
    /// </summary>
    class Framebuffer
    {
    public:
        /// <summary>
        /// 创建帧缓冲区
        /// </summary>
        /// <param name="spec">帧缓冲区规范</param>
        /// <returns></returns>
        static Ref<Framebuffer> Create(const FramebufferSpecification& spec);

        Framebuffer(const FramebufferSpecification& spec);

        ~Framebuffer();

        /// <summary>
        /// 调整大小：使无效
        /// </summary>
        void Invalidate();

        /// <summary>
        /// 绑定帧缓冲区
        /// </summary>
        void Bind() const;

        /// <summary>
        /// 解除绑定
        /// </summary>
        void Unbind() const;

        /// <summary>
        /// 重置帧缓冲区大小
        /// </summary>
        /// <param name="width">宽</param>
        /// <param name="height">高</param>
        void Resize(uint32_t width, uint32_t height);

        /// <summary>
        /// 读取像素
        /// </summary>
        /// <param name="attachmentIndex">颜色缓冲区 id</param>
        /// <param name="x">x坐标</param>
        /// <param name="y">y坐标</param>
        /// <returns>像素数据：输出到 attachmentIndex 颜色缓冲区的数据</returns>
        int GetPixel(uint32_t attachmentIndex, int x, int y);

        /// <summary>
        /// 清除帧缓冲区附件
        /// </summary>
        /// <param name="attachmentIndex">颜色缓冲区 id</param>
        /// <param name="value">清除值</param>
        void ClearAttachment(uint32_t attachmentIndex, int value);

        /// <summary>
        /// 返回颜色缓冲区 ID
        /// </summary>
        /// <param name="index">ID 列表索引</param>
        /// <returns></returns>
        uint32_t GetColorAttachmentRendererID(uint32_t index = 0) const
        {
            LF_CORE_ASSERT(index < m_ColorAttachments.size(), "index 越界！");

            return m_ColorAttachments[index];
        }

        /// <summary>
        /// 返回帧缓冲区规范
        /// </summary>
        /// <returns>帧缓冲区规范</returns>
        const FramebufferSpecification& GetSpecification() const { return m_Specification; }

        /// <summary>
        /// 返回深度缓冲区纹理 ID（用于 Shadow Map 采样）
        /// 仅当深度附件格式为 DEPTH_COMPONENT 时有意义
        /// </summary>
        uint32_t GetDepthAttachmentRendererID() const { return m_DepthAttachment; }

        // TODO 封装 Blit 方法 void BlitTo(const Ref<Framebuffer>& target, BufferType bufferType) const;
        
        /// <summary>
        /// 将 Texture2DArray 的指定层绑定为当前 FBO 的深度附件
        /// 用于 CSM 逐级联渲染：每次渲染一个级联前调用此方法切换目标层
        /// 仅当 Specification.Layers > 1 且深度格式为 DEPTH_COMPONENT_ARRAY 时有效
        /// </summary>
        /// <param name="layer">层索引（0 ~ Layers-1）</param>
        void BindDepthLayer(int layer);

        /// <summary>
        /// 将 Texture2DArray 的指定层绑定为当前 FBO 的颜色附件
        /// 用于逐级联渲染颜色数据（如 Translucent Shadow Map）
        /// 仅当颜色附件格式为 RGBA8_ARRAY 时有效
        /// </summary>
        /// <param name="attachmentIndex">颜色附件索引</param>
        /// <param name="layer">层索引（0 ~ Layers-1）</param>
        void BindColorLayer(uint32_t attachmentIndex, int layer);

        /// <summary>
        /// 获取深度 Texture2DArray 的纹理 ID
        /// 用于在 OpaquePass/TransparentPass 中绑定到纹理单元进行采样
        /// 仅当深度格式为 DEPTH_COMPONENT_ARRAY 时有意义
        /// </summary>
        uint32_t GetDepthArrayTextureID() const { return m_DepthAttachment; }

        /// <summary>
        /// 获取颜色 Texture2DArray 的纹理 ID
        /// 用于在 OpaquePass/TransparentPass 中绑定到纹理单元进行采样
        /// 仅当颜色格式为 RGBA8_ARRAY 时有意义
        /// </summary>
        /// <param name="index">颜色附件索引</param>
        uint32_t GetColorArrayTextureID(uint32_t index = 0) const { return GetColorAttachmentRendererID(index); }

        /// <summary>
        /// 将当前 FBO 的深度缓冲区 Blit 到目标 FBO
        /// 用于将 HDR FBO 的深度信息复制到主 FBO（使 Gizmo 能被场景物体正确遮挡）
        /// </summary>
        /// <param name="target">目标 FBO</param>
        void BlitDepthTo(const Ref<Framebuffer>& target) const;
        
        /// <summary>
        /// 将当前 FBO 的颜色缓冲区 Blit 到目标 FBO
        /// 用于调试：直接显示HDR内容而不做Tonemapping
        /// </summary>
        /// <param name="target">目标 FBO</param>
        void BlitColorTo(const Ref<Framebuffer>& target) const;
    private:
        uint32_t m_RendererID = 0;                  // 帧缓冲区 ID
        
        FramebufferSpecification m_Specification;   // 帧缓冲区规范

        std::vector<FramebufferTextureSpecification> m_ColorAttachmentSpecs;                    // 颜色缓冲区规范列表
        FramebufferTextureSpecification m_DepthAttachmentSpec = FramebufferTextureFormat::None; // 深度缓冲区规范

        std::vector<uint32_t> m_ColorAttachments;   // 颜色缓冲区 ID 列表
        uint32_t m_DepthAttachment = 0;             // 深度缓冲区 ID
    };
}