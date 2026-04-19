#include "lcpch.h"
#include "MeshImporter.h"

#include "Lucky/Renderer/Renderer3D.h"

#include <assimp/Importer.hpp>
#include <assimp/scene.h>
#include <assimp/postprocess.h>

namespace Lucky
{
    namespace
    {
        void ProcessMesh(const aiMesh* mesh, const aiScene* scene, std::vector<Vertex>& vertices, std::vector<uint32_t>& indices, std::vector<SubMesh>& subMeshes, const MeshImportOptions& options)
        {
            uint32_t vertexOffset = static_cast<uint32_t>(vertices.size());
            uint32_t indexOffset = static_cast<uint32_t>(indices.size());
        
            // 提取顶点
            for (unsigned int i = 0; i < mesh->mNumVertices; i++)
            {
                Vertex vertex;
            
                // 位置
                vertex.Position = {
                    mesh->mVertices[i].x * options.ScaleFactor,
                    mesh->mVertices[i].y * options.ScaleFactor,
                    mesh->mVertices[i].z * options.ScaleFactor
                };
            
                // 法线
                if (mesh->HasNormals())
                {
                    vertex.Normal = { mesh->mNormals[i].x, mesh->mNormals[i].y, mesh->mNormals[i].z };
                }
            
                // 纹理坐标（第一套）
                if (mesh->mTextureCoords[0])
                {
                    vertex.TexCoord = { mesh->mTextureCoords[0][i].x, mesh->mTextureCoords[0][i].y };
                }
                else
                {
                    vertex.TexCoord = { 0.0f, 0.0f };
                }
            
                // 切线
                if (mesh->HasTangentsAndBitangents())
                {
                    vertex.Tangent = {
                        mesh->mTangents[i].x,
                        mesh->mTangents[i].y,
                        mesh->mTangents[i].z,
                        1.0f                    // 手性（默认 1.0，后续可从 Bitangent 计算）
                    };
                }
            
                // 顶点颜色
                if (mesh->mColors[0])
                {
                    vertex.Color = {
                        mesh->mColors[0][i].r,
                        mesh->mColors[0][i].g,
                        mesh->mColors[0][i].b,
                        mesh->mColors[0][i].a
                    };
                }
                else
                {
                    vertex.Color = { 1.0f, 1.0f, 1.0f, 1.0f };
                }
            
                vertices.push_back(vertex);
            }
        
            // 提取索引
            for (unsigned int i = 0; i < mesh->mNumFaces; i++)
            {
                aiFace face = mesh->mFaces[i];
                for (unsigned int j = 0; j < face.mNumIndices; j++)
                {
                    indices.push_back(vertexOffset + face.mIndices[j]);
                }
            }
        
            // 创建 SubMesh
            SubMesh subMesh;
            //subMesh.Name = mesh->mName.C_Str();
            subMesh.IndexOffset = indexOffset;
            subMesh.IndexCount = static_cast<uint32_t>(indices.size()) - indexOffset;
            subMesh.MaterialIndex = mesh->mMaterialIndex;
            subMeshes.push_back(subMesh);
        }
    
        void ProcessNode(const aiNode* node, const aiScene* scene, std::vector<Vertex>& vertices, std::vector<uint32_t>& indices, std::vector<SubMesh>& subMeshes, const MeshImportOptions& options)
        {
            // 处理当前节点的所有 Mesh
            for (unsigned int i = 0; i < node->mNumMeshes; i++)
            {
                aiMesh* mesh = scene->mMeshes[node->mMeshes[i]];
                ProcessMesh(mesh, scene, vertices, indices, subMeshes, options);
            }
    
            // 递归处理子节点
            for (unsigned int i = 0; i < node->mNumChildren; i++)
            {
                ProcessNode(node->mChildren[i], scene, vertices, indices, subMeshes, options);
            }
        }
        
        void ProcessMaterials(const aiScene* scene, std::vector<Ref<Material>>& materials, const std::string& filepath)
        {
            std::string directory = filepath.substr(0, filepath.find_last_of("/\\"));
    
            for (unsigned int i = 0; i < scene->mNumMaterials; i++)
            {
                aiMaterial* aiMat = scene->mMaterials[i];
        
                // 创建默认 PBR 材质
                Ref<Material> material = Renderer3D::GetDefaultMaterial();  // 基于默认材质克隆
        
                // 提取材质名称
                aiString name;
                if (aiMat->Get(AI_MATKEY_NAME, name) == AI_SUCCESS)
                {
                    material->SetName(name.C_Str());
                }
        
                // 提取基础颜色
                aiColor4D baseColor;
                if (aiMat->Get(AI_MATKEY_BASE_COLOR, baseColor) == AI_SUCCESS)
                {
                    material->SetFloat4("u_Albedo", { baseColor.r, baseColor.g, baseColor.b, baseColor.a });
                }
                else
                {
                    aiColor3D diffuse;
                    if (aiMat->Get(AI_MATKEY_COLOR_DIFFUSE, diffuse) == AI_SUCCESS)
                    {
                        material->SetFloat4("u_Albedo", { diffuse.r, diffuse.g, diffuse.b, 1.0f });
                    }
                }
        
                // 提取金属度
                float metallic = 0.0f;
                if (aiMat->Get(AI_MATKEY_METALLIC_FACTOR, metallic) == AI_SUCCESS)
                {
                    material->SetFloat("u_Metallic", metallic);
                }
        
                // 提取粗糙度
                float roughness = 0.5f;
                if (aiMat->Get(AI_MATKEY_ROUGHNESS_FACTOR, roughness) == AI_SUCCESS)
                {
                    material->SetFloat("u_Roughness", roughness);
                }
        
                // TODO: 提取纹理路径并加载纹理
                // aiString texPath;
                // if (aiMat->GetTexture(aiTextureType_BASE_COLOR, 0, &texPath) == AI_SUCCESS)
                // {
                //     std::string fullPath = directory + "/" + texPath.C_Str();
                //     Ref<Texture2D> texture = Texture2D::Create(fullPath);
                //     material->SetTexture("u_AlbedoMap", texture);
                // }
        
                materials.push_back(material);
            }
        }
    }
    
    MeshImportResult MeshImporter::Import(const std::string& filepath, const MeshImportOptions& options)
    {
        MeshImportResult result;
        result.FilePath = filepath;
    
        Assimp::Importer importer;
    
        // 设置导入标志
        unsigned int flags = 0;
        if (options.Triangulate)       flags |= aiProcess_Triangulate;
        if (options.FlipUVs)           flags |= aiProcess_FlipUVs;
        if (options.CalculateNormals)  flags |= aiProcess_GenSmoothNormals;
        if (options.CalculateTangents) flags |= aiProcess_CalcTangentSpace;
        flags |= aiProcess_JoinIdenticalVertices;   // 合并重复顶点
        flags |= aiProcess_OptimizeMeshes;          // 优化网格
    
        const aiScene* scene = importer.ReadFile(filepath, flags);
    
        if (!scene || scene->mFlags & AI_SCENE_FLAGS_INCOMPLETE || !scene->mRootNode)
        {
            result.Success = false;
            result.ErrorMessage = importer.GetErrorString();
            LF_CORE_ERROR("Assimp Error: {0}", result.ErrorMessage);
            return result;
        }
    
        // 解析场景
        std::vector<Vertex> allVertices;
        std::vector<uint32_t> allIndices;
        std::vector<SubMesh> subMeshes;
    
        ProcessNode(scene->mRootNode, scene, allVertices, allIndices, subMeshes, options);
    
        // 创建 Mesh
        result.MeshData = CreateRef<Mesh>(allVertices, allIndices, subMeshes);
    
        // 提取材质
        ProcessMaterials(scene, result.Materials, filepath);
    
        result.Success = true;
        return result;
    }

    bool MeshImporter::IsFormatSupported(const std::string& filepath)
    {
        return false;
    }
}
