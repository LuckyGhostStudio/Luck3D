#include "lcpch.h"
#include "Renderer3D.h"

#include "Shader.h"
#include "Framebuffer.h"

#include "IBLPrecompute.h"

#include "Lucky/Asset/AssetManager.h"
#include "Lucky/Project/Project.h"

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

        const std::filesystem::path assetDir = Project::GetActive()->GetAssetDirectory();

        // 加载引擎内部着色器
        s_Data.ShaderLib->Load((assetDir / "Shaders/Internal/InternalError").string());
        s_Data.ShaderLib->Load((assetDir / "Shaders/Internal/EntityID").string());
        s_Data.ShaderLib->Load((assetDir / "Shaders/Internal/Outline/Silhouette").string());
        s_Data.ShaderLib->Load((assetDir / "Shaders/Internal/Outline/OutlineComposite").string());
        s_Data.ShaderLib->Load((assetDir / "Shaders/Internal/Shadow/Shadow").string());
        s_Data.ShaderLib->Load((assetDir / "Shaders/Internal/Shadow/PointShadow").string());
        s_Data.ShaderLib->Load((assetDir / "Shaders/Internal/PostProcess/Tonemapping").string());
        s_Data.ShaderLib->Load((assetDir / "Shaders/Internal/PostProcess/BrightExtract").string());
        s_Data.ShaderLib->Load((assetDir / "Shaders/Internal/PostProcess/GaussianBlur").string());
        s_Data.ShaderLib->Load((assetDir / "Shaders/Internal/PostProcess/BloomComposite").string());
        s_Data.ShaderLib->Load((assetDir / "Shaders/Internal/PostProcess/FXAA").string());
        s_Data.ShaderLib->Load((assetDir / "Shaders/Internal/PostProcess/Vignette").string());
        s_Data.ShaderLib->Load((assetDir / "Shaders/Internal/Debug/DebugCSMVisualize").string());
        s_Data.ShaderLib->Load((assetDir / "Shaders/Internal/IBL/BRDFIntegration").string());
        s_Data.ShaderLib->Load((assetDir / "Shaders/Internal/IBL/IrradianceConvolution").string());
        s_Data.ShaderLib->Load((assetDir / "Shaders/Internal/IBL/PrefilterConvolution").string());

        // 加载用户可见着色器
        s_Data.ShaderLib->Load((assetDir / "Shaders/Standard").string());
        s_Data.ShaderLib->Load((assetDir / "Shaders/Skybox").string());
        s_Data.ShaderLib->Load((assetDir / "Shaders/Sprite").string());

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
        const std::filesystem::path skyboxDir = assetDir / "Textures/Skybox";
        std::array skyboxFaces = {
            (skyboxDir / "right.jpg").string(),
            (skyboxDir / "left.jpg").string(),
            (skyboxDir / "up.jpg").string(),
            (skyboxDir / "down.jpg").string(),
            (skyboxDir / "front.jpg").string(),
            (skyboxDir / "back.jpg").string()
        };

        Ref<TextureCube> skyboxCubemap = nullptr;
        if (std::filesystem::exists(skyboxFaces[0]))
        {
            skyboxCubemap = TextureCube::Create(skyboxFaces);

            s_Data.DefaultSkyboxMaterial->SetTextureCube("u_SkyboxMap", skyboxCubemap);
            s_Data.DefaultSkyboxMaterial->SetFloat("u_Exposure", 1.0f);
            s_Data.DefaultSkyboxMaterial->SetFloat4("u_Tint", glm::vec4(1.0f));

            LF_INFO("Skybox loaded successfully from: {0}", skyboxDir.string());
        }
        else
        {
            LF_WARN("Skybox textures not found at: {0} (skipping skybox)", skyboxDir.string());
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