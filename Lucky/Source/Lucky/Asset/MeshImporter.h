#pragma once

#include "Lucky/Core/Base.h"
#include "Lucky/Renderer/Mesh.h"
#include "Lucky/Renderer/Material.h"

#include <string>
#include <vector>

namespace Lucky
{
    /// <summary>
    /// 导入结果
    /// </summary>
    struct MeshImportResult
    {
        Ref<Mesh> MeshData;                     // 导入的网格（包含所有 SubMesh）
        std::vector<Ref<Material>> Materials;   // 材质列表（索引对应 SubMesh::MaterialIndex）
        std::string FilePath;                   // 源文件路径
        bool Success = false;                   // 是否导入成功
        std::string ErrorMessage;               // 错误信息
    };
    
    /// <summary>
    /// 导入选项
    /// </summary>
    struct MeshImportOptions
    {
        bool CalculateNormals = true;   // 如果模型缺少法线，自动计算
        bool CalculateTangents = true;  // 如果模型缺少切线，自动计算
        bool FlipUVs = true;            // 翻转 UV 的 Y 轴（OpenGL 纹理坐标）
        bool Triangulate = true;        // 将多边形三角化
        float ScaleFactor = 1.0f;       // 缩放因子
    };
    
    /// <summary>
    /// 模型导入器：使用 Assimp 解析 3D 模型文件
    /// 支持格式：.obj, .fbx, .gltf, .glb, .dae, .3ds, .blend 等
    /// </summary>
    class MeshImporter
    {
    public:
        /// <summary>
        /// 从文件导入模型
        /// </summary>
        /// <param name="filepath">模型文件路径</param>
        /// <param name="options">导入选项</param>
        /// <returns>导入结果</returns>
        static MeshImportResult Import(const std::string& filepath, const MeshImportOptions& options = {});
        
        /// <summary>
        /// 检查文件格式是否支持
        /// </summary>
        /// <param name="filepath">文件路径</param>
        /// <returns>是否支持</returns>
        static bool IsFormatSupported(const std::string& filepath);
        
        /// <summary>
        /// 获取支持的文件格式列表
        /// </summary>
        /// <returns>格式列表（如 ".obj", ".fbx" 等）</returns>
        //static std::vector<std::string> GetSupportedFormats();
    };
}