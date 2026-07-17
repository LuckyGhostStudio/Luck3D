#pragma once

#include "Lucky/Scene/Entity.h"
#include "Lucky/Asset/AssetHandle.h"

#include <filesystem>

namespace Lucky
{
    /// <summary>
    /// 选择类型：用于区分当前选中的是 Entity / Asset / Folder
    /// Inspector 面板据此分发到不同的绘制逻辑
    /// </summary>
    enum class SelectionType : uint8_t
    {
        None = 0,   // 无选中
        Entity,     // 选中场景实体（Payload = Entity UUID，存于 s_SelectionID）
        Asset,      // 选中资产（Payload = AssetHandle，存于 s_SelectionID）
        Folder      // 选中目录（Payload = std::filesystem::path，存于 s_SelectionPath）
    };

    /// <summary>
    /// 选择管理器：全局维护"当前选中的对象"
    /// - Entity / Asset / Folder 三者互斥（同一时刻只能选中其一，对齐 Unity Inspector 行为）
    /// - Entity / Asset 的 Payload 是 UUID（存于 s_SelectionID）
    /// - Folder 的 Payload 是 filesystem::path（存于 s_SelectionPath，UUID 无法承载字符串）
    /// </summary>
    struct SelectionManager
    {
    private:
        inline static UUID s_SelectionID;
        inline static SelectionType s_SelectionType = SelectionType::None;
        inline static std::filesystem::path s_SelectionPath;    // 仅在 s_SelectionType == Folder 时有效
    public:
        // ---- 通用接口 ----

        /// <summary>
        /// 选中一个对象（带类型）
        /// </summary>
        static void Select(SelectionType type, UUID id);

        /// <summary>
        /// 判断某对象是否被选中（类型必须匹配）
        /// </summary>
        static bool IsSelected(SelectionType type, UUID id);

        /// <summary>
        /// 清空选中
        /// </summary>
        static void Deselect();

        /// <summary>
        /// 获取当前选中对象的 ID（Entity UUID 或 AssetHandle，根据 SelectionType 决定语义）
        /// </summary>
        static UUID GetSelection();

        /// <summary>
        /// 获取当前选中对象的类型
        /// </summary>
        static SelectionType GetSelectionType();

        // ---- 语义糖：Entity ----

        /// <summary>
        /// 选中 Entity（等价于 Select(SelectionType::Entity, id)）
        /// 保留旧签名 Select(UUID) 以兼容大量已有 Entity 语义的调用点
        /// </summary>
        static void Select(UUID entityID);

        /// <summary>
        /// 判断某 Entity 是否被选中（仅在 SelectionType 为 Entity 时匹配）
        /// 保留旧签名 IsSelected(UUID) 以兼容大量已有 Entity 语义的调用点
        /// </summary>
        static bool IsSelected(UUID entityID);

        // ---- 语义糖：Asset ----

        /// <summary>
        /// 选中 Asset
        /// </summary>
        static void SelectAsset(AssetHandle handle);

        /// <summary>
        /// 判断某 Asset 是否被选中（仅在 SelectionType 为 Asset 时匹配）
        /// </summary>
        static bool IsAssetSelected(AssetHandle handle);

        // ---- 语义糖：Folder ----

        /// <summary>
        /// 选中目录（Payload 存于 s_SelectionPath，s_SelectionID 置零）
        /// </summary>
        static void SelectFolder(const std::filesystem::path& path);

        /// <summary>
        /// 判断某目录是否被选中（仅在 SelectionType 为 Folder 时匹配）
        /// </summary>
        static bool IsFolderSelected(const std::filesystem::path& path);

        /// <summary>
        /// 获取当前选中的目录路径（若 SelectionType 非 Folder，返回空 path）
        /// </summary>
        static const std::filesystem::path& GetSelectedFolder();
    };
}