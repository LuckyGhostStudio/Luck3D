#include "lcpch.h"
#include "TextureCube.h"

#include <stb_image.h>
#include <glad/glad.h>
#include <glm/glm.hpp>
#include <glm/gtc/constants.hpp>

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
        SetTextureParameters();
    }
    
    // ======== 从 HDR 全景图构造（Equirectangular -> Cubemap 转换） ========
    
    TextureCube::TextureCube(const std::string& hdrPath, uint32_t resolution)
        : m_Path(hdrPath), m_Resolution(resolution), m_IsHDR(true)
    {
        // 1. 加载 HDR 全景图（浮点格式）
        stbi_set_flip_vertically_on_load(1);  // HDR 全景图需要垂直翻转（底部为纬度 -90°）
        int width, height, channels;
        float* hdrData = stbi_loadf(hdrPath.c_str(), &width, &height, &channels, 3);  // 强制 3 通道
        
        if (!hdrData)
        {
            LF_CORE_ERROR("TextureCube::CreateFromHDR() - Failed to load HDR file: {0}", hdrPath);
            // 创建空 Cubemap 作为 fallback
            glCreateTextures(GL_TEXTURE_CUBE_MAP, 1, &m_RendererID);
            glTextureStorage2D(m_RendererID, 1, GL_RGB16F, m_Resolution, m_Resolution);
            SetTextureParameters();
            return;
        }
        
        LF_CORE_INFO("TextureCube::CreateFromHDR() - Loading HDR: {0} ({1}x{2})", hdrPath, width, height);
        
        // 2. 创建 Cubemap 纹理（HDR 使用 GL_RGB16F 半精度浮点格式）
        glCreateTextures(GL_TEXTURE_CUBE_MAP, 1, &m_RendererID);
        glTextureStorage2D(m_RendererID, 1, GL_RGB16F, m_Resolution, m_Resolution);
        
        // 3. 对每个面执行 Equirectangular -> Cubemap 转换
        //    面顺序（OpenGL Cubemap 标准）：+X, -X, +Y, -Y, +Z, -Z
        std::vector<float> faceData(m_Resolution * m_Resolution * 3);
        
        for (int face = 0; face < 6; ++face)
        {
            for (uint32_t y = 0; y < m_Resolution; ++y)
            {
                for (uint32_t x = 0; x < m_Resolution; ++x)
                {
                    // 将像素坐标映射到 [-1, 1] 范围（像素中心采样）
                    float u = (2.0f * (x + 0.5f) / m_Resolution) - 1.0f;
                    float v = (2.0f * (y + 0.5f) / m_Resolution) - 1.0f;
                    
                    // 根据面索引计算 3D 方向向量
                    glm::vec3 dir;
                    switch (face)
                    {
                        case 0: dir = glm::vec3( 1.0f,   -v,   -u); break;  // +X (Right)
                        case 1: dir = glm::vec3(-1.0f,   -v,    u); break;  // -X (Left)
                        case 2: dir = glm::vec3(   u,  1.0f,    v); break;  // +Y (Top)
                        case 3: dir = glm::vec3(   u, -1.0f,   -v); break;  // -Y (Bottom)
                        case 4: dir = glm::vec3(   u,   -v,  1.0f); break;  // +Z (Front)
                        case 5: dir = glm::vec3(  -u,   -v, -1.0f); break;  // -Z (Back)
                    }
                    dir = glm::normalize(dir);
                    
                    // 方向向量 → 经纬度（Equirectangular 映射）
                    // atan2(z, x) → 经度 [-π, π]
                    // asin(y)     → 纬度 [-π/2, π/2]
                    float longitude = atan2f(dir.z, dir.x);
                    float latitude  = asinf(glm::clamp(dir.y, -1.0f, 1.0f));
                    
                    // 经纬度 → UV 坐标 [0, 1]
                    float sampleU = (longitude / (2.0f * glm::pi<float>())) + 0.5f;
                    float sampleV = (latitude  / glm::pi<float>()) + 0.5f;
                    
                    // 双线性插值采样 HDR 全景图
                    float fx = sampleU * (width - 1);
                    float fy = sampleV * (height - 1);
                    
                    int x0 = static_cast<int>(floorf(fx));
                    int y0 = static_cast<int>(floorf(fy));
                    int x1 = glm::min(x0 + 1, width - 1);
                    int y1 = glm::min(y0 + 1, height - 1);
                    x0 = glm::clamp(x0, 0, width - 1);
                    y0 = glm::clamp(y0, 0, height - 1);
                    
                    float tx = fx - floorf(fx);
                    float ty = fy - floorf(fy);
                    
                    int dstIndex = (y * m_Resolution + x) * 3;
                    
                    for (int c = 0; c < 3; ++c)
                    {
                        float c00 = hdrData[(y0 * width + x0) * 3 + c];
                        float c10 = hdrData[(y0 * width + x1) * 3 + c];
                        float c01 = hdrData[(y1 * width + x0) * 3 + c];
                        float c11 = hdrData[(y1 * width + x1) * 3 + c];
                        
                        // 双线性插值
                        faceData[dstIndex + c] = c00 * (1.0f - tx) * (1.0f - ty)
                                               + c10 * tx * (1.0f - ty)
                                               + c01 * (1.0f - tx) * ty
                                               + c11 * tx * ty;
                    }
                }
            }
            
            // 上传当前面数据到 Cubemap
            glTextureSubImage3D(m_RendererID, 0, 0, 0, face,
                               m_Resolution, m_Resolution, 1,
                               GL_RGB, GL_FLOAT, faceData.data());
        }
        
        // 4. 释放 HDR 原始数据
        stbi_image_free(hdrData);
        
        // 5. 设置纹理参数
        SetTextureParameters();
        
        LF_CORE_INFO("TextureCube::CreateFromHDR() - Cubemap created successfully ({0}x{0} per face)", m_Resolution);
    }
    
    // ======== 空 Cubemap 构造 ========
    
    TextureCube::TextureCube(uint32_t resolution)
        : m_Resolution(resolution)
    {
        glCreateTextures(GL_TEXTURE_CUBE_MAP, 1, &m_RendererID);
        glTextureStorage2D(m_RendererID, 1, GL_RGB16F, m_Resolution, m_Resolution);
        SetTextureParameters();
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
    
    // ======== 内部辅助方法 ========
    
    void TextureCube::SetTextureParameters()
    {
        glTextureParameteri(m_RendererID, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTextureParameteri(m_RendererID, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTextureParameteri(m_RendererID, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTextureParameteri(m_RendererID, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        glTextureParameteri(m_RendererID, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);
    }
}
