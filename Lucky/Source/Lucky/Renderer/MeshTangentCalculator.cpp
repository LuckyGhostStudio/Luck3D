#include "lcpch.h"
#include "MeshTangentCalculator.h"

namespace Lucky
{
    void MeshTangentCalculator::Calculate(std::vector<Vertex>& vertices, const std::vector<uint32_t>& indices)
    {
        if (vertices.empty() || indices.empty())
        {
            return;
        }
        
        // 临时存储每个顶点的累积 Tangent 和 Bitangent
        std::vector<glm::vec3> tangents(vertices.size(), glm::vec3(0.0f));
        std::vector<glm::vec3> bitangents(vertices.size(), glm::vec3(0.0f));
        
        // 1. 遍历每个三角形，计算面切线和面副切线
        for (size_t i = 0; i + 2 < indices.size(); i += 3)
        {
            uint32_t i0 = indices[i];
            uint32_t i1 = indices[i + 1];
            uint32_t i2 = indices[i + 2];
            
            // 边界检查
            if (i0 >= vertices.size() || i1 >= vertices.size() || i2 >= vertices.size())
            {
                continue;
            }
            
            const glm::vec3& p0 = vertices[i0].Position;
            const glm::vec3& p1 = vertices[i1].Position;
            const glm::vec3& p2 = vertices[i2].Position;
            
            const glm::vec2& uv0 = vertices[i0].TexCoord;
            const glm::vec2& uv1 = vertices[i1].TexCoord;
            const glm::vec2& uv2 = vertices[i2].TexCoord;
            
            glm::vec3 edge1 = p1 - p0;
            glm::vec3 edge2 = p2 - p0;
            
            float du1 = uv1.x - uv0.x;
            float dv1 = uv1.y - uv0.y;
            float du2 = uv2.x - uv0.x;
            float dv2 = uv2.y - uv0.y;
            
            float det = du1 * dv2 - du2 * dv1;
            
            // 避免除以零（退化三角形或退化 UV）
            if (std::abs(det) < 1e-8f)
            {
                continue;
            }
            
            float f = 1.0f / det;
            
            glm::vec3 tangent = f * (dv2 * edge1 - dv1 * edge2);
            glm::vec3 bitangent = f * (-du2 * edge1 + du1 * edge2);
            
            // 2. 累积到三个顶点（面积加权，tangent 长度与三角形面积成正比）
            tangents[i0] += tangent;
            tangents[i1] += tangent;
            tangents[i2] += tangent;
            
            bitangents[i0] += bitangent;
            bitangents[i1] += bitangent;
            bitangents[i2] += bitangent;
        }
        
        // 3 & 4. 正交化并计算 handedness
        for (size_t i = 0; i < vertices.size(); ++i)
        {
            const glm::vec3& n = vertices[i].Normal;
            const glm::vec3& t = tangents[i];
            const glm::vec3& b = bitangents[i];
            
            // Gram-Schmidt 正交化：T' = normalize(T - N * dot(N, T))
            glm::vec3 tangentOrtho = t - n * glm::dot(n, t);
            
            float len = glm::length(tangentOrtho);
            if (len < 1e-8f)
            {
                // 退化情况：生成一个任意的切线（与法线垂直）
                tangentOrtho = (std::abs(n.x) < 0.9f) 
                    ? glm::normalize(glm::cross(n, glm::vec3(1.0f, 0.0f, 0.0f)))
                    : glm::normalize(glm::cross(n, glm::vec3(0.0f, 1.0f, 0.0f)));
                
                //vertices[i].Tangent = glm::vec4(tangentOrtho, 1.0f);
                
                continue;
            }
            
            tangentOrtho = glm::normalize(tangentOrtho);
            
            // Handedness：判断 Bitangent 方向是否与 cross(N, T) 同向
            float handedness = (glm::dot(glm::cross(n, tangentOrtho), b) < 0.0f) ? -1.0f : 1.0f;
            
            //vertices[i].Tangent = glm::vec4(tangentOrtho, handedness);
        }
    }
}