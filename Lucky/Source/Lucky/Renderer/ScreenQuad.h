#pragma once

#include "Lucky/Core/Base.h"
#include "VertexArray.h"
#include "Buffer.h"

namespace Lucky
{
    /// <summary>
    /// 全屏四边形工具类
    /// 用于后处理和描边合成等需要全屏绘制的场景
    /// 
    /// 使用方式：
    ///   ScreenQuad::Init();    // 初始化（仅一次，在 Renderer::Init() 中调用）
    ///   ScreenQuad::Draw();    // 绘制全屏四边形
    ///   ScreenQuad::Shutdown(); // 释放资源（在 Renderer::Shutdown() 中调用）
    /// 
    /// 注意：调用 Draw() 前需要先绑定目标 FBO 和 Shader
    /// </summary>
    class ScreenQuad
    {
    public:
        /// <summary>
        /// 初始化全屏四边形（创建 VAO/VBO）
        /// </summary>
        static void Init();
        
        /// <summary>
        /// 释放资源
        /// </summary>
        static void Shutdown();
        
        /// <summary>
        /// 绘制全屏四边形（6 个顶点，2 个三角形）
        /// 调用前需要：
        /// 1. 绑定目标 FBO
        /// 2. 绑定 Shader 并设置 uniform
        /// </summary>
        static void Draw();
        
    private:
        static Ref<VertexArray> s_VAO;
        static Ref<VertexBuffer> s_VBO;
    };
}
