#include "lcpch.h"
#include "MeshFactory.h"

#include "glm/ext/scalar_constants.hpp"

namespace Lucky
{
    Ref<Mesh> MeshFactory::CreatePrimitive(PrimitiveType type)
    {
        Ref<Mesh> mesh = nullptr;
    
        switch (type)
        {
        case PrimitiveType::Cube:       mesh = CreateCube();        break;
        case PrimitiveType::Plane:      mesh = CreatePlane();       break;
        case PrimitiveType::Sphere:     mesh = CreateSphere();      break;
        case PrimitiveType::Cylinder:   mesh = CreateCylinder();    break;
        case PrimitiveType::Capsule:    mesh = CreateCapsule();     break;
        default:
            LF_CORE_WARN("MeshFactory::CreatePrimitive: Unknown PrimitiveType {0}", (int)type);
            return nullptr;
        }
    
        if (mesh)
        {
            mesh->SetName(GetPrimitiveTypeName(type));
        }
    
        return mesh;
    }
    
    Ref<Mesh> MeshFactory::CreateCube()
    {
        std::vector<Vertex> vertices =
        {
            // 右面 (X+)
            {{0.5f, -0.5f, 0.5f}, {1.0f, 1.0f, 1.0f, 1.0f}, {1.0f, 0.0f, 0.0f}, {1.0f, 0.0f}},  // 0: 右下前
            {{0.5f, 0.5f, 0.5f}, {1.0f, 1.0f, 1.0f, 1.0f}, {1.0f, 0.0f, 0.0f}, {1.0f, 1.0f}},   // 3: 右上前
            {{0.5f, 0.5f, -0.5f}, {1.0f, 1.0f, 1.0f, 1.0f}, {1.0f, 0.0f, 0.0f}, {0.0f, 1.0f}},  // 2: 右上后
            {{0.5f, -0.5f, -0.5f}, {1.0f, 1.0f, 1.0f, 1.0f}, {1.0f, 0.0f, 0.0f}, {0.0f, 0.0f}}, // 1: 右下后
            
            // 左面 (X-)
            {{-0.5f, -0.5f, -0.5f}, {1.0f, 1.0f, 1.0f, 1.0f}, {-1.0f, 0.0f, 0.0f}, {1.0f, 0.0f}}, // 4: 左下后
            {{-0.5f, 0.5f, -0.5f}, {1.0f, 1.0f, 1.0f, 1.0f}, {-1.0f, 0.0f, 0.0f}, {1.0f, 1.0f}},  // 7: 左上后
            {{-0.5f, 0.5f, 0.5f}, {1.0f, 1.0f, 1.0f, 1.0f}, {-1.0f, 0.0f, 0.0f}, {0.0f, 1.0f}},   // 6: 左上前
            {{-0.5f, -0.5f, 0.5f}, {1.0f, 1.0f, 1.0f, 1.0f}, {-1.0f, 0.0f, 0.0f}, {0.0f, 0.0f}},  // 5: 左下前
            
            // 上面 (Y+)
            {{-0.5f, 0.5f, 0.5f}, {1.0f, 1.0f, 1.0f, 1.0f}, {0.0f, 1.0f, 0.0f}, {0.0f, 1.0f}},
            {{0.5f, 0.5f, 0.5f}, {1.0f, 1.0f, 1.0f, 1.0f}, {0.0f, 1.0f, 0.0f}, {1.0f, 1.0f}},
            {{0.5f, 0.5f, -0.5f}, {1.0f, 1.0f, 1.0f, 1.0f}, {0.0f, 1.0f, 0.0f}, {1.0f, 0.0f}},
            {{-0.5f, 0.5f, -0.5f}, {1.0f, 1.0f, 1.0f, 1.0f}, {0.0f, 1.0f, 0.0f}, {0.0f, 0.0f}},
            
            // 下面 (Y-)
            {{-0.5f, -0.5f, -0.5f}, {1.0f, 1.0f, 1.0f, 1.0f}, {0.0f, -1.0f, 0.0f}, {1.0f, 1.0f}},
            {{0.5f, -0.5f, -0.5f}, {1.0f, 1.0f, 1.0f, 1.0f}, {0.0f, -1.0f, 0.0f}, {0.0f, 1.0f}},
            {{0.5f, -0.5f, 0.5f}, {1.0f, 1.0f, 1.0f, 1.0f}, {0.0f, -1.0f, 0.0f}, {0.0f, 0.0f}},
            {{-0.5f, -0.5f, 0.5f}, {1.0f, 1.0f, 1.0f, 1.0f}, {0.0f, -1.0f, 0.0f}, {1.0f, 0.0f}},
            
            // 前面 (Z+)
            {{-0.5f, -0.5f, 0.5f}, {1.0f, 1.0f, 1.0f, 1.0f}, {0.0f, 0.0f, 1.0f}, {0.0f, 0.0f}},
            {{0.5f, -0.5f, 0.5f}, {1.0f, 1.0f, 1.0f, 1.0f}, {0.0f, 0.0f, 1.0f}, {1.0f, 0.0f}},
            {{0.5f, 0.5f, 0.5f}, {1.0f, 1.0f, 1.0f, 1.0f}, {0.0f, 0.0f, 1.0f}, {1.0f, 1.0f}},
            {{-0.5f, 0.5f, 0.5f}, {1.0f, 1.0f, 1.0f, 1.0f}, {0.0f, 0.0f, 1.0f}, {0.0f, 1.0f}},
            
            // 后面 (Z-)
            {{0.5f, -0.5f, -0.5f}, {1.0f, 1.0f, 1.0f, 1.0f}, {0.0f, 0.0f, -1.0f}, {0.0f, 0.0f}},
            {{-0.5f, -0.5f, -0.5f}, {1.0f, 1.0f, 1.0f, 1.0f}, {0.0f, 0.0f, -1.0f}, {1.0f, 0.0f}},
            {{-0.5f, 0.5f, -0.5f}, {1.0f, 1.0f, 1.0f, 1.0f}, {0.0f, 0.0f, -1.0f}, {1.0f, 1.0f}},
            {{0.5f, 0.5f, -0.5f}, {1.0f, 1.0f, 1.0f, 1.0f}, {0.0f, 0.0f, -1.0f}, {0.0f, 1.0f}}
        };
        
        std::vector<uint32_t> indices =
        {
            0, 1, 2, 0, 2, 3,        // 右面
            4, 5, 6, 4, 6, 7,        // 左面
            8, 9, 10, 8, 10, 11,     // 上面
            12, 13, 14, 12, 14, 15,  // 下面
            16, 17, 18, 16, 18, 19,  // 前面
            20, 21, 22, 20, 22, 23   // 后面
        };
        
        return CreateRef<Mesh>(vertices, indices);
    }
    
    Ref<Mesh> MeshFactory::CreatePlane(uint32_t subdivisions)
    {
        // 参数校验
        subdivisions = std::max<uint32_t>(subdivisions, 1);
    
        uint32_t vertexCount = (subdivisions + 1) * (subdivisions + 1);
        uint32_t indexCount = subdivisions * subdivisions * 6;
    
        std::vector<Vertex> vertices;
        vertices.reserve(vertexCount);
    
        std::vector<uint32_t> indices;
        indices.reserve(indexCount);
    
        // 生成顶点
        for (uint32_t i = 0; i <= subdivisions; ++i)
        {
            for (uint32_t j = 0; j <= subdivisions; ++j)
            {
                float x = -0.5f + static_cast<float>(i) / static_cast<float>(subdivisions);
                float z = -0.5f + static_cast<float>(j) / static_cast<float>(subdivisions);
            
                float u = static_cast<float>(i) / static_cast<float>(subdivisions);
                float v = static_cast<float>(j) / static_cast<float>(subdivisions);
            
                vertices.push_back({
                    { x, 0.0f, z },             // Position
                    { 1.0f, 1.0f, 1.0f, 1.0f }, // Color（白色）
                    { 0.0f, 1.0f, 0.0f },       // Normal（朝上）
                    { u, v }                    // TexCoord
                });
            }
        }
    
        // 生成索引
        for (uint32_t i = 0; i < subdivisions; ++i)
        {
            for (uint32_t j = 0; j < subdivisions; ++j)
            {
                uint32_t topLeft = i * (subdivisions + 1) + j;
                uint32_t topRight = i * (subdivisions + 1) + (j + 1);
                uint32_t bottomLeft = (i + 1) * (subdivisions + 1) + j;
                uint32_t bottomRight = (i + 1) * (subdivisions + 1) + (j + 1);
            
                // 三角形 1
                indices.push_back(topLeft);
                indices.push_back(bottomLeft);
                indices.push_back(topRight);
            
                // 三角形 2
                indices.push_back(topRight);
                indices.push_back(bottomLeft);
                indices.push_back(bottomRight);
            }
        }
    
        return CreateRef<Mesh>(vertices, indices);
    }
    
    Ref<Mesh> MeshFactory::CreateSphere(uint32_t segments, uint32_t rings)
    {
        // 参数校验
        segments = std::max<uint32_t>(segments, 3);
        rings = std::max<uint32_t>(rings, 2);
        
        constexpr float radius = 0.5f;
        constexpr float PI = glm::pi<float>();
        
        uint32_t vertexCount = (segments + 1) * (rings + 1);
        uint32_t indexCount = segments * rings * 6;
        
        std::vector<Vertex> vertices;
        vertices.reserve(vertexCount);
        
        std::vector<uint32_t> indices;
        indices.reserve(indexCount);
        
        // 生成顶点
        for (uint32_t ring = 0; ring <= rings; ++ring)
        {
            float phi = PI * static_cast<float>(ring) / static_cast<float>(rings);  // [0, π]
            float y = radius * cos(phi);
            float ringRadius = radius * sin(phi);
            
            for (uint32_t seg = 0; seg <= segments; ++seg)
            {
                float theta = 2.0f * PI * static_cast<float>(seg) / static_cast<float>(segments);  // [0, 2π]
                float x = ringRadius * cos(theta);
                float z = ringRadius * sin(theta);
                
                // 法线 = 归一化的位置向量（球心在原点）
                glm::vec3 normal = glm::normalize(glm::vec3(x, y, z));
                
                float u = static_cast<float>(seg) / static_cast<float>(segments);
                float v = static_cast<float>(ring) / static_cast<float>(rings);
                
                vertices.push_back({
                    { x, y, z },                // Position
                    { 1.0f, 1.0f, 1.0f, 1.0f }, // Color
                    normal,                     // Normal
                    { u, v }                    // TexCoord
                });
            }
        }
        
        // 生成索引
        for (uint32_t ring = 0; ring < rings; ++ring)
        {
            for (uint32_t seg = 0; seg < segments; ++seg)
            {
                uint32_t current = ring * (segments + 1) + seg;
                uint32_t next = current + 1;
                uint32_t below = (ring + 1) * (segments + 1) + seg;
                uint32_t belowNext = below + 1;
                
                // 三角形 1
                indices.push_back(current);
                indices.push_back(below);
                indices.push_back(next);
                
                // 三角形 2
                indices.push_back(next);
                indices.push_back(below);
                indices.push_back(belowNext);
            }
        }
        
        return CreateRef<Mesh>(vertices, indices);
    }
    
    Ref<Mesh> MeshFactory::CreateCylinder(uint32_t segments)
    {
        // 参数校验
        segments = std::max<uint32_t>(segments, 3);
        
        constexpr float radius = 0.5f;
        constexpr float halfHeight = 0.5f;
        constexpr float PI = glm::pi<float>();
        
        std::vector<Vertex> vertices;
        std::vector<uint32_t> indices;
        
        // ========== 1. 侧面 ==========
        uint32_t sideBaseIndex = 0;
        
        for (uint32_t seg = 0; seg <= segments; ++seg)
        {
            float theta = 2.0f * PI * static_cast<float>(seg) / static_cast<float>(segments);
            float cosTheta = cos(theta);
            float sinTheta = sin(theta);
            
            float x = radius * cosTheta;
            float z = radius * sinTheta;
            
            glm::vec3 normal = { cosTheta, 0.0f, sinTheta };  // 水平向外
            
            float u = static_cast<float>(seg) / static_cast<float>(segments);
            
            // 上圈顶点
            vertices.push_back({
                { x, halfHeight, z },
                { 1.0f, 1.0f, 1.0f, 1.0f },
                normal,
                { u, 1.0f }
            });
            
            // 下圈顶点
            vertices.push_back({
                { x, -halfHeight, z },
                { 1.0f, 1.0f, 1.0f, 1.0f },
                normal,
                { u, 0.0f }
            });
        }
        
        // 侧面索引
        for (uint32_t seg = 0; seg < segments; ++seg)
        {
            uint32_t topLeft     = sideBaseIndex + seg * 2;
            uint32_t bottomLeft  = topLeft + 1;
            uint32_t topRight    = topLeft + 2;
            uint32_t bottomRight = topLeft + 3;
            
            // 三角形 1
            indices.push_back(topLeft);
            indices.push_back(bottomLeft);
            indices.push_back(topRight);
            
            // 三角形 2
            indices.push_back(topRight);
            indices.push_back(bottomLeft);
            indices.push_back(bottomRight);
        }
        
        // ========== 2. 顶面 ==========
        uint32_t topCapBaseIndex = static_cast<uint32_t>(vertices.size());
        
        // 中心点
        vertices.push_back({
            { 0.0f, halfHeight, 0.0f },
            { 1.0f, 1.0f, 1.0f, 1.0f },
            { 0.0f, 1.0f, 0.0f },       // 法线朝上
            { 0.5f, 0.5f }              // UV 中心
        });
        
        // 圆周顶点
        for (uint32_t seg = 0; seg <= segments; ++seg)
        {
            float theta = 2.0f * PI * static_cast<float>(seg) / static_cast<float>(segments);
            float cosTheta = cos(theta);
            float sinTheta = sin(theta);
            
            vertices.push_back({
                { radius * cosTheta, halfHeight, radius * sinTheta },
                { 1.0f, 1.0f, 1.0f, 1.0f },
                { 0.0f, 1.0f, 0.0f },                               // 法线朝上
                { cosTheta * 0.5f + 0.5f, sinTheta * 0.5f + 0.5f }  // 圆形 UV 映射
            });
        }
        
        // 顶面索引（扇形三角形）
        for (uint32_t seg = 0; seg < segments; ++seg)
        {
            indices.push_back(topCapBaseIndex); // 中心点
            indices.push_back(topCapBaseIndex + 1 + seg);
            indices.push_back(topCapBaseIndex + 2 + seg);
        }
        
        // ========== 3. 底面 ==========
        uint32_t bottomCapBaseIndex = static_cast<uint32_t>(vertices.size());
        
        // 中心点
        vertices.push_back({
            { 0.0f, -halfHeight, 0.0f },
            { 1.0f, 1.0f, 1.0f, 1.0f },
            { 0.0f, -1.0f, 0.0f },      // 法线朝下
            { 0.5f, 0.5f }              // UV 中心
        });
        
        // 圆周顶点
        for (uint32_t seg = 0; seg <= segments; ++seg)
        {
            float theta = 2.0f * PI * static_cast<float>(seg) / static_cast<float>(segments);
            float cosTheta = cos(theta);
            float sinTheta = sin(theta);
            
            vertices.push_back({
                { radius * cosTheta, -halfHeight, radius * sinTheta },
                { 1.0f, 1.0f, 1.0f, 1.0f },
                { 0.0f, -1.0f, 0.0f },                              // 法线朝下
                { cosTheta * 0.5f + 0.5f, sinTheta * 0.5f + 0.5f }  // 圆形 UV 映射
            });
        }
        
        // 底面索引（扇形三角形，绕序与顶面相反）
        for (uint32_t seg = 0; seg < segments; ++seg)
        {
            indices.push_back(bottomCapBaseIndex);           // 中心点
            indices.push_back(bottomCapBaseIndex + 2 + seg); // 注意：与顶面相反
            indices.push_back(bottomCapBaseIndex + 1 + seg);
        }
        
        return CreateRef<Mesh>(vertices, indices);
    }
    
    Ref<Mesh> MeshFactory::CreateCapsule(uint32_t segments, uint32_t rings)
    {
        // 参数校验
        segments = std::max<uint32_t>(segments, 3);
        rings = std::max<uint32_t>(rings, 2);
        
        constexpr float radius = 0.5f;
        constexpr float halfHeight = 0.5f;  // 圆柱部分半高
        constexpr float PI = glm::pi<float>();
        
        std::vector<Vertex> vertices;
        std::vector<uint32_t> indices;
        
        // ========== 1. 上半球 ==========
        // 从北极（ring=0）到赤道（ring=rings）
        for (uint32_t ring = 0; ring <= rings; ++ring)
        {
            // phi 从 0（北极）到 π/2（赤道）
            float phi = (PI / 2.0f) * static_cast<float>(ring) / static_cast<float>(rings);
            float y = radius * cos(phi) + halfHeight;   // 偏移到圆柱顶部之上
            float ringRadius = radius * sin(phi);
            
            for (uint32_t seg = 0; seg <= segments; ++seg)
            {
                float theta = 2.0f * PI * static_cast<float>(seg) / static_cast<float>(segments);
                float x = ringRadius * cos(theta);
                float z = ringRadius * sin(theta);
                
                // 法线 = 归一化的球面方向（相对于半球中心）
                glm::vec3 spherePos = { x, radius * cos(phi), z };
                glm::vec3 normal = glm::normalize(spherePos);
                
                float u = static_cast<float>(seg) / static_cast<float>(segments);
                // V 映射：北极 = 1.0，赤道 = 0.75（上半球占 UV 的上 1/4）
                float v = 1.0f - (static_cast<float>(ring) / static_cast<float>(rings)) * 0.25f;
                
                vertices.push_back({
                    { x, y, z },
                    { 1.0f, 1.0f, 1.0f, 1.0f },
                    normal,
                    { u, v }
                });
            }
        }
        
        // ========== 2. 下半球 ==========
        // 从赤道（ring=0）到南极（ring=rings）
        // 注意：赤道层与上半球的赤道层共享位置但需要独立顶点（UV 不同）
        for (uint32_t ring = 0; ring <= rings; ++ring)
        {
            // phi 从 π/2（赤道）到 π（南极）
            float phi = (PI / 2.0f) + (PI / 2.0f) * static_cast<float>(ring) / static_cast<float>(rings);
            float y = radius * cos(phi) - halfHeight;  // 偏移到圆柱底部之下
            float ringRadius = radius * sin(phi);
            
            for (uint32_t seg = 0; seg <= segments; ++seg)
            {
                float theta = 2.0f * PI * static_cast<float>(seg) / static_cast<float>(segments);
                float x = ringRadius * cos(theta);
                float z = ringRadius * sin(theta);
                
                glm::vec3 spherePos = { x, radius * cos(phi), z };
                glm::vec3 normal = glm::normalize(spherePos);
                
                float u = static_cast<float>(seg) / static_cast<float>(segments);
                // V 映射：赤道 = 0.25，南极 = 0.0（下半球占 UV 的下 1/4）
                float v = 0.25f - (static_cast<float>(ring) / static_cast<float>(rings)) * 0.25f;
                
                vertices.push_back({
                    { x, y, z },
                    { 1.0f, 1.0f, 1.0f, 1.0f },
                    normal,
                    { u, v }
                });
            }
        }
        
        // ========== 3. 生成索引 ==========
        // 上半球索引
        uint32_t topHemiOffset = 0;
        for (uint32_t ring = 0; ring < rings; ++ring)
        {
            for (uint32_t seg = 0; seg < segments; ++seg)
            {
                uint32_t current = topHemiOffset + ring * (segments + 1) + seg;
                uint32_t next = current + 1;
                uint32_t below = current + (segments + 1);
                uint32_t belowNext = below + 1;
                
                indices.push_back(current);
                indices.push_back(below);
                indices.push_back(next);
                
                indices.push_back(next);
                indices.push_back(below);
                indices.push_back(belowNext);
            }
        }
        
        // 中间连接带（上半球赤道层 → 下半球赤道层）
        uint32_t topEquatorOffset = topHemiOffset + rings * (segments + 1);
        uint32_t bottomHemiOffset = (rings + 1) * (segments + 1);
        uint32_t bottomEquatorOffset = bottomHemiOffset;
        
        for (uint32_t seg = 0; seg < segments; ++seg)
        {
            uint32_t topCurrent = topEquatorOffset + seg;
            uint32_t topNext = topCurrent + 1;
            uint32_t bottomCurrent = bottomEquatorOffset + seg;
            uint32_t bottomNext = bottomCurrent + 1;
            
            indices.push_back(topCurrent);
            indices.push_back(bottomCurrent);
            indices.push_back(topNext);
            
            indices.push_back(topNext);
            indices.push_back(bottomCurrent);
            indices.push_back(bottomNext);
        }
        
        // 下半球索引
        for (uint32_t ring = 0; ring < rings; ++ring)
        {
            for (uint32_t seg = 0; seg < segments; ++seg)
            {
                uint32_t current = bottomHemiOffset + ring * (segments + 1) + seg;
                uint32_t next = current + 1;
                uint32_t below = current + (segments + 1);
                uint32_t belowNext = below + 1;
                
                indices.push_back(current);
                indices.push_back(below);
                indices.push_back(next);
                
                indices.push_back(next);
                indices.push_back(below);
                indices.push_back(belowNext);
            }
        }
        
        return CreateRef<Mesh>(vertices, indices);
    }
    
    const char* MeshFactory::GetPrimitiveTypeName(PrimitiveType type)
    {
        switch (type)
        {
        case PrimitiveType::Cube:       return "Cube";
        case PrimitiveType::Plane:      return "Plane";
        case PrimitiveType::Sphere:     return "Sphere";
        case PrimitiveType::Cylinder:   return "Cylinder";
        case PrimitiveType::Capsule:    return "Capsule";
        default:                        return "Unknown";
        }
    }
}
