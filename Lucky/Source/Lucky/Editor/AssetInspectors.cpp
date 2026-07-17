#include "lcpch.h"
#include "AssetInspectors.h"

#include "Lucky/Editor/MaterialEditor.h"
#include "Lucky/Editor/EditorIconManager.h"
#include "Lucky/Editor/InspectorHeader.h"

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

        const Ref<Texture2D>& icon = EditorIconManager::GetAssetTypeIcon(type);
        InspectorHeader::Draw(icon, displayName, AssetTypeToString(type), "AssetSettings");
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
}
