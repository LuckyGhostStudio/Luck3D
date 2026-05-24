#include "lcpch.h"
#include "MeshSerializer.h"

#include <fstream>
#include <filesystem>

namespace Lucky
{
    bool MeshSerializer::Serialize(const Ref<Mesh>& mesh, const std::string& filepath)
    {
        if (!mesh)
        {
            LF_CORE_ERROR("MeshSerializer::Serialize - Mesh is null!");
            return false;
        }

        // 确保目录存在
        std::filesystem::path path(filepath);
        if (path.has_parent_path())
        {
            std::filesystem::create_directories(path.parent_path());
        }

        std::ofstream file(filepath, std::ios::binary);
        if (!file.is_open())
        {
            LF_CORE_ERROR("MeshSerializer::Serialize - Failed to open file: {0}", filepath);
            return false;
        }

        const auto& vertices = mesh->GetVertices();
        const auto& indices = mesh->GetIndices();
        const auto& subMeshes = mesh->GetSubMeshes();
        const std::string& name = mesh->GetName();

        // 写入 Header
        LMeshHeader header;
        header.VertexCount = mesh->GetVertexCount();
        header.IndexCount = mesh->GetVertexIndexCount();
        header.SubMeshCount = mesh->GetSubMeshCount();
        header.NameLength = static_cast<uint32_t>(name.size());

        file.write(reinterpret_cast<const char*>(&header), sizeof(LMeshHeader));

        // 写入名称
        file.write(name.data(), name.size());

        // 写入顶点数据
        file.write(reinterpret_cast<const char*>(vertices.data()), vertices.size() * sizeof(Vertex));

        // 写入索引数据
        file.write(reinterpret_cast<const char*>(indices.data()), indices.size() * sizeof(uint32_t));

        // 写入 SubMesh 数据
        for (const auto& subMesh : subMeshes)
        {
            file.write(reinterpret_cast<const char*>(&subMesh), sizeof(SubMesh));
        }

        file.close();

        LF_CORE_INFO("MeshSerializer: Saved mesh '{0}' to '{1}' ({2} vertices, {3} indices, {4} submeshes)",
            name, filepath, header.VertexCount, header.IndexCount, header.SubMeshCount);
        return true;
    }

    Ref<Mesh> MeshSerializer::Deserialize(const std::string& filepath)
    {
        if (!std::filesystem::exists(filepath))
        {
            LF_CORE_ERROR("MeshSerializer::Deserialize - File not found: {0}", filepath);
            return nullptr;
        }

        std::ifstream file(filepath, std::ios::binary);
        if (!file.is_open())
        {
            LF_CORE_ERROR("MeshSerializer::Deserialize - Failed to open file: {0}", filepath);
            return nullptr;
        }

        // 读取 Header
        LMeshHeader header;
        file.read(reinterpret_cast<char*>(&header), sizeof(LMeshHeader));

        // 验证魔数
        if (header.Magic[0] != 'L' || header.Magic[1] != 'M' ||
            header.Magic[2] != 'S' || header.Magic[3] != 'H')
        {
            LF_CORE_ERROR("MeshSerializer::Deserialize - Invalid file format: {0}", filepath);
            return nullptr;
        }

        // 验证版本
        if (header.Version != 1)
        {
            LF_CORE_ERROR("MeshSerializer::Deserialize - Unsupported version {0}: {1}", header.Version, filepath);
            return nullptr;
        }

        // 读取名称
        std::string name(header.NameLength, '\0');
        file.read(name.data(), header.NameLength);

        // 读取顶点数据
        std::vector<Vertex> vertices(header.VertexCount);
        file.read(reinterpret_cast<char*>(vertices.data()), header.VertexCount * sizeof(Vertex));

        // 读取索引数据
        std::vector<uint32_t> indices(header.IndexCount);
        file.read(reinterpret_cast<char*>(indices.data()), header.IndexCount * sizeof(uint32_t));

        // 读取 SubMesh 数据
        std::vector<SubMesh> subMeshes(header.SubMeshCount);
        for (uint32_t i = 0; i < header.SubMeshCount; ++i)
        {
            file.read(reinterpret_cast<char*>(&subMeshes[i]), sizeof(SubMesh));
        }

        file.close();

        // 创建 Mesh
        Ref<Mesh> mesh = CreateRef<Mesh>(vertices, indices, subMeshes);
        mesh->SetName(name);

        LF_CORE_INFO("MeshSerializer: Loaded mesh '{0}' from '{1}' ({2} vertices, {3} indices)", name, filepath, header.VertexCount, header.IndexCount);
        return mesh;
    }
}
