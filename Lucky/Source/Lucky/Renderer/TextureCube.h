#pragma once

#include "Lucky/Core/Base.h"
#include "Texture.h"

#include <array>
#include <string>

namespace Lucky
{
    /// <summary>
    /// 立方体贴图纹理（Cubemap）
    /// 用于天空盒渲染和后续 IBL 环境光照
    /// 
    /// 支持两种加载方式：
    /// 1. 从 6 面独立图片文件加载（LDR 格式：.jpg/.png）
    /// 2. 从单张 HDR 全景图加载（.hdr 格式，自动转换为 Cubemap）
    /// </summary>
    class TextureCube : public Texture
    {
    public:
        /// <summary>
        /// 从 6 面图片文件创建 Cubemap
        /// 面顺序：+X(Right), -X(Left), +Y(Top), -Y(Bottom), +Z(Front), -Z(Back)
        /// </summary>
        /// <param name="facePaths">6 面图片文件路径数组</param>
        /// <returns>TextureCube 实例</returns>
        static Ref<TextureCube> Create(const std::array<std::string, 6>& facePaths);
        
        /// <summary>
        /// 从单张 HDR Equirectangular 全景图创建 Cubemap
        /// 内部执行 Equirectangular -> Cubemap 转换
        /// </summary>
        /// <param name="hdrPath">HDR 全景图文件路径（.hdr 格式）</param>
        /// <param name="resolution">Cubemap 每面分辨率（默认 1024x1024）</param>
        /// <returns>TextureCube 实例</returns>
        static Ref<TextureCube> CreateFromHDR(const std::string& hdrPath, uint32_t resolution = 1024);
        
        /// <summary>
        /// 创建空的 Cubemap（指定分辨率，用于运行时填充）
        /// </summary>
        /// <param name="resolution">每面分辨率</param>
        /// <returns>TextureCube 实例</returns>
        static Ref<TextureCube> Create(uint32_t resolution);
        
        /// <summary>
        /// 从 6 面图片文件构造
        /// </summary>
        TextureCube(const std::array<std::string, 6>& facePaths);
        
        /// <summary>
        /// 从 HDR 全景图构造
        /// </summary>
        TextureCube(const std::string& hdrPath, uint32_t resolution);
        
        /// <summary>
        /// 空 Cubemap 构造
        /// </summary>
        TextureCube(uint32_t resolution);
        
        ~TextureCube();
        
        // ---- Texture 接口实现 ----
        uint32_t GetWidth() const override { return m_Resolution; }
        uint32_t GetHeight() const override { return m_Resolution; }
        uint32_t GetRendererID() const override { return m_RendererID; }
        const std::string& GetPath() const override { return m_Path; }
        void SetData(void* data, uint32_t size) override;
        void Bind(uint32_t slot = 0) const override;
        
    private:
        uint32_t m_RendererID = 0;      // OpenGL 纹理 ID
        uint32_t m_Resolution = 0;      // 每面分辨率
        std::string m_Path;             // 路径（6 面时为第一张图片路径，HDR 时为 .hdr 路径）
        bool m_IsHDR = false;           // 是否为 HDR 格式
    };
}
