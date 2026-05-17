#pragma once

#include <string>

namespace Lucky
{
    /// <summary>
    /// 资产类型枚举
    /// </summary>
    enum class AssetType : uint8_t
    {
        None = 0,       // 无效类型
        Material,       // 材质（.lmat）
        Mesh,           // 网格/模型（.obj/.fbx/.gltf/.glb/.dae/.3ds/.blend）
        Texture2D,      // 2D 纹理（.png/.jpg/.tga/.bmp/.hdr）
        Scene,          // 场景（.luck3d）
        Shader          // 着色器（预留）
    };

    /// <summary>
    /// AssetType 转字符串
    /// </summary>
    inline const char* AssetTypeToString(AssetType type)
    {
        switch (type)
        {
            case AssetType::Material:   return "Material";
            case AssetType::Mesh:       return "Mesh";
            case AssetType::Texture2D:  return "Texture2D";
            case AssetType::Scene:      return "Scene";
            case AssetType::Shader:     return "Shader";
            default:                    return "None";
        }
    }

    /// <summary>
    /// 字符串转 AssetType
    /// </summary>
    inline AssetType StringToAssetType(const std::string& str)
    {
        if (str == "Material")  return AssetType::Material;
        if (str == "Mesh")      return AssetType::Mesh;
        if (str == "Texture2D") return AssetType::Texture2D;
        if (str == "Scene")     return AssetType::Scene;
        if (str == "Shader")    return AssetType::Shader;
        return AssetType::None;
    }

    /// <summary>
    /// 根据文件扩展名推断资产类型
    /// </summary>
    /// <param name="extension">文件扩展名（含点号，如 ".lmat"、".obj"）</param>
    /// <returns>推断的资产类型，未知扩展名返回 None</returns>
    inline AssetType GetAssetTypeFromExtension(const std::string& extension)
    {
        // 材质
        if (extension == ".lmat")
        {
            return AssetType::Material;
        }

        // 模型
        if (extension == ".obj" || extension == ".fbx" ||
            extension == ".gltf" || extension == ".glb" ||
            extension == ".dae" || extension == ".3ds" ||
            extension == ".blend")
        {
            return AssetType::Mesh;
        }

        // 纹理
        if (extension == ".png" || extension == ".jpg" ||
            extension == ".jpeg" || extension == ".tga" ||
            extension == ".bmp" || extension == ".hdr")
        {
            return AssetType::Texture2D;
        }

        // 场景
        if (extension == ".luck3d")
        {
            return AssetType::Scene;
        }

        // Shader
        if (extension == ".vert" || extension == ".frag")
        {
            return AssetType::Shader;
        }

        return AssetType::None;
    }
}
