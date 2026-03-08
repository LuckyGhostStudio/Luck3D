#include "lcpch.h"
#include "Renderer.h"

#include "Renderer3D.h"

namespace Lucky
{
    void Renderer::Init()
    {
        RenderCommand::Init();
        Renderer3D::Init();
    }

    void Renderer::Shutdown()
    {
        Renderer3D::Shutdown();
    }

    void Renderer::OnWindowResize(uint32_t width, uint32_t height)
    {
        RenderCommand::SetViewport(0, 0, width, height);    // 设置视口大小
    }
}