#include "lcpch.h"
#include "AssetInspectors.h"

#include "Lucky/Editor/MaterialEditor.h"
#include "Lucky/Editor/EditorIconManager.h"
#include "Lucky/Editor/InspectorHeader.h"
#include "Lucky/Editor/Preview/AssetPreviewCache.h"

#include "Lucky/UI/Widgets.h"

#include "Lucky/Asset/AssetManager.h"
#include "Lucky/Renderer/Material.h"
#include "Lucky/Renderer/Mesh.h"
#include "Lucky/Renderer/Texture.h"
#include "Lucky/Scene/Scene.h"

#include <imgui.h>
#include <filesystem>

namespace Lucky
{
    /// <summary>
    /// Header 图标：纹理 + 内容类别（内容类别决定框内缩放策略，见 GridIconKind）
    /// </summary>
    struct HeaderIcon
    {
        Ref<Texture2D> Texture;
        UI::GridIconKind Kind = UI::GridIconKind::Symbolic;
    };

    /// <summary>
    /// 取 Header 图标：可预览资产（纹理 / 材质 / 网格 / 场景）优先用真实缩略图（Content），
    /// 与资产面板共用 AssetPreviewCache；取不到或不支持的类型回退静态类型图标（Symbolic）
    /// </summary>
    static HeaderIcon GetHeaderIcon(AssetHandle handle, AssetType type)
    {
        // 纹理资产原图即预览
        if (type == AssetType::Texture2D)
        {
            Ref<Texture2D> texture = AssetManager::GetAsset<Texture2D>(handle);
            if (texture)
            {
                return { texture, UI::GridIconKind::Content };
            }
        }

        // 材质 / 网格 / 场景走预览缓存
        if (type == AssetType::Material || type == AssetType::Mesh || type == AssetType::Scene)
        {
            const Ref<Texture2D>& preview = AssetPreviewCache::GetOrRender(handle, type);
            if (preview)
            {
                return { preview, UI::GridIconKind::Content };
            }
        }

        return { EditorIconManager::GetAssetTypeIcon(type), UI::GridIconKind::Symbolic };
    }

    /// <summary>
    /// 绘制所有 AssetInspector Header：图标 + "名称 (AssetType)" + 设置按钮
    /// </summary>
    static void DrawAssetHeader(AssetHandle handle, const std::string& name, AssetType type)
    {
        // 名称：若资产未 SetName（如原生纹理），fallback 到文件名 stem；仍为空时给占位
        const std::string& path = AssetManager::GetAssetFilePath(handle);
        std::string displayName = name;
        if (displayName.empty() && !path.empty())
        {
            displayName = std::filesystem::path(path).stem().string();
        }
        if (displayName.empty())
        {
            displayName = "<Unnamed>";
        }

        HeaderIcon icon = GetHeaderIcon(handle, type);
        InspectorHeader::Draw(icon.Texture, displayName, AssetTypeToString(type), "AssetSettings", icon.Kind);
    }
    
    void MaterialInspector::Draw(AssetHandle handle)
    {
        Ref<Material> material = AssetManager::GetAsset<Material>(handle);
        if (!material)
        {
            ImGui::TextDisabled("Failed to load material (handle=%llu).", static_cast<uint64_t>(handle));
            return;
        }

        DrawAssetHeader(handle, material->GetName(), AssetType::Material);
        
        MaterialEditor::OnGUI(material);
    }
    
    void MeshInspector::Draw(AssetHandle handle)
    {
        Ref<Mesh> mesh = AssetManager::GetAsset<Mesh>(handle);
        if (!mesh)
        {
            ImGui::TextDisabled("Failed to load mesh (handle=%llu).", static_cast<uint64_t>(handle));
            return;
        }

        DrawAssetHeader(handle, mesh->GetName(), AssetType::Mesh);
    }
    
    void Texture2DInspector::Draw(AssetHandle handle)
    {
        Ref<Texture2D> texture = AssetManager::GetAsset<Texture2D>(handle);
        if (!texture)
        {
            ImGui::TextDisabled("Failed to load texture (handle=%llu).", static_cast<uint64_t>(handle));
            return;
        }

        DrawAssetHeader(handle, texture->GetName(), AssetType::Texture2D);
    }

    void SceneInspector::Draw(AssetHandle handle)
    {
        DrawAssetHeader(handle, std::string(), AssetType::Scene);
    }

    void ShaderInspector::Draw(AssetHandle handle)
    {
        DrawAssetHeader(handle, std::string(), AssetType::Shader);
    }

    void ScriptInspector::Draw(AssetHandle handle)
    {
        DrawAssetHeader(handle, std::string(), AssetType::Script);
    }
}
