#pragma once

#include "Lucky/Asset/AssetHandle.h"

namespace Lucky
{
    /// <summary>
    /// 材质资产 Inspector
    /// </summary>
    class MaterialInspector
    {
    public:
        static void Draw(AssetHandle handle);
    };

    /// <summary>
    /// 网格资产 Inspector
    /// </summary>
    class MeshInspector
    {
    public:
        static void Draw(AssetHandle handle);
    };

    /// <summary>
    /// 2D 纹理资产 Inspector
    /// </summary>
    class Texture2DInspector
    {
    public:
        static void Draw(AssetHandle handle);
    };

    /// <summary>
    /// 场景资产 Inspector
    /// </summary>
    class SceneInspector
    {
    public:
        static void Draw(AssetHandle handle);
    };

    /// <summary>
    /// Shader 资产 Inspector
    /// </summary>
    class ShaderInspector
    {
    public:
        static void Draw(AssetHandle handle);
    };
}
