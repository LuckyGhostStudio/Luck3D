#include "lcpch.h"
#include "TextureCube.h"

#include <stb_image.h>
#include <glad/glad.h>

namespace Lucky
{
    // ======== 静态工厂方法 ========
    
    Ref<TextureCube> TextureCube::Create(const std::array<std::string, 6>& facePaths)
    {
        return CreateRef<TextureCube>(facePaths);
    }
    
    Ref<TextureCube> TextureCube::CreateFromHDR(const std::string& hdrPath, uint32_t resolution)
    {
        return CreateRef<TextureCube>(hdrPath, resolution);
    }
    
    Ref<TextureCube> TextureCube::Create(uint32_t resolution)
    {
        return CreateRef<TextureCube>(resolution);
    }
    
    // ======== 从 6 面图片文件构造 ========
    
    TextureCube::TextureCube(const std::array<std::string, 6>& facePaths)
    {
        m_Path = facePaths[0];  // 使用第一张图片路径作为标识
        
        glCreateTextures(GL_TEXTURE_CUBE_MAP, 1, &m_RendererID);
        
        // 加载第一张图片获取分辨率
        int width, height, channels;
        stbi_set_flip_vertically_on_load(0);  // Cubemap 不需要垂直翻转
        
        stbi_uc* data = stbi_load(facePaths[0].c_str(), &width, &height, &channels, 0);
        LF_CORE_ASSERT(data, "Failed to load cubemap face: {0}", facePaths[0]);
        
        m_Resolution = static_cast<uint32_t>(width);
        
        // 确定格式
        GLenum internalFormat = (channels == 4) ? GL_RGBA8 : GL_RGB8;
        GLenum dataFormat = (channels == 4) ? GL_RGBA : GL_RGB;
        
        // 分配存储
        glTextureStorage2D(m_RendererID, 1, internalFormat, m_Resolution, m_Resolution);
        
        // 上传第一面（+X）
        glTextureSubImage3D(m_RendererID, 0, 0, 0, 0, m_Resolution, m_Resolution, 1, dataFormat, GL_UNSIGNED_BYTE, data);
        stbi_image_free(data);
        
        // 加载并上传剩余 5 面
        for (int i = 1; i < 6; ++i)
        {
            data = stbi_load(facePaths[i].c_str(), &width, &height, &channels, 0);
            LF_CORE_ASSERT(data, "Failed to load cubemap face: {0}", facePaths[i]);
            
            glTextureSubImage3D(m_RendererID, 0, 0, 0, i, m_Resolution, m_Resolution, 1, dataFormat, GL_UNSIGNED_BYTE, data);
            stbi_image_free(data);
        }
        
        // 设置纹理参数
        glTextureParameteri(m_RendererID, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTextureParameteri(m_RendererID, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTextureParameteri(m_RendererID, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTextureParameteri(m_RendererID, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        glTextureParameteri(m_RendererID, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);
    }
    
    // ======== 从 HDR 全景图构造 ========
    
    TextureCube::TextureCube(const std::string& hdrPath, uint32_t resolution)
        : m_Path(hdrPath), m_Resolution(resolution), m_IsHDR(true)
    {
        // TODO Phase 2：实现 Equirectangular -> Cubemap 转换
        // 当前阶段先创建空 Cubemap，后续实现 HDR 转换
        
        glCreateTextures(GL_TEXTURE_CUBE_MAP, 1, &m_RendererID);
        glTextureStorage2D(m_RendererID, 1, GL_RGB16F, m_Resolution, m_Resolution);
        
        glTextureParameteri(m_RendererID, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTextureParameteri(m_RendererID, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTextureParameteri(m_RendererID, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTextureParameteri(m_RendererID, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        glTextureParameteri(m_RendererID, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);
        
        LF_CORE_WARN("TextureCube::CreateFromHDR() - HDR conversion not yet implemented, created empty cubemap");
    }
    
    // ======== 空 Cubemap 构造 ========
    
    TextureCube::TextureCube(uint32_t resolution)
        : m_Resolution(resolution)
    {
        glCreateTextures(GL_TEXTURE_CUBE_MAP, 1, &m_RendererID);
        glTextureStorage2D(m_RendererID, 1, GL_RGB16F, m_Resolution, m_Resolution);
        
        glTextureParameteri(m_RendererID, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTextureParameteri(m_RendererID, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTextureParameteri(m_RendererID, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTextureParameteri(m_RendererID, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        glTextureParameteri(m_RendererID, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);
    }
    
    // ======== 析构 ========
    
    TextureCube::~TextureCube()
    {
        glDeleteTextures(1, &m_RendererID);
    }
    
    // ======== 接口实现 ========
    
    void TextureCube::SetData(void* data, uint32_t size)
    {
        // Cubemap 不支持通过 SetData 设置（使用构造函数加载）
        LF_CORE_ASSERT(false, "TextureCube::SetData() is not supported. Use Create() factory methods.");
    }
    
    void TextureCube::Bind(uint32_t slot) const
    {
        glBindTextureUnit(slot, m_RendererID);
    }
}
