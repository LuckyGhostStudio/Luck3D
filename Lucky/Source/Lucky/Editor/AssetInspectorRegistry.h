#pragma once

#include "Lucky/Asset/AssetHandle.h"
#include "Lucky/Asset/AssetType.h"

#include <functional>
#include <unordered_map>

namespace Lucky
{
    /// <summary>
    /// 资产 Inspector 注册表：按 AssetType 分发资产的检视面板绘制
    /// 
    /// 用法：
    /// 1. 在编辑器启动时注册各 AssetType 的绘制函数：
    ///    AssetInspectorRegistry::Register(AssetType::Material, &MaterialInspector::Draw);
    /// 2. InspectorPanel 检测到当前选中的是 Asset 时调用 Draw(handle) 完成分发
    /// 
    /// </summary>
    class AssetInspectorRegistry
    {
    public:
        using DrawFn = std::function<void(AssetHandle)>;

        /// <summary>
        /// 注册某种 AssetType 的 Inspector 绘制函数
        /// 后注册者覆盖先注册者
        /// </summary>
        static void Register(AssetType type, DrawFn fn);

        /// <summary>
        /// 注销某种 AssetType 的绘制函数
        /// </summary>
        static void Unregister(AssetType type);

        /// <summary>
        /// 分发绘制：根据 handle 对应的 AssetType 查表并调用对应绘制函数
        /// </summary>
        static void Draw(AssetHandle handle);

        /// <summary>
        /// 清空所有注册（编辑器关闭时可选调用）
        /// </summary>
        static void Clear();
    };
}
