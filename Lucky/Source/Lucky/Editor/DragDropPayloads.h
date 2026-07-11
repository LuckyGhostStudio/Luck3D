#pragma once

namespace Lucky::DragDrop
{
    /// 资产拖拽：Payload 数据为 AssetHandle（uint64_t）
    /// 用途：从 ProjectAssetsPanel 拖拽资产到 Inspector 属性槽位、Viewport 等
    constexpr const char* AssetHandle = "ASSET_HANDLE";

    /// 实体拖拽：Payload 数据为 UUID（uint64_t）
    /// 用途：Hierarchy 面板中拖拽实体节点进行排序/重设父子关系
    constexpr const char* EntityHierarchy = "ENTITY_HIERARCHY";
}
