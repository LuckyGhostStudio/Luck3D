#include <Lucky.h>
#include <Lucky/Core/EntryPoint.h>

#include "EditorLayer.h"

class Luck3DApplication : public Lucky::Application
{
public:
    Luck3DApplication(const Lucky::ApplicationSpecification& specification)
        : Application(specification)
    {
        PushLayer(new Lucky::EditorLayer());
    }
};

Lucky::Application* Lucky::CreateApplication(ApplicationCommandLineArgs args)
{
    ApplicationSpecification spec;
    spec.Name = "Luck3D";
    spec.CommandLineArgs = args;

    return new Luck3DApplication(spec);
}