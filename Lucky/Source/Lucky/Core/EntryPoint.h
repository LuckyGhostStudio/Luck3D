#pragma once

extern Lucky::Application* Lucky::CreateApplication();

inline int main(int argc, char** argv)
{
    Lucky::Log::Init();    // 初始化日志系统

    LF_CORE_INFO("Initialized Log.");
    LF_CORE_INFO("Hello Luck3D.");
    
    Lucky::Application* app = Lucky::CreateApplication(); // 创建 Application

    app->Run();

    delete app;
}