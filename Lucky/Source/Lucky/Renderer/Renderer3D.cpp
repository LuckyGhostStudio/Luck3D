#include "lcpch.h"
#include "Renderer3D.h"

#include "Shader.h"
#include "Framebuffer.h"

#include "IBLPrecompute.h"

#include "Lucky/Asset/AssetManager.h"

#include <filesystem>

namespace Lucky
{
    /// <summary>
    /// 渲染器全局共享数据
    /// </summary>
    struct Renderer3DData
    {
        Ref<ShaderLibrary> ShaderLib;

        Ref<Shader> InternalErrorShader;
        Ref<Shader> StandardShader;
        Ref<Shader> SkyboxShader;
        Ref<Material> InternalErrorMaterial;
        Ref<Material> DefaultMaterial;
        Ref<Material> DefaultSkyboxMaterial;

        std::unordered_map<TextureDefault, Ref<Texture2D>> DefaultTextures;
    };

    static Renderer3DData s_Data;

    void Renderer3D::Init()
    {
        s_Data.ShaderLib = CreateRef<ShaderLibrary>();

        // 加载引擎内部着色器
        s_Data.ShaderLib->Load("Assets/Shaders/Internal/InternalError");
        s_Data.ShaderLib->Load("Assets/Shaders/Internal/EntityID");
        s_Data.ShaderLib->Load("Assets/Shaders/Internal/Outline/Silhouette");
        s_Data.ShaderLib->Load("Assets/Shaders/Internal/Outline/OutlineComposite");
        s_Data.ShaderLib->Load("Assets/Shaders/Internal/Shadow/Shadow");
        s_Data.ShaderLib->Load("Assets/Shaders/Internal/Shadow/PointShadow");
        s_Data.ShaderLib->Load("Assets/Shaders/Internal/PostProcess/Tonemapping");
        s_Data.ShaderLib->Load("Assets/Shaders/Internal/PostProcess/BrightExtract");
        s_Data.ShaderLib->Load("Assets/Shaders/Internal/PostProcess/GaussianBlur");
        s_Data.ShaderLib->Load("Assets/Shaders/Internal/PostProcess/BloomComposite");
        s_Data.ShaderLib->Load("Assets/Shaders/Internal/PostProcess/FXAA");
        s_Data.ShaderLib->Load("Assets/Shaders/Internal/PostProcess/Vignette");
        s_Data.ShaderLib->Load("Assets/Shaders/Internal/Debug/DebugCSMVisualize");
        s_Data.ShaderLib->Load("Assets/Shaders/Internal/IBL/BRDFIntegration");
        s_Data.ShaderLib->Load("Assets/Shaders/Internal/IBL/IrradianceConvolution");
        s_Data.ShaderLib->Load("Assets/Shaders/Internal/IBL/PrefilterConvolution");

        // 加载用户可见着色器
        s_Data.ShaderLib->Load("Assets/Shaders/Standard");
        s_Data.ShaderLib->Load("Assets/Shaders/Skybox");
        s_Data.ShaderLib->Load("Assets/Shaders/Sprite");

        s_Data.InternalErrorShader = s_Data.ShaderLib->Get("InternalError");
        s_Data.StandardShader = s_Data.ShaderLib->Get("Standard");
        s_Data.SkyboxShader = s_Data.ShaderLib->Get("Skybox");

        // 创建内部材质
        s_Data.InternalErrorMaterial = CreateRef<Material>("InternalError", s_Data.InternalErrorShader);
        s_Data.DefaultMaterial = CreateRef<Material>("Default-Material", s_Data.StandardShader);
        s_Data.DefaultSkyboxMaterial = CreateRef<Material>("Default-Skybox", s_Data.ShaderLib->Get("Skybox"));

        // PBR 默认参数
        s_Data.DefaultMaterial->SetFloat4("u_Albedo", glm::vec4(0.8f, 0.8f, 0.8f, 1.0f));
        s_Data.DefaultMaterial->SetFloat("u_Metallic", 0.0f);
        s_Data.DefaultMaterial->SetFloat("u_Roughness", 0.5f);
        s_Data.DefaultMaterial->SetFloat("u_AO", 1.0f);
        s_Data.DefaultMaterial->SetFloat3("u_Emission", glm::vec3(0.0f));
        s_Data.DefaultMaterial->SetFloat("u_EmissionIntensity", 1.0f);

        // ======== 天空盒加载（硬编码 6 面 Cubemap） ========
        const std::string skyboxDir = "Assets/Textures/Skybox/";
        std::array skyboxFaces = {
            skyboxDir + "right.jpg",
            skyboxDir + "left.jpg",
            skyboxDir + "up.jpg",
            skyboxDir + "down.jpg",
            skyboxDir + "front.jpg",
            skyboxDir + "back.jpg"
        };

        Ref<TextureCube> skyboxCubemap = nullptr;
        if (std::filesystem::exists(skyboxFaces[0]))
        {
            skyboxCubemap = TextureCube::Create(skyboxFaces);

            s_Data.DefaultSkyboxMaterial->SetTextureCube("u_SkyboxMap", skyboxCubemap);
            s_Data.DefaultSkyboxMaterial->SetFloat("u_Exposure", 1.0f);
            s_Data.DefaultSkyboxMaterial->SetFloat4("u_Tint", glm::vec4(1.0f));

            LF_INFO("Skybox loaded successfully from: {0}", skyboxDir);
        }
        else
        {
            LF_WARN("Skybox textures not found at: {0} (skipping skybox)", skyboxDir);
        }

        AssetManager::EnsureAsset(s_Data.DefaultMaterial, "Assets/Internal/Materials/Default-Material.lmat");
        AssetManager::EnsureAsset(s_Data.DefaultSkyboxMaterial, "Assets/Internal/Materials/Default-Skybox.lmat");

        // 创建全局默认纹理
        uint32_t whiteData = 0xFFFFFFFF;
        s_Data.DefaultTextures[TextureDefault::White] = Texture2D::Create(1, 1);
        s_Data.DefaultTextures[TextureDefault::White]->SetData(&whiteData, sizeof(uint32_t));

        uint32_t blackData = 0xFF000000;
        s_Data.DefaultTextures[TextureDefault::Black] = Texture2D::Create(1, 1);
        s_Data.DefaultTextures[TextureDefault::Black]->SetData(&blackData, sizeof(uint32_t));

        uint32_t normalData = 0xFFFF8080;
        s_Data.DefaultTextures[TextureDefault::Normal] = Texture2D::Create(1, 1);
        s_Data.DefaultTextures[TextureDefault::Normal]->SetData(&normalData, sizeof(uint32_t));

        // ======== 初始化 IBL 预计算 ========
        IBLPrecompute::Init();

        if (skyboxCubemap)
        {
            IBLPrecompute::GenerateFromCubemap(skyboxCubemap->GetRendererID(), 128);
        }
    }

    void Renderer3D::Shutdown()
    {
        IBLPrecompute::Shutdown();
    }

    Ref<ShaderLibrary>& Renderer3D::GetShaderLibrary()
    {
        return s_Data.ShaderLib;
    }

    Ref<Material>& Renderer3D::GetInternalErrorMaterial()
    {
        return s_Data.InternalErrorMaterial;
    }

    Ref<Material>& Renderer3D::GetDefaultMaterial()
    {
        return s_Data.DefaultMaterial;
    }

    Ref<Material>& Renderer3D::GetDefaultSkyboxMaterial()
    {
        return s_Data.DefaultSkyboxMaterial;
    }

    const Ref<Texture2D>& Renderer3D::GetDefaultTexture(TextureDefault type)
    {
        auto it = s_Data.DefaultTextures.find(type);
        if (it != s_Data.DefaultTextures.end())
        {
            return it->second;
        }

        return s_Data.DefaultTextures[TextureDefault::White];
    }
}