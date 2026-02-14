#include <Lucky.h>
#include <Lucky/Core/EntryPoint.h>

#include "EditorLayer.h"

class LFrameApplication : public Lucky::Application
{
public:
    LFrameApplication()
        : Lucky::Application()
    {
        PushLayer(new Lucky::EditorLayer());
    }
};

Lucky::Application* Lucky::CreateApplication()
{
    return new LFrameApplication();
}