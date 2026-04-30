#pragma once

#include "Lucky/Core/Base.h"
#include "Framebuffer.h"
#include "Shader.h"

namespace Lucky
{
    /// <summary>
    /// 后处理效果执行空间
    /// </summary>
    enum class PostProcessSpace
    {
        HDR,    // 在 Tonemapping 之前执行（HDR 空间）
        LDR     // 在 Tonemapping 之后执行（LDR 空间）
    };

    /// <summary>
    /// 后处理效果基类 所有后处理效果都继承此类
    /// </summary>
    class PostProcessEffect
    {
    public:
        virtual ~PostProcessEffect() = default;

        /// <summary>
        /// 初始化效果（加载 Shader、创建 FBO 等）
        /// </summary>
        virtual void Init() = 0;

        /// <summary>
        /// 执行后处理效果
        /// </summary>
        /// <param name="sourceTexture">输入纹理 ID</param>
        /// <param name="destFBO">输出 FBO</param>
        /// <param name="width">视口宽度</param>
        /// <param name="height">视口高度</param>
        virtual void Execute(uint32_t sourceTexture, Ref<Framebuffer> destFBO, uint32_t width, uint32_t height) = 0;

        /// <summary>
        /// 调整大小（视口变化时调用）
        /// </summary>
        virtual void Resize(uint32_t width, uint32_t height) {}

        /// <summary>
        /// 获取效果名称
        /// </summary>
        virtual const std::string& GetName() const = 0;

        /// <summary>
        /// 获取效果执行空间
        /// </summary>
        virtual PostProcessSpace GetSpace() const = 0;

        bool Enabled = true;   // 是否启用
        int Order = 0;         // 执行顺序（越小越先执行）
    };
}