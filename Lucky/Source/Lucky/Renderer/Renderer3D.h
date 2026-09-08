#pragma once

#include "EditorCamera.h"
#include "Framebuffer.h"

#include "Texture.h"
#include "Mesh.h"
#include "Material.h"

#include "LightRenderData.h"
#include "CameraRenderData.h"

namespace Lucky
{
    /// <summary>
    /// 全局渲染服务：提供跨 SceneRenderer 共享的资源与初始化
    /// 具体的场景渲染由 SceneRenderer 实例承担
    /// </summary>
    class Renderer3D
    {
    public:
        /// <summary>
        /// 全局初始化：加载 ShaderLibrary、默认材质、默认纹理、IBLPrecompute
        /// 在 Application 启动、创建任何 SceneRenderer 之前调用一次
        /// </summary>
        static void Init();

        /// <summary>
        /// 全局释放
        /// </summary>
        static void Shutdown();

        static Ref<ShaderLibrary>& GetShaderLibrary();
        static Ref<Material>& GetInternalErrorMaterial();
        static Ref<Material>& GetDefaultMaterial();
        static Ref<Material>& GetDefaultSkyboxMaterial();
        static const Ref<Texture2D>& GetDefaultTexture(TextureDefault type);
    };
}
