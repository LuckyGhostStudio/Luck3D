#pragma once

#include "Lucky/Core/Base.h"
#include "Shader.h"
#include "VertexArray.h"
#include "Buffer.h"
#include "TextureCube.h"

#include <glm/glm.hpp>

namespace Lucky
{
    /// <summary>
    /// IBL 预计算数据（持有 3 张预计算纹理的 ID）
    /// </summary>
    struct IBLData
    {
        uint32_t IrradianceMapID = 0;       // Irradiance Cubemap 纹理 ID
        uint32_t PrefilterMapID = 0;        // Prefiltered Environment Cubemap 纹理 ID
        uint32_t BRDFLUTID = 0;             // BRDF LUT Texture2D 纹理 ID
        uint32_t PrefilterMaxMipLevel = 4;  // Prefiltered Map 最大 Mip Level（maxMipLevels - 1）
        bool Valid = false;                 // 是否有效（已生成）
        
        // ---- 默认 fallback 纹理（IBL 关闭时绑定，避免 sampler 未定义行为） ----
        uint32_t DefaultBlackCubemapID = 0; // 1×1 黑色 Cubemap
        uint32_t DefaultBlack2DID = 0;      // 1×1 黑色 Texture2D
    };
    
    /// <summary>
    /// IBL 预计算管理器
    /// 负责生成 IBL 所需的 3 张预计算纹理：
    /// 1. BRDF LUT（全局一次性，与环境图无关）
    /// 2. Irradiance Map（依赖环境图，天空盒变更时重新生成）
    /// 3. Prefiltered Environment Map（依赖环境图，天空盒变更时重新生成）
    /// 
    /// 使用方式：
    ///   IBLPrecompute::Init();                          // 初始化（加载 Shader，生成 BRDF LUT）
    ///   IBLPrecompute::GenerateFromCubemap(cubemapID);  // 从环境 Cubemap 生成 IBL 数据
    ///   IBLData data = IBLPrecompute::GetIBLData();     // 获取 IBL 纹理 ID
    ///   IBLPrecompute::Shutdown();                      // 释放资源
    /// </summary>
    class IBLPrecompute
    {
    public:
        /// <summary>
        /// 初始化：加载 IBL 预计算 Shader，生成 BRDF LUT
        /// 在 Renderer3D::Init() 中调用
        /// </summary>
        static void Init();
        
        /// <summary>
        /// 释放所有 IBL 纹理和资源
        /// 在 Renderer3D::Shutdown() 中调用
        /// </summary>
        static void Shutdown();
        
        /// <summary>
        /// 从环境 Cubemap 生成 Irradiance Map 和 Prefiltered Map
        /// 天空盒变更时调用此方法重新生成
        /// </summary>
        /// <param name="envCubemapID">原始环境 Cubemap 的 OpenGL 纹理 ID</param>
        /// <param name="prefilterResolution">Prefilter Map 分辨率（每面，默认 128）</param>
        static void GenerateFromCubemap(uint32_t envCubemapID, int prefilterResolution = 128);
        
        /// <summary>
        /// 获取 IBL 预计算数据（3 张纹理 ID）
        /// </summary>
        static const IBLData& GetIBLData();
        
        /// <summary>
        /// IBL 数据是否有效（已生成）
        /// </summary>
        static bool IsValid();
        
    private:
        /// <summary>
        /// 生成默认 fallback 纹理（1×1 黑色 Cubemap + 1×1 黑色 Texture2D）
        /// IBL 关闭时绑定这些纹理，避免 sampler 未绑定导致的未定义行为
        /// </summary>
        static void GenerateDefaultFallbackTextures();
        /// <summary>
        /// 生成 BRDF LUT（512×512，RG16F）
        /// </summary>
        static uint32_t GenerateBRDFLUT();
        
        /// <summary>
        /// 生成 Irradiance Map（32×32 per face，RGB16F Cubemap）
        /// </summary>
        static uint32_t GenerateIrradianceMap(uint32_t envCubemap);
        
        /// <summary>
        /// 生成 Prefiltered Environment Map（RGB16F Cubemap，Mip Levels 根据分辨率动态计算）
        /// </summary>
        /// <param name="envCubemap">原始环境 Cubemap 的 OpenGL 纹理 ID</param>
        /// <param name="resolution">Prefilter Map 分辨率（每面）</param>
        static uint32_t GeneratePrefilterMap(uint32_t envCubemap, int resolution);
    };
}
