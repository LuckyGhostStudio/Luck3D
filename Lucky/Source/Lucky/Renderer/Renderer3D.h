#pragma once

#include "Camera.h"
#include "EditorCamera.h"

#include "Texture.h"
#include "Mesh.h"
#include "Material.h"

namespace Lucky
{
    /// <summary>
    /// 方向光数据：从 Scene 传递给 Renderer3D 的光照参数
    /// </summary>
    struct DirectionalLightData
    {
        glm::vec3 Direction = glm::vec3(-0.8f, -1.0f, -0.5f);   // 光照方向（世界空间）
        glm::vec3 Color = glm::vec3(1.0f, 1.0f, 1.0f);          // 光照颜色
        float Intensity = 1.0f;                                 // 光照强度
    };
    
    class Renderer3D
    {
    public:
        /// <summary>
        /// 初始化渲染器
        /// </summary>
        static void Init();

        static void Shutdown();

        /// <summary>
        /// 开始渲染场景
        /// </summary>
        /// <param name="camera">编辑器相机</param>
        /// <param name="lightData">方向光数据</param>
        static void BeginScene(const EditorCamera& camera, const DirectionalLightData& lightData);

        /// <summary>
        /// 开始渲染场景：设置场景参数
        /// </summary>
        /// <param name="camera">相机</param>
        /// <param name="transform">Transform</param>
        static void BeginScene(const Camera& camera, const glm::mat4& transform);

        /// <summary>
        /// 结束渲染场景
        /// </summary>
        static void EndScene();
        
        /// <summary>
        /// 绘制网格（使用指定材质列表）
        /// 材质列表的索引对应 SubMesh 的 MaterialIndex
        /// </summary>
        /// <param name="transform">模型变换矩阵</param>
        /// <param name="mesh">网格</param>
        /// <param name="materials">材质列表</param>
        static void DrawMesh(const glm::mat4& transform, Ref<Mesh>& mesh, const std::vector<Ref<Material>>& materials);

        /// <summary>
        /// 统计数据
        /// </summary>
        struct Statistics
        {
            uint32_t DrawCalls = 0;     // 绘制调用次数
            uint32_t TriangleCount = 0; // 三角形个数

            /// <summary>
            /// 返回总顶点个数
            /// </summary>
            /// <returns></returns>
            uint32_t GetTotalVertexCount() { return TriangleCount * 3; }

            /// <summary>
            /// 返回总索引个数
            /// </summary>
            /// <returns></returns>
            uint32_t GetTotalIndexCount() { return TriangleCount * 6; }
        };

        static Statistics GetStats();

        /// <summary>
        /// 重置统计数据
        /// </summary>
        static void ResetStats();
        
        static Ref<ShaderLibrary>& GetShaderLibrary();
        static Ref<Material>& GetInternalErrorMaterial();
        static Ref<Material>& GetDefaultMaterial();
        static const Ref<Texture2D>& GetWhiteTexture();
    };
}