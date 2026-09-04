#include "lcpch.h"
#include "Renderer.h"

#include "Renderer3D.h"
#include "Renderer2D.h"
#include "GizmoRenderer.h"
#include "ScreenQuad.h"

namespace Lucky
{
    void Renderer::Init()
    {
        RenderCommand::Init();
        ScreenQuad::Init();
        Renderer3D::Init();
        Renderer2D::Init();     // 必须在 Renderer3D::Init 之后（依赖 ShaderLibrary 与白色纹理）
        GizmoRenderer::Init();
    }

    void Renderer::Shutdown()
    {
        Renderer3D::Shutdown();
        Renderer2D::Shutdown();
        GizmoRenderer::Shutdown();
        ScreenQuad::Shutdown();
    }

    void Renderer::OnWindowResize(uint32_t width, uint32_t height)
    {
        RenderCommand::SetViewport(0, 0, width, height);    // 设置视口大小
    }
}