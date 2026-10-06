#pragma once

#include "Lucky/Core/Base.h"
#include "Lucky/Renderer/Framebuffer.h"

namespace Lucky
{
    class Material;
    class Mesh;

    /// <summary>
    /// 资产预览离屏渲染服务
    /// 内部持有一个专用 SceneRenderer（128×128，关闭所有可选 Pass），
    /// 为 Material / Mesh 一次性渲染到共享 FBO 后将 FBO 引用返回；
    /// 像素留在该 FBO 的 Color Attachment 中，由调用方（Cache）在下一次渲染前 Blit 走
    /// </summary>
    class AssetPreviewRenderer
    {
    public:
        /// <summary>
        /// 初始化：创建专用 SceneRenderer 和球体预览网格
        /// 必须在 Renderer::Init 之后调用
        /// </summary>
        static void Init();

        /// <summary>
        /// 释放专用 SceneRenderer 与预览网格
        /// </summary>
        static void Shutdown();

        /// <summary>
        /// 渲染一张材质预览（球体 + 该材质 + 固定布光）
        /// 结果留在共享 FBO 的 Color Attachment 中
        /// </summary>
        /// <param name="material">目标材质，为空时返回空 Ref</param>
        /// <returns>共享 FBO 引用</returns>
        static const Ref<Framebuffer>& RenderMaterial(const Ref<Material>& material);

        /// <summary>
        /// 渲染一张网格预览（Mesh + 默认白色材质 + 按包围盒自动框选相机）
        /// 结果留在共享 FBO 的 Color Attachment 中
        /// </summary>
        /// <param name="mesh">目标网格，为空时返回空 Ref</param>
        /// <returns>共享 FBO 引用</returns>
        static const Ref<Framebuffer>& RenderMesh(const Ref<Mesh>& mesh);

        /// <summary>
        /// 预览纹理尺寸（宽 = 高 = 128）
        /// </summary>
        static uint32_t GetPreviewSize();
    };
}
