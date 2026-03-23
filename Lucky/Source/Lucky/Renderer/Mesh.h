#pragma once

#include <glm/glm.hpp>

#include "Buffer.h"
#include "VertexArray.h"

namespace Lucky
{
    struct Vertex
    {
        glm::vec3 Position; // 位置
        glm::vec4 Color;    // 颜色
        glm::vec3 Normal;   // 法线
        glm::vec2 TexCoord; // 纹理坐标
    };
    
    // struct Vertex
    // {
    //     glm::vec3 Position;
    //     glm::vec3 Normal;
    //     glm::vec3 Tangent;
    //     glm::vec3 Binormal;
    //     glm::vec2 Texcoord;
    // };
    
    struct SubMesh
    {
        uint32_t IndexOffset;   // 在 indices 中的偏移
        uint32_t IndexCount;    // 索引数量
        uint32_t VertexCount;   // 顶点数量
        uint32_t MaterialIndex; // 材质索引
    };
    
    class Mesh
    {
    public:
        Mesh() = default;
        Mesh(const Mesh&) = default;
        Mesh(const std::vector<Vertex>& vertices, const std::vector<uint32_t>& indices);
        Mesh(const std::vector<Vertex>& vertices, const std::vector<uint32_t>& indices, const std::vector<SubMesh>& subMeshes);
        
        /// <summary>
        /// 添加子网格
        /// </summary>
        /// <param name="subMesh">要添加的子网格</param>
        /// <returns>新添加子网格的索引</returns>
        uint32_t AddSubMesh(const SubMesh& subMesh);
        
        /// <summary>
        /// 添加子网格
        /// </summary>
        /// <param name="indexOffset">索引偏移</param>
        /// <param name="indexCount">索引数量</param>
        /// <param name="vertexCount">顶点数量</param>
        /// <param name="materialIndex">材质索引</param>
        /// <returns>新添加子网格的索引</returns>
        uint32_t AddSubMesh(uint32_t indexOffset, uint32_t indexCount, uint32_t vertexCount, uint32_t materialIndex);
        
        /// <summary>
        /// 清空所有子网格信息（重置为单个网格状态）
        /// </summary>
        void ClearSubMeshes();
        
        /// <summary>
        /// 获取指定索引的子网格
        /// </summary>
        /// <param name="index">子网格索引</param>
        /// <returns>指定的 SubMesh，若索引无效返回默认值</returns>
        SubMesh GetSubMesh(uint32_t index) const;
        
        /// <summary>
        /// 更新指定索引的子网格信息
        /// </summary>
        /// <param name="index">子网格索引</param>
        /// <param name="subMesh">新的子网格数据</param>
        /// <returns>成功更新 true 索引无效 false</returns>
        bool UpdateSubMesh(uint32_t index, const SubMesh& subMesh);
        
        const std::vector<Vertex>& GetVertices() const { return m_Vertices; }
        void SetVertexBufferData(const void* data, uint32_t size);
        
        const Ref<VertexArray>& GetVertexArray() const { return m_VertexArray; }
        
        uint32_t GetVertexCount() const { return m_VertexCount; }
        uint32_t GetVertexIndexCount() const { return m_VertexIndexCount; }
        uint32_t GetSubMeshCount() const { return m_SubMeshCount; }
        
        const std::vector<SubMesh>& GetSubMeshes() const { return m_SubMeshes; }
        std::vector<SubMesh>& GetSubMeshes() { return m_SubMeshes; }
    private:
        std::vector<Vertex> m_Vertices;			// 顶点列表
        std::vector<uint32_t> m_VertexIndices;	// 顶点索引列表 
        
        Ref<VertexArray> m_VertexArray;		// 顶点数组 VAO
        Ref<VertexBuffer> m_VertexBuffer;	// 顶点缓冲区 VBO
        Ref<IndexBuffer> m_IndexBuffer;	    // 索引缓冲区 VBO
        
        uint32_t m_VertexCount = 0;			// 顶点个数
        uint32_t m_VertexIndexCount = 0;	// 顶点索引个数
        uint32_t m_SubMeshCount = 0;		// 子网格数
        
        std::vector<SubMesh> m_SubMeshes;   // 子网格列表
    };
}
