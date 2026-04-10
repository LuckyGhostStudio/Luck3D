#pragma once

extern Lucky::Application* Lucky::CreateApplication(ApplicationCommandLineArgs args);

inline int main(int argc, char** argv)
{
    Lucky::Log::Init();    // 初始化日志系统

    LF_CORE_INFO("Initialized Log.");
    LF_CORE_INFO("Hello Lucky.");
    
    Lucky::Application* app = Lucky::CreateApplication({ argc, argv }); // 创建 Application

    app->Run();

    delete app;
}