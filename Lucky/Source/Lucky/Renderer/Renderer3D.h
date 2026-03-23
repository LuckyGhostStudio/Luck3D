#pragma once

#include "Camera.h"
#include "Texture.h"
#include "Mesh.h"
#include "EditorCamera.h"

namespace Lucky
{
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
        static void BeginScene(const EditorCamera& camera);

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
        
        static void DrawMesh(const glm::mat4& transform, Ref<Mesh>& mesh);

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
    };
}