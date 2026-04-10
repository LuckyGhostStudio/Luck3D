#pragma once

#include "Base.h"
#include "Window.h"
#include "LayerStack.h"
#include "Lucky/ImGui/ImGuiLayer.h"
#include "Events/ApplicationEvent.h"

namespace Lucky
{
    /// <summary>
    /// Application 命令行参数
    /// </summary>
    struct ApplicationCommandLineArgs
    {
        int Count = 0;          // 参数个数
        char** Args = nullptr;  // 参数列表

        const char* operator[](int index) const
        {
            LF_CORE_ASSERT(index < Count, "Index out of args count range!");

            return Args[index];
        }
    };

    /// <summary>
    /// Application 规范
    /// </summary>
    struct ApplicationSpecification
    {
        std::string Name = "Lucky Application";
        std::string WorkingDirectory;               // 项目工作目录

        ApplicationCommandLineArgs CommandLineArgs; // 命令行参数
    };

    /// <summary>
    /// 应用程序：管理应用程序的入口点和生命周期
    /// </summary>
    class Application
    {
    public:
        Application(const ApplicationSpecification& specification);
        virtual ~Application();

        /// <summary>
        /// 添加普通层到层栈
        /// </summary>
        /// <param name="layer">普通层</param>
        void PushLayer(Layer* layer) { m_LayerStack.PushLayer(layer); }

        /// <summary>
        /// 添加覆盖层到层栈
        /// </summary>
        /// <param name="layer">覆盖层</param>
        void PushOverlay(Layer* layer) { m_LayerStack.PushOverlay(layer); }

        /// <summary>
        /// 从层栈移除普通层
        /// </summary>
        /// <param name="layer">普通层</param>
        void PopLayer(Layer* layer) { m_LayerStack.PopLayer(layer); }

        /// <summary>
        /// 从层栈移除覆盖层
        /// </summary>
        /// <param name="layer">覆盖层</param>
        void PopOverlay(Layer* layer) { m_LayerStack.PopOverlay(layer); }

        /// <summary>
        /// 事件回调函数
        /// </summary>
        /// <param name="event">事件</param>
        void OnEvent(Event& event);

        /// <summary>
        /// 运行：主循环
        /// </summary>
        void Run();

        /// <summary>
        /// 关闭
        /// </summary>
        void Close();

        static Application& GetInstance() { return *s_Instance; }

        const ApplicationSpecification& GetSpecification() const { return m_Specification; }

        Window& GetWindow() const { return *m_Window; }

        ImGuiLayer* GetImGuiLayer() const { return m_ImGuiLayer; }
    private:
        /// <summary>
        /// 窗口关闭回调函数
        /// </summary>
        /// <param name="e">窗口关闭事件</param>
        /// <returns>是否已关闭</returns>
        bool OnWindowClose(WindowCloseEvent& e);

        /// <summary>
        /// 窗口缩放时调用
        /// </summary>
        /// <param name="e">窗口缩放事件</param>
        /// <returns>事件处理结果</returns>
        bool OnWindowResize(WindowResizeEvent& e);
    private:
        static Application* s_Instance;

        ApplicationSpecification m_Specification;   // App 规范

        Scope<Window> m_Window;     // 窗口

        LayerStack m_LayerStack;    // 层栈
        ImGuiLayer* m_ImGuiLayer;

        float m_LastFrameTime = 0.0f;   // 上一帧时间

        bool m_Running = true;      // 运行
        bool m_Minimized = false;   // 最小化
    };

    /// <summary>
    /// 创建 App：在 LuckyApplication 中定义
    /// </summary>
    /// <param name="args">命令行参数</param>
    /// <returns></returns>
    Application* CreateApplication(ApplicationCommandLineArgs args);
}