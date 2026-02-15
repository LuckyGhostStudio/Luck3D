#include "lpch.h"
#include "Renderer.h"

namespace Lucky
{
    void Renderer::Init()
    {
        RenderCommand::Init();

    }

    void Renderer::Shutdown()
    {

    }

    void Renderer::OnWindowResize(uint32_t width, uint32_t height)
    {
        RenderCommand::SetViewport(0, 0, width, height);    // 设置视口大小
    }
}