#include "EditorLayer.h"

#include <imgui/imgui.h>

#include "Panels/ExamplePanel.h"

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

namespace Lucky
{
#define SCENE_EXAMPLE_PANEL_ID "ExamplePanel"

    EditorLayer::EditorLayer()
        : Layer("EditorLayer"),
        m_EditorCamera(30.0f, 1280.0f / 720.0f, 0.01f, 1000.0f)
    {

    }

    void EditorLayer::OnAttach()
    {
        LF_TRACE("EditorLayer::OnAttach");
        
        FramebufferSpecification fbSpec; // 帧缓冲区规范

        fbSpec.Attachments =
        {
            FramebufferTextureFormat::RGBA8,        // 颜色缓冲区 0
            FramebufferTextureFormat::RED_INTEGER,  // 颜色缓冲区 1：作为 id 实现鼠标点击拾取
            FramebufferTextureFormat::Depth         // 深度缓冲区
        };

        fbSpec.Width = 1280;
        fbSpec.Height = 720;

        m_Framebuffer = Framebuffer::Create(fbSpec);   // 创建帧缓冲区

        m_PanelManager = CreateScope<PanelManager>();

        m_PanelManager->AddPanel<ExamplePanel>(SCENE_EXAMPLE_PANEL_ID, "Example", true);
    }

    void EditorLayer::OnDetach()
    {
        LF_TRACE("EditorLayer::OnDetach");
    }

    void EditorLayer::OnUpdate(DeltaTime dt)
    {
        if (FramebufferSpecification spec = m_Framebuffer->GetSpecification();
            m_Size.x > 0.0f && m_Size.y > 0.0f &&
            (spec.Width != m_Size.x || spec.Height != m_Size.y))
        {
            m_Framebuffer->Resize((uint32_t)m_Size.x, (uint32_t)m_Size.y);  // 重置帧缓冲区大小
            m_EditorCamera.SetViewportSize(m_Size.x, m_Size.y);             // 重置编辑器相机视口大小
        }

        m_EditorCamera.OnUpdate(dt);    // 更新编辑器相机
        
        m_Framebuffer->Bind();          // 绑定帧缓冲区

        RenderCommand::SetClearColor({ 0.1f, 0.1f, 0.1f, 1.0f });
        RenderCommand::Clear();

        Renderer3D::BeginScene(m_EditorCamera);
        {
            glm::vec3 m_Position = { 0.0f, 0.0f, 0.0f };  // 位置
            glm::vec3 m_Scale = { 1.0f, 1.0f, 1.0f };     // 缩放

            glm::mat4 transform = glm::translate(glm::mat4(1.0f), m_Position) * glm::scale(glm::mat4(1.0f), m_Scale);

            Renderer3D::DrawMesh(transform, m_SquareColor);
        }
        Renderer3D::EndScene();
        
        m_Framebuffer->Unbind();    // 解除绑定帧缓冲区
    }

    void EditorLayer::OnImGuiRender()
    {
        // 渲染 DockSpace
        m_EditorDockSpace.ImGuiRender();

        UI_DrawMenuBar();

        //m_PanelManager->OnImGuiRender();

        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(1, 0)); // 设置 Gui 窗口样式：边界 = 0
        ImGui::Begin("Scene");
        {
            ImVec2 viewportPanelSize = ImGui::GetContentRegionAvail();  // 当前面板大小
            m_Size = { viewportPanelSize.x, viewportPanelSize.y };      // 视口大小
            
            uint32_t textureID = m_Framebuffer->GetColorAttachmentRendererID(); // 颜色缓冲区 0 ID

            ImGui::Image(reinterpret_cast<void*>(textureID), ImVec2{ m_Size.x, m_Size.y }, ImVec2(0, 1), ImVec2(1, 0));   // 场景视口图像
        }
        ImGui::End();
        ImGui::PopStyleVar();
    }

    void EditorLayer::OnEvent(Event& event)
    {
        m_PanelManager->OnEvent(event);

        m_EditorCamera.OnEvent(event);
    }

    void EditorLayer::UI_DrawMenuBar()
    {
        if (ImGui::BeginMainMenuBar())
        {
            // File
            if (ImGui::BeginMenu("File"))
            {
                // 退出
                if (ImGui::MenuItem("Quit", "Ctrl Q"))
                {
                    Application::GetInstance().Close();    // 退出程序
                }

                ImGui::EndMenu();
            }

            if (ImGui::BeginMenu("Window"))
            {
                if (ImGui::MenuItem("Example"))
                {
                    uint32_t panelID = Hash::GenerateFNVHash(SCENE_EXAMPLE_PANEL_ID);
                    PanelData* panelData = m_PanelManager->GetPanelData(panelID);
                    panelData->IsOpen = true;
                }

                ImGui::EndMenu();
            }

            if (ImGui::BeginMenu("Help"))
            {
                if (ImGui::MenuItem("About Luck3D"))
                {

                }

                ImGui::EndMenu();
            }

            ImGui::EndMainMenuBar();
        }
    }
}
