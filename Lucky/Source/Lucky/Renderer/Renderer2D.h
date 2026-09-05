#pragma once

#include "EditorCamera.h"
#include "Texture.h"
#include "Material.h"

#include <glm/glm.hpp>

namespace Lucky
{
    /// <summary>
    /// 2D 渲染器：世界空间 Sprite 批处理绘制
    /// 与 Renderer3D 共享相机 / FBO / EntityID 缓冲，作为 RenderPipeline 中的 Pass 与 3D 一起渲染
    /// </summary>
    class Renderer2D
    {
    public:
        /// <summary>
        /// 初始化渲染器（创建 VAO/VBO/IBO、加载 Sprite Shader、准备白色纹理）
        /// 必须在 Renderer3D::Init() 之后调用，因为依赖 ShaderLibrary 与白色纹理
        /// </summary>
        static void Init();

        /// <summary>
        /// 释放资源
        /// </summary>
        static void Shutdown();

        /// <summary>
        /// 开始场景：使用编辑器相机
        /// </summary>
        /// <param name="camera">编辑器相机</param>
        static void BeginScene(const EditorCamera& camera);

        /// <summary>
        /// 开始场景：通用重载（供 Runtime 相机 / Sprite2DPass 使用）
        /// </summary>
        /// <param name="viewMatrix">视图矩阵</param>
        /// <param name="projectionMatrix">投影矩阵</param>
        static void BeginScene(const glm::mat4& viewMatrix, const glm::mat4& projectionMatrix);

        /// <summary>
        /// 结束场景：Flush 剩余批次
        /// </summary>
        static void EndScene();

        /// <summary>
        /// 立即提交当前批次并重置（用户显式断批时调用）
        /// </summary>
        static void Flush();

        /// <summary>
        /// 设置当前批次使用的材质：若与当前批次材质不同，则先 Flush 再切换
        /// 后续 DrawQuad 调用都归属该材质批次，直到再次切换
        /// </summary>
        /// <param name="material">材质引用（nullptr 视为使用默认材质）</param>
        static void SetBatchMaterial(const Ref<Material>& material);

        /// <summary>
        /// 获取默认 Sprite 材质（Shader = Sprite，Alpha Blend + Cull Off + ZWrite Off）
        /// </summary>
        static const Ref<Material>& GetDefaultMaterial();

        /// <summary>
        /// 获取 Sprite 错误材质
        /// </summary>
        static const Ref<Material>& GetErrorMaterial();

        // ---- 图元绘制 API ----

        /// <summary>
        /// 绘制纯色 Quad（通用变换矩阵）
        /// </summary>
        /// <param name="transform">模型变换矩阵（世界空间）</param>
        /// <param name="color">颜色</param>
        /// <param name="entityID">实体 ID（用于拾取，-1 表示无效）</param>
        static void DrawQuad(const glm::mat4& transform, const glm::vec4& color, int entityID = -1);

        /// <summary>
        /// 绘制带纹理的 Quad
        /// </summary>
        /// <param name="transform">模型变换矩阵（世界空间）</param>
        /// <param name="texture">纹理（nullptr 视为纯色）</param>
        /// <param name="tintColor">颜色 Tint</param>
        /// <param name="uvRect">UV 区域（xy=uvMin, zw=uvMax），默认整张图</param>
        /// <param name="tilingFactor">平铺倍数</param>
        /// <param name="flipX">水平翻转 UV</param>
        /// <param name="flipY">垂直翻转 UV</param>
        /// <param name="entityID">实体 ID</param>
        static void DrawQuad(const glm::mat4& transform, const Ref<Texture2D>& texture,
                             const glm::vec4& tintColor = glm::vec4(1.0f),
                             const glm::vec4& uvRect = glm::vec4(0.0f, 0.0f, 1.0f, 1.0f),
                             float tilingFactor = 1.0f,
                             bool flipX = false,
                             bool flipY = false,
                             int entityID = -1);

        // ---- 统计数据 ----

        /// <summary>
        /// 统计数据
        /// </summary>
        struct Statistics
        {
            uint32_t DrawCalls = 0;     // 绘制调用次数
            uint32_t QuadCount = 0;     // Quad 个数

            /// <summary>
            /// 返回总顶点个数
            /// </summary>
            uint32_t GetTotalVertexCount() const { return QuadCount * 4; }

            /// <summary>
            /// 返回总索引个数
            /// </summary>
            uint32_t GetTotalIndexCount() const { return QuadCount * 6; }
        };

        /// <summary>
        /// 获取统计数据（值拷贝）
        /// </summary>
        static Statistics GetStats();

        /// <summary>
        /// 重置统计数据
        /// </summary>
        static void ResetStats();
    };
}
