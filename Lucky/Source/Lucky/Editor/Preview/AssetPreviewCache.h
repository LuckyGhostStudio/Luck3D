#pragma once

#include "Lucky/Core/Base.h"
#include "Lucky/Asset/AssetHandle.h"
#include "Lucky/Asset/AssetType.h"
#include "Lucky/Renderer/Texture.h"

namespace Lucky
{
    /// <summary>
    /// 资产预览缓存：按 AssetHandle 存放独立的 Ref<Texture2D>
    /// miss 时调用 AssetPreviewRenderer 渲染一次，并把像素 Blit 到自己持有的真实 Texture2D 上，
    /// 这样每个 Entry 拥有独立 OpenGL 纹理，同屏多张缩略图互不覆盖
    /// </summary>
    class AssetPreviewCache
    {
    public:
        static void Init();
        static void Shutdown();

        /// <summary>
        /// 获取（或按需渲染）指定资产的预览纹理
        /// 仅支持 AssetType::Material / AssetType::Mesh，其他类型返回空 Ref
        /// </summary>
        /// <param name="handle">资产 Handle，无效时返回空 Ref</param>
        /// <param name="type">资产类型（由上层传入，避免重复查询 AssetManager）</param>
        /// <returns>独立预览纹理，可直接传给 ImGui::Image；不支持的类型或资产加载失败时返回空 Ref</returns>
        static const Ref<Texture2D>& GetOrRender(AssetHandle handle, AssetType type);

        /// <summary>
        /// 使指定资产的预览失效，下次 Get 时重新渲染并 Blit
        /// Material 保存 / Material 属性改动 / Mesh 替换 / 资产删除移动都应调用
        /// </summary>
        /// <param name="handle">资产 Handle</param>
        static void Invalidate(AssetHandle handle);

        /// <summary>
        /// 清空所有预览（Project 关闭 / 全局 Shutdown 时调用）
        /// </summary>
        static void Clear();
    };
}
