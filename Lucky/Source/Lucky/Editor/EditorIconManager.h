#pragma once

#include "Lucky/Core/Base.h"
#include "Lucky/Renderer/Texture.h"
#include "Lucky/Asset/AssetType.h"
#include "Lucky/Scene/Components/ComponentType.h"

#include <string>
#include <unordered_map>

namespace Lucky
{
    /// <summary>
    /// 编辑器图标管理器
    /// 管理所有固定图标（纹理图标）的加载和获取
    /// </summary>
    class EditorIconManager
    {
    public:
        /// <summary>
        /// 初始化：加载所有图标纹理
        /// </summary>
        static void Init();

        /// <summary>
        /// 销毁：释放所有图标纹理
        /// </summary>
        static void Shutdown();

        // ======== 资产类型图标 ========

        /// <summary>
        /// 获取资产类型图标
        /// </summary>
        /// <param name="type">资产类型</param>
        /// <returns>对应的图标纹理，未知类型返回通用文件图标</returns>
        static const Ref<Texture2D>& GetAssetTypeIcon(AssetType type);

        // ======== 组件图标 ========

        /// <summary>
        /// 获取组件图标（通过组件类型枚举）
        /// </summary>
        /// <param name="type">组件类型</param>
        /// <returns>对应的图标纹理，未知组件返回 nullptr</returns>
        static const Ref<Texture2D>& GetComponentIcon(ComponentType type);

        // ======== 实体图标 ========

        /// <summary>
        /// 获取实体图标
        /// </summary>
        /// <returns>实体图标纹理</returns>
        static const Ref<Texture2D>& GetEntityIcon();

        // ======== 通用图标 ========

        /// <summary>
        /// 获取文件夹图标
        /// </summary>
        /// <param name="isOpen">是否为打开状态</param>
        /// <returns>文件夹图标纹理</returns>
        static const Ref<Texture2D>& GetFolderIcon(bool isOpen = false);

        /// <summary>
        /// 获取通用文件图标
        /// </summary>
        /// <returns>文件图标纹理</returns>
        static const Ref<Texture2D>& GetFileIcon();
    };
}
