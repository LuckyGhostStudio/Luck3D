#pragma once

#include "Lucky/Renderer/Mesh.h"

namespace Lucky
{
    /// <summary>
    /// .lmesh 文件头
    /// </summary>
    struct LMeshHeader
    {
        char Magic[4] = { 'L', 'M', 'S', 'H' };   // 魔数
        uint32_t Version = 1;                       // 格式版本
        uint32_t VertexCount = 0;                   // 顶点数量
        uint32_t IndexCount = 0;                    // 索引数量
        uint32_t SubMeshCount = 0;                  // 子网格数量
        uint32_t NameLength = 0;                    // 名称长度（字节）
        uint32_t Flags = 0;                         // 预留标志位
    };

    /// <summary>
    /// Mesh 序列化器：负责 .lmesh 二进制文件的读写
    /// </summary>
    class MeshSerializer
    {
    public:
        /// <summary>
        /// 将 Mesh 序列化到 .lmesh 文件
        /// </summary>
        /// <param name="mesh">要序列化的 Mesh</param>
        /// <param name="filepath">输出文件路径</param>
        /// <returns>是否成功</returns>
        static bool Serialize(const Ref<Mesh>& mesh, const std::string& filepath);

        /// <summary>
        /// 从 .lmesh 文件反序列化 Mesh
        /// </summary>
        /// <param name="filepath">文件路径</param>
        /// <returns>反序列化的 Mesh（失败返回 nullptr）</returns>
        static Ref<Mesh> Deserialize(const std::string& filepath);
    };
}
