#include "lcpch.h"
#include "SelectionManager.h"

namespace Lucky
{
    // ---- 通用接口 ----

    void SelectionManager::Select(SelectionType type, UUID id)
    {
        s_SelectionType = type;
        s_SelectionID = id;
    }

    bool SelectionManager::IsSelected(SelectionType type, UUID id)
    {
        return s_SelectionType == type && s_SelectionID == id;
    }

    void SelectionManager::Deselect()
    {
        s_SelectionType = SelectionType::None;
        s_SelectionID = 0;
        s_SelectionPath.clear();
    }

    UUID SelectionManager::GetSelection()
    {
        return s_SelectionID;
    }

    SelectionType SelectionManager::GetSelectionType()
    {
        return s_SelectionType;
    }

    // ---- 语义糖：Entity ----

    void SelectionManager::Select(UUID entityID)
    {
        Select(SelectionType::Entity, entityID);
    }

    bool SelectionManager::IsSelected(UUID entityID)
    {
        return IsSelected(SelectionType::Entity, entityID);
    }

    // ---- 语义糖：Asset ----

    void SelectionManager::SelectAsset(AssetHandle handle)
    {
        Select(SelectionType::Asset, UUID(static_cast<uint64_t>(handle)));
    }

    bool SelectionManager::IsAssetSelected(AssetHandle handle)
    {
        return IsSelected(SelectionType::Asset, UUID(static_cast<uint64_t>(handle)));
    }

    // ---- 语义糖：Folder ----

    void SelectionManager::SelectFolder(const std::filesystem::path& path)
    {
        s_SelectionType = SelectionType::Folder;
        s_SelectionID = 0;              // Folder 无 UUID Payload
        s_SelectionPath = path;
    }

    bool SelectionManager::IsFolderSelected(const std::filesystem::path& path)
    {
        return s_SelectionType == SelectionType::Folder && s_SelectionPath == path;
    }

    const std::filesystem::path& SelectionManager::GetSelectedFolder()
    {
        return s_SelectionPath;
    }
}