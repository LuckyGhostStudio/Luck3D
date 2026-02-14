#include <Lucky.h>
#include <Lucky/Core/EntryPoint.h>

#include "EditorLayer.h"

class Luck3DApplication : public Lucky::Application
{
public:
    Luck3DApplication()
        : Lucky::Application()
    {
        PushLayer(new Lucky::EditorLayer());
    }
};

Lucky::Application* Lucky::CreateApplication()
{
    return new Luck3DApplication();
}