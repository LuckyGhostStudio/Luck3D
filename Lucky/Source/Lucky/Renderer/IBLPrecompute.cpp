#include "lcpch.h"
#include "IBLPrecompute.h"

#include "Renderer3D.h"
#include "RenderCommand.h"
#include "ScreenQuad.h"

#include <glad/glad.h>
#include <glm/ext/matrix_transform.hpp>
#include <glm/ext/matrix_clip_space.hpp>

namespace Lucky
{
    /// <summary>
    /// IBL 预计算内部数据
    /// </summary>
    struct IBLPrecomputeData
    {
        // ---- Shader ----
        Ref<Shader> BRDFShader;             // BRDF LUT 生成 Shader
        Ref<Shader> IrradianceShader;       // Irradiance 卷积 Shader
        Ref<Shader> PrefilterShader;        // Prefilter 卷积 Shader
        
        // ---- Cube 几何体（渲染到 Cubemap 使用） ----
        Ref<VertexArray> CubeVAO;
        Ref<VertexBuffer> CubeVBO;
        
        // ---- 6 面 View 矩阵和 Projection 矩阵 ----
        glm::mat4 CaptureProjection;
        glm::mat4 CaptureViews[6];
        
        // ---- IBL 数据 ----
        IBLData Data;
        
        // ---- 保存/恢复渲染状态 ----
        GLint PreviousViewport[4] = { 0 };
        GLint PreviousFBO = 0;
    };
    
    static IBLPrecomputeData s_IBLData;
    
    // Cube 顶点数据（与 SkyboxPass 相同）
    static const float s_CubeVertices[] = {
        // +X face
         1.0f, -1.0f, -1.0f,
         1.0f, -1.0f,  1.0f,
         1.0f,  1.0f,  1.0f,
         1.0f,  1.0f,  1.0f,
         1.0f,  1.0f, -1.0f,
         1.0f, -1.0f, -1.0f,
        // -X face
        -1.0f, -1.0f,  1.0f,
        -1.0f, -1.0f, -1.0f,
        -1.0f,  1.0f, -1.0f,
        -1.0f,  1.0f, -1.0f,
        -1.0f,  1.0f,  1.0f,
        -1.0f, -1.0f,  1.0f,
        // +Y face
        -1.0f,  1.0f, -1.0f,
         1.0f,  1.0f, -1.0f,
         1.0f,  1.0f,  1.0f,
         1.0f,  1.0f,  1.0f,
        -1.0f,  1.0f,  1.0f,
        -1.0f,  1.0f, -1.0f,
        // -Y face
        -1.0f, -1.0f,  1.0f,
         1.0f, -1.0f,  1.0f,
         1.0f, -1.0f, -1.0f,
         1.0f, -1.0f, -1.0f,
        -1.0f, -1.0f, -1.0f,
        -1.0f, -1.0f,  1.0f,
        // +Z face
        -1.0f, -1.0f,  1.0f,
        -1.0f,  1.0f,  1.0f,
         1.0f,  1.0f,  1.0f,
         1.0f,  1.0f,  1.0f,
         1.0f, -1.0f,  1.0f,
        -1.0f, -1.0f,  1.0f,
        // -Z face
         1.0f, -1.0f, -1.0f,
         1.0f,  1.0f, -1.0f,
        -1.0f,  1.0f, -1.0f,
        -1.0f,  1.0f, -1.0f,
        -1.0f, -1.0f, -1.0f,
         1.0f, -1.0f, -1.0f
    };
    
    /// <summary>
    /// 保存当前渲染状态（在 IBL 预计算前调用）
    /// </summary>
    static void SaveRenderState()
    {
        glGetIntegerv(GL_VIEWPORT, s_IBLData.PreviousViewport);
        glGetIntegerv(GL_FRAMEBUFFER_BINDING, &s_IBLData.PreviousFBO);
    }
    
    /// <summary>
    /// 恢复渲染状态（在 IBL 预计算后调用）
    /// </summary>
    static void RestoreRenderState()
    {
        glBindFramebuffer(GL_FRAMEBUFFER, s_IBLData.PreviousFBO);
        glViewport(
            s_IBLData.PreviousViewport[0],
            s_IBLData.PreviousViewport[1],
            s_IBLData.PreviousViewport[2],
            s_IBLData.PreviousViewport[3]
        );
    }
    
    void IBLPrecompute::Init()
    {
        // ---- 获取 IBL Shader（已在 Renderer3D::Init() 中加载） ----
        auto& shaderLib = Renderer3D::GetShaderLibrary();
        s_IBLData.BRDFShader = shaderLib->Get("BRDFIntegration");
        s_IBLData.IrradianceShader = shaderLib->Get("IrradianceConvolution");
        s_IBLData.PrefilterShader = shaderLib->Get("PrefilterConvolution");
        
        // ---- 创建 Cube VAO（用于渲染到 Cubemap） ----
        s_IBLData.CubeVAO = VertexArray::Create();
        s_IBLData.CubeVBO = VertexBuffer::Create((float*)s_CubeVertices, sizeof(s_CubeVertices));
        s_IBLData.CubeVBO->SetLayout({
            { ShaderDataType::Float3, "a_Position" }
        });
        s_IBLData.CubeVAO->AddVertexBuffer(s_IBLData.CubeVBO);
        
        // ---- 初始化 6 面 View 矩阵和 Projection 矩阵 ----
        // 90° FOV 的透视投影（Cubemap 每面恰好覆盖 90°）
        s_IBLData.CaptureProjection = glm::perspective(glm::radians(90.0f), 1.0f, 0.1f, 10.0f);
        
        // 6 面 View 矩阵（从原点看向 6 个方向）
        s_IBLData.CaptureViews[0] = glm::lookAt(glm::vec3(0), glm::vec3( 1,  0,  0), glm::vec3(0, -1,  0));  // +X
        s_IBLData.CaptureViews[1] = glm::lookAt(glm::vec3(0), glm::vec3(-1,  0,  0), glm::vec3(0, -1,  0));  // -X
        s_IBLData.CaptureViews[2] = glm::lookAt(glm::vec3(0), glm::vec3( 0,  1,  0), glm::vec3(0,  0,  1));  // +Y
        s_IBLData.CaptureViews[3] = glm::lookAt(glm::vec3(0), glm::vec3( 0, -1,  0), glm::vec3(0,  0, -1));  // -Y
        s_IBLData.CaptureViews[4] = glm::lookAt(glm::vec3(0), glm::vec3( 0,  0,  1), glm::vec3(0, -1,  0));  // +Z
        s_IBLData.CaptureViews[5] = glm::lookAt(glm::vec3(0), glm::vec3( 0,  0, -1), glm::vec3(0, -1,  0));  // -Z
        
        // ---- 生成默认 fallback 纹理（IBL 关闭时绑定，避免 sampler 未定义行为） ----
        GenerateDefaultFallbackTextures();
        
        // ---- 生成 BRDF LUT（全局一次性，与环境图无关） ----
        s_IBLData.Data.BRDFLUTID = GenerateBRDFLUT();
        
        LF_CORE_INFO("IBLPrecompute::Init() - BRDF LUT generated (512x512, RG16F)");
    }
    
    void IBLPrecompute::Shutdown()
    {
        // 释放 IBL 纹理
        if (s_IBLData.Data.BRDFLUTID != 0)
        {
            glDeleteTextures(1, &s_IBLData.Data.BRDFLUTID);
            s_IBLData.Data.BRDFLUTID = 0;
        }
        if (s_IBLData.Data.IrradianceMapID != 0)
        {
            glDeleteTextures(1, &s_IBLData.Data.IrradianceMapID);
            s_IBLData.Data.IrradianceMapID = 0;
        }
        if (s_IBLData.Data.PrefilterMapID != 0)
        {
            glDeleteTextures(1, &s_IBLData.Data.PrefilterMapID);
            s_IBLData.Data.PrefilterMapID = 0;
        }
        
        s_IBLData.Data.Valid = false;
        
        // 释放默认 fallback 纹理
        if (s_IBLData.Data.DefaultBlackCubemapID != 0)
        {
            glDeleteTextures(1, &s_IBLData.Data.DefaultBlackCubemapID);
            s_IBLData.Data.DefaultBlackCubemapID = 0;
        }
        if (s_IBLData.Data.DefaultBlack2DID != 0)
        {
            glDeleteTextures(1, &s_IBLData.Data.DefaultBlack2DID);
            s_IBLData.Data.DefaultBlack2DID = 0;
        }
        
        // 释放 Cube VAO/VBO
        s_IBLData.CubeVAO = nullptr;
        s_IBLData.CubeVBO = nullptr;
        
        // 释放 Shader 引用
        s_IBLData.BRDFShader = nullptr;
        s_IBLData.IrradianceShader = nullptr;
        s_IBLData.PrefilterShader = nullptr;
        
        LF_CORE_INFO("IBLPrecompute::Shutdown() - All IBL resources released");
    }
    
    void IBLPrecompute::GenerateFromCubemap(uint32_t envCubemapID)
    {
        if (envCubemapID == 0)
        {
            LF_CORE_WARN("IBLPrecompute::GenerateFromCubemap() - Invalid cubemap ID");
            s_IBLData.Data.Valid = false;
            return;
        }
        
        SaveRenderState();
        
        // 释放旧的 Irradiance Map 和 Prefilter Map（如果存在）
        if (s_IBLData.Data.IrradianceMapID != 0)
        {
            glDeleteTextures(1, &s_IBLData.Data.IrradianceMapID);
            s_IBLData.Data.IrradianceMapID = 0;
        }
        if (s_IBLData.Data.PrefilterMapID != 0)
        {
            glDeleteTextures(1, &s_IBLData.Data.PrefilterMapID);
            s_IBLData.Data.PrefilterMapID = 0;
        }
        
        // 生成 Irradiance Map
        s_IBLData.Data.IrradianceMapID = GenerateIrradianceMap(envCubemapID);
        
        // 生成 Prefiltered Environment Map
        s_IBLData.Data.PrefilterMapID = GeneratePrefilterMap(envCubemapID);
        
        s_IBLData.Data.Valid = true;
        
        RestoreRenderState();
        
        LF_CORE_INFO("IBLPrecompute::GenerateFromCubemap() - IBL data generated successfully");
    }
    
    const IBLData& IBLPrecompute::GetIBLData()
    {
        return s_IBLData.Data;
    }
    
    bool IBLPrecompute::IsValid()
    {
        return s_IBLData.Data.Valid;
    }
    
    void IBLPrecompute::GenerateDefaultFallbackTextures()
    {
        // ---- 创建 1×1 黑色 Cubemap（IBL 关闭时绑定到 IrradianceMap / PrefilterMap 槽位） ----
        glCreateTextures(GL_TEXTURE_CUBE_MAP, 1, &s_IBLData.Data.DefaultBlackCubemapID);
        glTextureStorage2D(s_IBLData.Data.DefaultBlackCubemapID, 1, GL_RGB16F, 1, 1);
        glTextureParameteri(s_IBLData.Data.DefaultBlackCubemapID, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTextureParameteri(s_IBLData.Data.DefaultBlackCubemapID, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        glTextureParameteri(s_IBLData.Data.DefaultBlackCubemapID, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);
        glTextureParameteri(s_IBLData.Data.DefaultBlackCubemapID, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTextureParameteri(s_IBLData.Data.DefaultBlackCubemapID, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        
        // 填充 6 个面为黑色（RGB16F = 0.0）
        float blackPixel[3] = { 0.0f, 0.0f, 0.0f };
        for (int face = 0; face < 6; ++face)
        {
            glTextureSubImage3D(s_IBLData.Data.DefaultBlackCubemapID, 0, 0, 0, face, 1, 1, 1, GL_RGB, GL_FLOAT, blackPixel);
        }
        
        // ---- 创建 1×1 黑色 Texture2D（IBL 关闭时绑定到 BRDFLUT 槽位） ----
        glCreateTextures(GL_TEXTURE_2D, 1, &s_IBLData.Data.DefaultBlack2DID);
        glTextureStorage2D(s_IBLData.Data.DefaultBlack2DID, 1, GL_RG16F, 1, 1);
        glTextureParameteri(s_IBLData.Data.DefaultBlack2DID, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTextureParameteri(s_IBLData.Data.DefaultBlack2DID, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        glTextureParameteri(s_IBLData.Data.DefaultBlack2DID, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTextureParameteri(s_IBLData.Data.DefaultBlack2DID, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        
        // 填充为黑色（RG16F = 0.0）
        float blackPixel2[2] = { 0.0f, 0.0f };
        glTextureSubImage2D(s_IBLData.Data.DefaultBlack2DID, 0, 0, 0, 1, 1, GL_RG, GL_FLOAT, blackPixel2);
        
        LF_CORE_INFO("IBLPrecompute - Default fallback textures generated (1x1 black cubemap + 1x1 black 2D)");
    }
    
    uint32_t IBLPrecompute::GenerateBRDFLUT()
    {
        const uint32_t resolution = 512;
        
        SaveRenderState();
        
        // 1. 创建 RG16F 纹理
        uint32_t brdfLUT;
        glCreateTextures(GL_TEXTURE_2D, 1, &brdfLUT);
        glTextureStorage2D(brdfLUT, 1, GL_RG16F, resolution, resolution);
        glTextureParameteri(brdfLUT, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTextureParameteri(brdfLUT, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        glTextureParameteri(brdfLUT, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTextureParameteri(brdfLUT, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        
        // 2. 创建临时 FBO
        uint32_t fbo;
        glCreateFramebuffers(1, &fbo);
        glNamedFramebufferTexture(fbo, GL_COLOR_ATTACHMENT0, brdfLUT, 0);
        
        // 3. 渲染
        glBindFramebuffer(GL_FRAMEBUFFER, fbo);
        glViewport(0, 0, resolution, resolution);
        glClear(GL_COLOR_BUFFER_BIT);
        
        s_IBLData.BRDFShader->Bind();
        ScreenQuad::Draw();
        
        // 4. 清理临时 FBO
        glDeleteFramebuffers(1, &fbo);
        
        RestoreRenderState();
        
        return brdfLUT;
    }
    
    uint32_t IBLPrecompute::GenerateIrradianceMap(uint32_t envCubemap)
    {
        const uint32_t resolution = 32;
        
        // 1. 创建 Irradiance Cubemap
        uint32_t irradianceMap;
        glCreateTextures(GL_TEXTURE_CUBE_MAP, 1, &irradianceMap);
        glTextureStorage2D(irradianceMap, 1, GL_RGB16F, resolution, resolution);
        glTextureParameteri(irradianceMap, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTextureParameteri(irradianceMap, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        glTextureParameteri(irradianceMap, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);
        glTextureParameteri(irradianceMap, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTextureParameteri(irradianceMap, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        
        // 2. 创建临时 FBO + RBO
        uint32_t fbo, rbo;
        glCreateFramebuffers(1, &fbo);
        glCreateRenderbuffers(1, &rbo);
        glNamedRenderbufferStorage(rbo, GL_DEPTH_COMPONENT24, resolution, resolution);
        glNamedFramebufferRenderbuffer(fbo, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, rbo);
        
        // 3. 渲染 6 个面
        s_IBLData.IrradianceShader->Bind();
        s_IBLData.IrradianceShader->SetMat4("u_Projection", s_IBLData.CaptureProjection);
        
        // 绑定原始环境 Cubemap
        glBindTextureUnit(0, envCubemap);
        s_IBLData.IrradianceShader->SetInt("u_EnvironmentMap", 0);
        
        glBindFramebuffer(GL_FRAMEBUFFER, fbo);
        glViewport(0, 0, resolution, resolution);
        
        for (int face = 0; face < 6; ++face)
        {
            s_IBLData.IrradianceShader->SetMat4("u_View", s_IBLData.CaptureViews[face]);
            
            // 将 Cubemap 的第 face 面绑定为 FBO 颜色附件
            glNamedFramebufferTextureLayer(fbo, GL_COLOR_ATTACHMENT0, irradianceMap, 0, face);
            
            glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
            
            // 绘制 Cube
            RenderCommand::DrawArrays(s_IBLData.CubeVAO, 36);
        }
        
        // 4. 清理
        glDeleteFramebuffers(1, &fbo);
        glDeleteRenderbuffers(1, &rbo);
        
        LF_CORE_INFO("IBLPrecompute - Irradiance Map generated ({0}x{0} per face)", resolution);
        
        return irradianceMap;
    }
    
    uint32_t IBLPrecompute::GeneratePrefilterMap(uint32_t envCubemap)
    {
        const uint32_t resolution = 128;
        const uint32_t maxMipLevels = 5;
        
        // 1. 创建带 Mipmap 的 Cubemap
        uint32_t prefilterMap;
        glCreateTextures(GL_TEXTURE_CUBE_MAP, 1, &prefilterMap);
        glTextureStorage2D(prefilterMap, maxMipLevels, GL_RGB16F, resolution, resolution);
        glTextureParameteri(prefilterMap, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTextureParameteri(prefilterMap, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        glTextureParameteri(prefilterMap, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);
        glTextureParameteri(prefilterMap, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
        glTextureParameteri(prefilterMap, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        
        // 2. 创建临时 FBO + RBO
        uint32_t fbo, rbo;
        glCreateFramebuffers(1, &fbo);
        glCreateRenderbuffers(1, &rbo);
        
        // 3. 对每个 Mip Level 渲染
        s_IBLData.PrefilterShader->Bind();
        s_IBLData.PrefilterShader->SetMat4("u_Projection", s_IBLData.CaptureProjection);
        
        // 绑定原始环境 Cubemap（需要 Mipmap 用于 textureLod 采样）
        // 先为原始环境 Cubemap 生成 Mipmap
        glGenerateTextureMipmap(envCubemap);
        
        glBindTextureUnit(0, envCubemap);
        s_IBLData.PrefilterShader->SetInt("u_EnvironmentMap", 0);
        
        glBindFramebuffer(GL_FRAMEBUFFER, fbo);
        
        for (uint32_t mip = 0; mip < maxMipLevels; ++mip)
        {
            // 当前 Mip Level 的分辨率
            uint32_t mipWidth = static_cast<uint32_t>(resolution * std::pow(0.5f, static_cast<float>(mip)));
            uint32_t mipHeight = mipWidth;
            
            // 调整 RBO 大小
            glNamedRenderbufferStorage(rbo, GL_DEPTH_COMPONENT24, mipWidth, mipHeight);
            glNamedFramebufferRenderbuffer(fbo, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, rbo);
            
            glViewport(0, 0, mipWidth, mipHeight);
            
            // 粗糙度 = mip / (maxMipLevels - 1)
            float roughness = static_cast<float>(mip) / static_cast<float>(maxMipLevels - 1);
            s_IBLData.PrefilterShader->SetFloat("u_Roughness", roughness);
            
            // 渲染 6 个面
            for (int face = 0; face < 6; ++face)
            {
                s_IBLData.PrefilterShader->SetMat4("u_View", s_IBLData.CaptureViews[face]);
                
                // 绑定 Cubemap 的第 face 面的第 mip 级到 FBO
                glNamedFramebufferTextureLayer(fbo, GL_COLOR_ATTACHMENT0, prefilterMap, mip, face);
                
                glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
                RenderCommand::DrawArrays(s_IBLData.CubeVAO, 36);
            }
        }
        
        // 4. 清理
        glDeleteFramebuffers(1, &fbo);
        glDeleteRenderbuffers(1, &rbo);
        
        // 记录最大 Mip Level
        s_IBLData.Data.PrefilterMaxMipLevel = maxMipLevels - 1;
        
        LF_CORE_INFO("IBLPrecompute - Prefiltered Map generated ({0}x{0} per face, {1} mip levels)", resolution, maxMipLevels);
        
        return prefilterMap;
    }
}
