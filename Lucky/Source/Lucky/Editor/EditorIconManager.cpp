#include "lcpch.h"
#include "EditorIconManager.h"

namespace Lucky
{
    /// <summary>
    /// 图标管理器内部数据
    /// </summary>
    struct EditorIconData
    {
        // ---- 资产类型图标 ----
        std::unordered_map<AssetType, Ref<Texture2D>> AssetTypeIcons;

        // ---- 组件图标 ----
        std::unordered_map<ComponentType, Ref<Texture2D>> ComponentIcons;

        // ---- 实体图标 ----
        Ref<Texture2D> EntityIcon;

        // ---- 通用图标 ----
        Ref<Texture2D> FolderIcon;
        Ref<Texture2D> FolderOpenIcon;
        Ref<Texture2D> FileIcon;
    };

    static EditorIconData s_IconData;

    static constexpr const char* s_IconRootPath = "Resources/Icons";

    /// <summary>
    /// 加载单个图标纹理
    /// </summary>
    static Ref<Texture2D> LoadIcon(const std::string& relativePath)
    {
        std::string fullPath = std::string(s_IconRootPath) + "/" + relativePath;

        Ref<Texture2D> texture = Texture2D::Create(fullPath);
        if (!texture || texture->GetRendererID() == 0)
        {
            LF_CORE_WARN("EditorIconManager: Failed to load icon '{0}'", fullPath);
            return nullptr;
        }

        return texture;
    }

    void EditorIconManager::Init()
    {
        LF_CORE_INFO("EditorIconManager::Init - Loading editor icons...");

        // ---- 加载通用图标 ----
        s_IconData.FolderIcon       = LoadIcon("Common/Folder.png");
        s_IconData.FolderOpenIcon   = LoadIcon("Common/FolderOpen.png");
        s_IconData.FileIcon         = LoadIcon("Common/File.png");

        // ---- 加载资产类型图标 ----
        s_IconData.AssetTypeIcons[AssetType::Material]  = LoadIcon("Asset/Material.png");
        s_IconData.AssetTypeIcons[AssetType::Mesh]      = LoadIcon("Asset/Mesh.png");
        s_IconData.AssetTypeIcons[AssetType::Texture2D] = LoadIcon("Asset/Texture.png");
        s_IconData.AssetTypeIcons[AssetType::Scene]     = LoadIcon("Asset/Scene.png");
        s_IconData.AssetTypeIcons[AssetType::Shader]    = LoadIcon("Asset/Shader.png");

        // ---- 加载组件图标 ----
        s_IconData.ComponentIcons[ComponentType::Transform]          = LoadIcon("Component/Transform.png");
        s_IconData.ComponentIcons[ComponentType::MeshFilter]         = LoadIcon("Component/MeshFilter.png");
        s_IconData.ComponentIcons[ComponentType::MeshRenderer]       = LoadIcon("Component/MeshRenderer.png");
        s_IconData.ComponentIcons[ComponentType::Light]              = LoadIcon("Component/Light.png");
        s_IconData.ComponentIcons[ComponentType::PostProcessVolume]  = LoadIcon("Component/PostProcessVolume.png");

        // ---- 加载实体图标 ----
        s_IconData.EntityIcon = LoadIcon("Entity/Entity.png");

        LF_CORE_INFO("EditorIconManager::Init - Done.");
    }

    void EditorIconManager::Shutdown()
    {
        LF_CORE_INFO("EditorIconManager::Shutdown");

        s_IconData.AssetTypeIcons.clear();
        s_IconData.ComponentIcons.clear();
        s_IconData.EntityIcon.reset();
        s_IconData.FolderIcon.reset();
        s_IconData.FolderOpenIcon.reset();
        s_IconData.FileIcon.reset();
    }

    const Ref<Texture2D>& EditorIconManager::GetAssetTypeIcon(AssetType type)
    {
        auto it = s_IconData.AssetTypeIcons.find(type);
        if (it != s_IconData.AssetTypeIcons.end() && it->second)
        {
            return it->second;
        }

        return s_IconData.FileIcon;
    }

    const Ref<Texture2D>& EditorIconManager::GetComponentIcon(ComponentType type)
    {
        auto it = s_IconData.ComponentIcons.find(type);
        if (it != s_IconData.ComponentIcons.end() && it->second)
        {
            return it->second;
        }

        static Ref<Texture2D> s_NullIcon = nullptr;
        return s_NullIcon;
    }

    const Ref<Texture2D>& EditorIconManager::GetEntityIcon()
    {
        return s_IconData.EntityIcon;
    }

    const Ref<Texture2D>& EditorIconManager::GetFolderIcon(bool isOpen)
    {
        if (isOpen && s_IconData.FolderOpenIcon)
        {
            return s_IconData.FolderOpenIcon;
        }

        return s_IconData.FolderIcon;
    }

    const Ref<Texture2D>& EditorIconManager::GetFileIcon()
    {
        return s_IconData.FileIcon;
    }
}
