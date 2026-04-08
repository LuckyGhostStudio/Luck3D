#include "lcpch.h"
#include "Mesh.h"

#include "MeshTangentCalculator.h"

namespace Lucky
{
    Mesh::Mesh(const std::vector<Vertex>& vertices, const std::vector<uint32_t>& indices) 
        : m_Vertices(vertices),
        m_VertexIndices(indices)
    {
        m_VertexCount = static_cast<uint32_t>(m_Vertices.size());
        m_VertexIndexCount = static_cast<uint32_t>(m_VertexIndices.size());
        
        SubMesh defaultSubMesh;
        defaultSubMesh.IndexOffset = 0;
        defaultSubMesh.IndexCount = m_VertexIndexCount;
        defaultSubMesh.VertexCount = m_VertexCount;
        defaultSubMesh.MaterialIndex = 0;
        m_SubMeshes.push_back(defaultSubMesh);  // 添加默认子网格，包含所有顶点和索引，材质索引为 0
        m_SubMeshCount = 1;

        m_VertexArray = VertexArray::Create();                                  // 创建顶点数组 VAO
        m_VertexBuffer = VertexBuffer::Create(m_VertexCount * sizeof(Vertex));  // 创建顶点缓冲 VBO

        // 设置顶点缓冲区布局
        m_VertexBuffer->SetLayout({
            { ShaderDataType::Float3, "a_Position" },   // 位置
            { ShaderDataType::Float4, "a_Color" },      // 颜色
            { ShaderDataType::Float3, "a_Normal" },     // 法线
            { ShaderDataType::Float2, "a_TexCoord" },   // 纹理坐标
            { ShaderDataType::Float4, "a_Tangent" },    // 切线 + 手性
        });
        m_VertexArray->AddVertexBuffer(m_VertexBuffer);    // 添加 VBO 到 VAO

        m_IndexBuffer = IndexBuffer::Create(m_VertexIndices.data(), m_VertexIndexCount);    // 创建索引缓冲 EBO
        m_VertexArray->SetIndexBuffer(m_IndexBuffer);                                       // 设置 EBO 到 VAO
    }
    
    Mesh::Mesh(const std::vector<Vertex>& vertices, const std::vector<uint32_t>& indices, const std::vector<SubMesh>& subMeshes)
        : m_Vertices(vertices),
        m_VertexIndices(indices),
        m_SubMeshes(subMeshes)
    {
        m_VertexCount = static_cast<uint32_t>(m_Vertices.size());
        m_VertexIndexCount = static_cast<uint32_t>(m_VertexIndices.size());
        m_SubMeshCount = static_cast<uint32_t>(m_SubMeshes.size());

        m_VertexArray = VertexArray::Create();                                  // 创建顶点数组 VAO
        m_VertexBuffer = VertexBuffer::Create(m_VertexCount * sizeof(Vertex));  // 创建顶点缓冲 VBO

        // 设置顶点缓冲区布局
        m_VertexBuffer->SetLayout({
            { ShaderDataType::Float3, "a_Position" },   // location 0 位置
            { ShaderDataType::Float4, "a_Color" },      // location 1 颜色
            { ShaderDataType::Float3, "a_Normal" },     // location 2 法线
            { ShaderDataType::Float2, "a_TexCoord" },   // location 3 纹理坐标
            { ShaderDataType::Float4, "a_Tangent" },    // location 4 切线 + 手性
        });
        m_VertexArray->AddVertexBuffer(m_VertexBuffer); // 添加 VBO 到 VAO

        m_IndexBuffer = IndexBuffer::Create(m_VertexIndices.data(), m_VertexIndexCount);    // 创建索引缓冲 EBO
        m_VertexArray->SetIndexBuffer(m_IndexBuffer);                                       // 设置 EBO 到 VAO
    }

    void Mesh::RecalculateTangents()
    {
        if (m_Vertices.empty() || m_VertexIndices.empty())
        {
            return;
        }
    
        MeshTangentCalculator::Calculate(m_Vertices, m_VertexIndices);
    
        // 更新 GPU 缓冲区
        uint32_t dataSize = m_VertexCount * sizeof(Vertex);
        m_VertexBuffer->SetData(m_Vertices.data(), dataSize);
    }

    uint32_t Mesh::AddSubMesh(const SubMesh& subMesh)
    {
        // 索引越界
        if (subMesh.IndexOffset + subMesh.IndexCount > m_VertexIndexCount)
        {
            LF_CORE_WARN("AddSubMesh: Index out of range! IndexOffset = {0}, IndexCount = {1}, TotalIndexCount = {2}", subMesh.IndexOffset, subMesh.IndexCount, m_VertexIndexCount);
            return UINT32_MAX;  // 返回无效索引
        }

        // 顶点个数为 0
        if (subMesh.VertexCount == 0)
        {
            LF_CORE_WARN("AddSubMesh: VertexCount = 0");
            return UINT32_MAX;
        }

        // 添加子网格到列表
        m_SubMeshes.push_back(subMesh);
        m_SubMeshCount = static_cast<uint32_t>(m_SubMeshes.size());

        LF_CORE_INFO("SubMesh added #{0}, IndexOffset = {1}, IndexCount = {2}, VertexCount = {3}, MaterialIndex = {4}", m_SubMeshCount - 1, subMesh.IndexOffset, subMesh.IndexCount, subMesh.VertexCount, subMesh.MaterialIndex);

        return m_SubMeshCount - 1;  // 新子网格的索引
    }
    
    uint32_t Mesh::AddSubMesh(uint32_t indexOffset, uint32_t indexCount, uint32_t vertexCount, uint32_t materialIndex)
    {
        SubMesh newSubMesh;
        newSubMesh.IndexOffset = indexOffset;
        newSubMesh.IndexCount = indexCount;
        newSubMesh.VertexCount = vertexCount;
        newSubMesh.MaterialIndex = materialIndex;

        return AddSubMesh(newSubMesh);
    }

    // 清空所有子网格
    void Mesh::ClearSubMeshes()
    {
        m_SubMeshes.clear();
        m_SubMeshCount = 0;
    }

    // 获取指定索引的子网格
    SubMesh Mesh::GetSubMesh(uint32_t index) const
    {
        if (index < m_SubMeshes.size())
        {
            return m_SubMeshes[index];
        }

        LF_CORE_WARN("GetSubMesh: Index {0} out of range, valid range [0, {1})", index, m_SubMeshes.size());
        
        return {};
    }
    
    bool Mesh::UpdateSubMesh(uint32_t index, const SubMesh& subMesh)
    {
        if (index >= m_SubMeshes.size())
        {
            LF_CORE_WARN("UpdateSubMesh: Index {0} out of range, valid range [0, {1})", index, m_SubMeshes.size());
            return false;
        }

        // 索引越界
        if (subMesh.IndexOffset + subMesh.IndexCount > m_VertexIndexCount)
        {
            LF_CORE_WARN("AddSubMesh: Index out of range! IndexOffset = {0}, IndexCount = {1}, TotalIndexCount = {2}", subMesh.IndexOffset, subMesh.IndexCount, m_VertexIndexCount);
            return false;  // 返回无效索引
        }

        // 顶点个数为 0
        if (subMesh.VertexCount == 0)
        {
            LF_CORE_WARN("AddSubMesh: VertexCount = 0");
            return false;
        }

        m_SubMeshes[index] = subMesh;

        LF_CORE_INFO("SubMesh updated #{0}, IndexOffset = {1}, IndexCount = {2}, VertexCount = {3}, MaterialIndex = {4}", m_SubMeshCount - 1, subMesh.IndexOffset, subMesh.IndexCount, subMesh.VertexCount, subMesh.MaterialIndex);

        return true;
    }

    void Mesh::SetVertexBufferData(const void* data, uint32_t size)
    {
        m_VertexBuffer->SetData(data, size);
    }
}
