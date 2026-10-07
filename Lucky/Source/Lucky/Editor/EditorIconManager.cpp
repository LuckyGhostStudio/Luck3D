#include "lcpch.h"
#include "EditorIconManager.h"

#include "Lucky/Core/FileSystem.h"

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

        // ---- 光源子类型图标 ----
        std::unordered_map<LightType, Ref<Texture2D>> LightIcons;

        // ---- 实体图标 ----
        Ref<Texture2D> EntityIcon;

        // ---- 通用图标 ----
        Ref<Texture2D> FolderIcon;
        Ref<Texture2D> FolderOpenIcon;
        Ref<Texture2D> FolderEmptyIcon;          // 空文件夹
        Ref<Texture2D> FileIcon;

        // ---- 通用图标（128 大尺寸版，Grid 布局等大图标场景用；缺失的回退普通版）----
        Ref<Texture2D> FolderIconLarge;
        Ref<Texture2D> FolderOpenIconLarge;
        Ref<Texture2D> FolderEmptyIconLarge;
        Ref<Texture2D> FileIconLarge;

        // ---- 资产类型图标（128 大尺寸版；只覆盖有资源的类型，Mesh/Texture2D/Scene 走真实预览）----
        std::unordered_map<AssetType, Ref<Texture2D>> AssetTypeIconsLarge;

        // ---- 拖拽图标 ----
        Ref<Texture2D> DragDropIcon;         // 通用拖拽图标（虚线框 + 右下角 +）
        Ref<Texture2D> DragRejectedIcon;     // 禁止拖拽图标（红色禁止圈）

        // ---- 设置图标 ----
        Ref<Texture2D> SettingsIcon;         // 设置图标

        // ---- 工具条图标 ----
        Ref<Texture2D> PlayIcon;
        Ref<Texture2D> PauseIcon;
        Ref<Texture2D> SelectionIcon;
        Ref<Texture2D> TranslationIcon;
        Ref<Texture2D> RotationIcon;
        Ref<Texture2D> ScaleIcon;
    };

    static EditorIconData s_IconData;

    /// <summary>
    /// 图标根目录：跟随 exe 分发（Resources/Icons 相对 exe）
    /// </summary>
    static const std::filesystem::path& GetIconRootPath()
    {
        static const std::filesystem::path s_IconRootPath = FileSystem::GetEditorExecutableDirectory() / "Resources" / "Icons";
        return s_IconRootPath;
    }

    /// <summary>
    /// 加载单个图标纹理
    /// </summary>
    static Ref<Texture2D> LoadIcon(const std::string& relativePath)
    {
        std::filesystem::path fullPath = GetIconRootPath() / relativePath;
        std::string fullPathStr = fullPath.string();

        Ref<Texture2D> texture = Texture2D::Create(fullPathStr);
        if (!texture || texture->GetRendererID() == 0)
        {
            LF_CORE_WARN("EditorIconManager: Failed to load icon '{0}'", fullPathStr);
            return nullptr;
        }

        return texture;
    }

    void EditorIconManager::Init()
    {
        LF_CORE_INFO("EditorIconManager::Init - Loading editor icons...");

        // ---- 加载通用图标 ----
        s_IconData.FolderIcon           = LoadIcon("Common/Folder.png");
        s_IconData.FolderOpenIcon       = LoadIcon("Common/FolderOpen.png");
        s_IconData.FolderEmptyIcon      = LoadIcon("Common/FolderEmpty.png");
        s_IconData.FileIcon             = LoadIcon("Common/File.png");

        // ---- 通用图标（128 大尺寸版）----
        s_IconData.FolderIconLarge      = LoadIcon("Common/Folder128.png");
        s_IconData.FolderOpenIconLarge  = LoadIcon("Common/FolderOpen128.png");
        s_IconData.FolderEmptyIconLarge = LoadIcon("Common/FolderEmpty128.png");
        s_IconData.FileIconLarge        = LoadIcon("Common/File128.png");

        // ---- 加载拖拽图标 ----
        s_IconData.DragDropIcon     = LoadIcon("Common/DragDrop.png");
        s_IconData.DragRejectedIcon = LoadIcon("Common/DragRejected.png");

        // ---- 加载设置图标 ----
        s_IconData.SettingsIcon     = LoadIcon("Common/Settings.png");

        // ---- 加载工具条图标 ----
        s_IconData.PlayIcon = LoadIcon("Toolbar/Play.png");
        s_IconData.PauseIcon = LoadIcon("Toolbar/Pause.png");
        s_IconData.SelectionIcon = LoadIcon("Toolbar/Selection.png");
        s_IconData.TranslationIcon = LoadIcon("Toolbar/Translation.png");
        s_IconData.RotationIcon = LoadIcon("Toolbar/Rotation.png");
        s_IconData.ScaleIcon = LoadIcon("Toolbar/Scale.png");

        // ---- 加载资产类型图标 ----
        s_IconData.AssetTypeIcons[AssetType::Material]  = LoadIcon("Asset/Material.png");
        s_IconData.AssetTypeIcons[AssetType::Mesh]      = LoadIcon("Asset/Mesh.png");
        s_IconData.AssetTypeIcons[AssetType::Texture2D] = LoadIcon("Asset/Texture.png");
        s_IconData.AssetTypeIcons[AssetType::Scene]     = LoadIcon("Asset/Scene.png");
        s_IconData.AssetTypeIcons[AssetType::Shader]    = LoadIcon("Asset/Shader.png");
        s_IconData.AssetTypeIcons[AssetType::Script]    = LoadIcon("Asset/Script.png");

        // ---- 资产类型图标（128 大尺寸版）----
        // Material 大图供 Pending Create 占位使用（占位阶段资产尚未落盘，没有可预览内容，只能显示静态图标）；
        // Mesh/Texture2D/Scene 走真实预览，无需大图
        s_IconData.AssetTypeIconsLarge[AssetType::Material] = LoadIcon("Asset/Material128.png");
        s_IconData.AssetTypeIconsLarge[AssetType::Shader] = LoadIcon("Asset/Shader128.png");
        s_IconData.AssetTypeIconsLarge[AssetType::Script] = LoadIcon("Asset/Script128.png");

        // ---- 加载组件图标 ----
        s_IconData.ComponentIcons[ComponentType::Transform]          = LoadIcon("Component/Transform.png");
        s_IconData.ComponentIcons[ComponentType::MeshFilter]         = LoadIcon("Component/MeshFilter.png");
        s_IconData.ComponentIcons[ComponentType::MeshRenderer]       = LoadIcon("Component/MeshRenderer.png");
        s_IconData.ComponentIcons[ComponentType::SpriteRenderer]     = LoadIcon("Component/SpriteRenderer.png");
        s_IconData.ComponentIcons[ComponentType::Light]              = LoadIcon("Component/Light.png");
        s_IconData.ComponentIcons[ComponentType::PostProcessVolume]  = LoadIcon("Component/PostProcessVolume.png");
        s_IconData.ComponentIcons[ComponentType::Camera]             = LoadIcon("Component/Camera.png");
        s_IconData.ComponentIcons[ComponentType::Script]             = LoadIcon("Component/Script.png");

        // ---- 加载光源子类型图标 ----
        s_IconData.LightIcons[LightType::Directional]   = LoadIcon("Component/DirectionalLight.png");
        s_IconData.LightIcons[LightType::Point]         = LoadIcon("Component/PointLight.png");
        s_IconData.LightIcons[LightType::Spot]          = LoadIcon("Component/SpotLight.png");

        // ---- 加载实体图标 ----
        s_IconData.EntityIcon = LoadIcon("Entity/Entity.png");

        LF_CORE_INFO("EditorIconManager::Init - Done.");
    }

    void EditorIconManager::Shutdown()
    {
        LF_CORE_INFO("EditorIconManager::Shutdown");

        s_IconData.AssetTypeIcons.clear();
        s_IconData.AssetTypeIconsLarge.clear();
        s_IconData.ComponentIcons.clear();
        s_IconData.LightIcons.clear();
        s_IconData.EntityIcon.reset();
        s_IconData.FolderIcon.reset();
        s_IconData.FolderOpenIcon.reset();
        s_IconData.FolderEmptyIcon.reset();
        s_IconData.FileIcon.reset();
        s_IconData.FolderIconLarge.reset();
        s_IconData.FolderOpenIconLarge.reset();
        s_IconData.FolderEmptyIconLarge.reset();
        s_IconData.FileIconLarge.reset();
        s_IconData.DragDropIcon.reset();
        s_IconData.DragRejectedIcon.reset();
        s_IconData.SettingsIcon.reset();
        s_IconData.PlayIcon.reset();
        s_IconData.PauseIcon.reset();
    }

    const Ref<Texture2D>& EditorIconManager::GetAssetTypeIcon(AssetType type, bool large)
    {
        if (large)
        {
            auto itLarge = s_IconData.AssetTypeIconsLarge.find(type);
            if (itLarge != s_IconData.AssetTypeIconsLarge.end() && itLarge->second)
            {
                return itLarge->second;
            }
        }

        auto it = s_IconData.AssetTypeIcons.find(type);
        if (it != s_IconData.AssetTypeIcons.end() && it->second)
        {
            return it->second;
        }

        // 未知类型回退通用文件图标（large 优先 128 版）
        if (large && s_IconData.FileIconLarge)
        {
            return s_IconData.FileIconLarge;
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

    const Ref<Texture2D>& EditorIconManager::GetLightIcon(LightType lightType)
    {
        auto it = s_IconData.LightIcons.find(lightType);
        if (it != s_IconData.LightIcons.end() && it->second)
        {
            return it->second;
        }

        // 回退到通用 Light 图标
        return GetComponentIcon(ComponentType::Light);
    }

    const Ref<Texture2D>& EditorIconManager::GetEntityIcon()
    {
        return s_IconData.EntityIcon;
    }

    const Ref<Texture2D>& EditorIconManager::GetFolderIcon(bool isOpen, bool isEmpty, bool large)
    {
        // 空文件夹优先空图标（不分开合状态）；128 版缺失回退普通空图标，再缺失落回普通文件夹图标
        if (isEmpty)
        {
            if (large && s_IconData.FolderEmptyIconLarge)
            {
                return s_IconData.FolderEmptyIconLarge;
            }
            if (s_IconData.FolderEmptyIcon)
            {
                return s_IconData.FolderEmptyIcon;
            }
        }

        // 128 大尺寸版（缺失回退普通版）
        if (large)
        {
            if (isOpen && s_IconData.FolderOpenIconLarge)
            {
                return s_IconData.FolderOpenIconLarge;
            }
            if (!isOpen && s_IconData.FolderIconLarge)
            {
                return s_IconData.FolderIconLarge;
            }
        }

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

    const Ref<Texture2D>& EditorIconManager::GetDragDropIcon(bool rejected)
    {
        return rejected ? s_IconData.DragRejectedIcon : s_IconData.DragDropIcon;
    }

    const Ref<Texture2D>& EditorIconManager::GetSettingsIcon()
    {
        return s_IconData.SettingsIcon;
    }

    const Ref<Texture2D>& EditorIconManager::GetPlayIcon()
    {
        return s_IconData.PlayIcon;
    }

    const Ref<Texture2D>& EditorIconManager::GetPauseIcon()
    {
        return s_IconData.PauseIcon;
    }

    const Ref<Texture2D>& EditorIconManager::GetSelectionIcon()
    {
        return s_IconData.SelectionIcon;
    }

    const Ref<Texture2D>& EditorIconManager::GetTranslationIcon()
    {
        return s_IconData.TranslationIcon;
    }

    const Ref<Texture2D>& EditorIconManager::GetRotationIcon()
    {
        return s_IconData.RotationIcon;
    }

    const Ref<Texture2D>& EditorIconManager::GetScaleIcon()
    {
        return s_IconData.ScaleIcon;
    }
}
