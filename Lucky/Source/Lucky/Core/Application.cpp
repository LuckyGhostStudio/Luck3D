#include "lpch.h"
#include "Application.h"

#include "Events/ApplicationEvent.h"
#include "Log.h"

namespace Lucky
{
    Application* Application::s_Instance = nullptr;

    Application::Application()
    {
        LF_CORE_ASSERT(!s_Instance, "Application 已存在!");

        s_Instance = this;

        m_Window = Window::Create(WindowProps());                               // 创建窗口
        m_Window->SetEventCallback(LF_BIND_EVENT_FUNC(Application::OnEvent));   // 设置回调函数

        m_ImGuiLayer = new ImGuiLayer();    // 创建 ImGui 层
        PushOverlay(m_ImGuiLayer);          // 添加 ImGuiLayer 到覆盖层
    }

    Application::~Application()
    {

    }

    void Application::OnEvent(Event& event)
    {
        EventDispatcher dispatcher(event);  // 事件调度器
        dispatcher.Dispatch<WindowCloseEvent>(LF_BIND_EVENT_FUNC(Application::OnWindowClose));      // 窗口关闭事件
        dispatcher.Dispatch<WindowResizeEvent>(LF_BIND_EVENT_FUNC(Application::OnWindowResize));    // 窗口大小改变事件

        // 从最顶层向下遍历层栈
        for (auto it = m_LayerStack.end(); it != m_LayerStack.begin();)
        {
            if (event.m_Handled)
            {
                break;  // 事件已处理
            }
            (*--it)->OnEvent(event);    // 层获取并处理事件
        }
    }

    void Application::Run()
    {
        while (m_Running)
        {
            float time = (float)glfwGetTime();              // 当前时间
            DeltaTime deltaTime = time - m_LastFrameTime;   // 帧间隔 = 当前时间 - 上一帧时间
            m_LastFrameTime = time;                         // 更新上一帧时间

            // 窗口未最小化
            if (!m_Minimized)
            {
                // 更新层栈中所有层
                for (Layer* layer : m_LayerStack)
                {
                    layer->OnUpdate(deltaTime);
                }

                // ImGui渲染
                m_ImGuiLayer->Begin();
                for (Layer* layer : m_LayerStack)
                {
                    layer->OnImGuiRender();
                }
                m_ImGuiLayer->End();
            }

            m_Window->OnUpdate();   // 更新窗口
        }
    }

    void Application::Close()
    {
        m_Running = false;
    }

    bool Application::OnWindowClose(WindowCloseEvent& e)
    {
        m_Running = false;  // 结束运行
        return true;
    }

    bool Application::OnWindowResize(WindowResizeEvent& e)
    {
        if (e.GetWidth() == 0 || e.GetHeight() == 0)
        {
            m_Minimized = true;        // 窗口最小化
            return false;
        }

        m_Minimized = false;

        return false;
    }
}