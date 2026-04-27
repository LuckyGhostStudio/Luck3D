#pragma once

#include "Lucky/Renderer/VertexArray.h"
#include "Lucky/Renderer/RenderState.h"

#include <glm/glm.hpp>

namespace Lucky
{
    /// <summary>
    /// 渲染命令：渲染接口中最基本的渲染调用
    /// </summary>
    class RenderCommand
    {
    public:
        /// <summary>
        /// 初始化渲染器
        /// </summary>
        static void Init();

        /// <summary>
        /// 设置清屏颜色
        /// </summary>
        /// <param name="color">清屏颜色</param>
        static void SetClearColor(const glm::vec4& color);

        /// <summary>
        /// 设置视口大小
        /// </summary>
        /// <param name="x">左下 x</param>
        /// <param name="y">左下 y</param>
        /// <param name="width">宽</param>
        /// <param name="height">高</param>
        static void SetViewport(uint32_t x, uint32_t y, uint32_t width, uint32_t height);

        /// <summary>
        /// 清屏
        /// </summary>
        static void Clear();

        /// <summary>
        /// 绘制索引缓冲区
        /// </summary>
        /// <param name="vertexArray">待绘制的顶点数组</param>
        /// <param name="indexCount">索引个数</param>
        static void DrawIndexed(const Ref<VertexArray>& vertexArray, uint32_t indexCount = 0);
        
        /// <summary>
        /// 绘制索引缓冲区：指定索引偏移和索引个数
        /// </summary>
        /// <param name="vertexArray">待绘制的顶点数组</param>
        /// <param name="indexOffset">索引偏移</param>
        /// <param name="indexCount">索引个数</param>
        static void DrawIndexedRange(const Ref<VertexArray>& vertexArray, uint32_t indexOffset = 0, uint32_t indexCount = 0);

        /// <summary>
        /// 绘制直线
        /// </summary>
        /// <param name="vertexArray">待绘制顶点数组</param>
        /// <param name="vertexCount">直线顶点个数</param>
        static void DrawLines(const Ref<VertexArray>& vertexArray, uint32_t vertexCount);

        /// <summary>
        /// 绘制三角形（无索引）
        /// </summary>
        /// <param name="vertexArray">顶点数组</param>
        /// <param name="vertexCount">顶点个数</param>
        static void DrawArrays(const Ref<VertexArray>& vertexArray, uint32_t vertexCount);

        /// <summary>
        /// 设置线宽
        /// </summary>
        /// <param name="width">宽度</param>
        static void SetLineWidth(float width);

        // ---- 渲染状态控制 ----

        /// <summary>
        /// 设置面剔除模式
        /// </summary>
        static void SetCullMode(CullMode mode);

        /// <summary>
        /// 设置深度测试开关
        /// </summary>
        static void SetDepthTest(bool enable);

        /// <summary>
        /// 设置深度写入开关
        /// </summary>
        static void SetDepthWrite(bool enable);

        /// <summary>
        /// 设置深度测试比较函数
        /// </summary>
        static void SetDepthFunc(DepthCompareFunc func);

        /// <summary>
        /// 设置混合模式
        /// </summary>
        static void SetBlendMode(BlendMode mode);

        /// <summary>
        /// 设置 DrawBuffer 目标（控制写入哪些颜色附件）
        /// </summary>
        /// <param name="attachments">颜色附件列表</param>
        /// <param name="count">附件数量</param>
        static void SetDrawBuffers(const uint32_t* attachments, uint32_t count);

        /// <summary>
        /// 绑定纹理到指定纹理单元
        /// </summary>
        /// <param name="slot">纹理单元索引（0, 1, 2, ...）</param>
        /// <param name="textureID">纹理 ID</param>
        static void BindTextureUnit(uint32_t slot, uint32_t textureID);
    };
}