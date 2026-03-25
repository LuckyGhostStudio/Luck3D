#include "SceneViewportPanel.h"

#include "Lucky/Renderer/RenderCommand.h"

#include "imgui/imgui.h"

namespace Lucky
{
    SceneViewportPanel::SceneViewportPanel(const Ref<Scene>& scene)
        : m_Scene(scene), 
        m_EditorCamera(30.0f, 1280.0f / 720.0f, 0.01f, 1000.0f)
    {
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
    }

    void SceneViewportPanel::OnUpdate(DeltaTime dt)
    {
        if (FramebufferSpecification spec = m_Framebuffer->GetSpecification();
            m_ViewportSize.x > 0.0f && m_ViewportSize.y > 0.0f &&
            (spec.Width != m_ViewportSize.x || spec.Height != m_ViewportSize.y))
        {
            m_Framebuffer->Resize((uint32_t)m_ViewportSize.x, (uint32_t)m_ViewportSize.y);  // 重置帧缓冲区大小
            m_EditorCamera.SetViewportSize(m_ViewportSize.x, m_ViewportSize.y);             // 重置编辑器相机视口大小
        }

        m_EditorCamera.OnUpdate(dt);    // 更新编辑器相机
        
        m_Framebuffer->Bind();          // 绑定帧缓冲区

        RenderCommand::SetClearColor({ 0.1f, 0.1f, 0.1f, 1.0f });
        RenderCommand::Clear();

        m_Scene->OnUpdate(dt, m_EditorCamera);   // 更新场景
        
        m_Framebuffer->Unbind();    // 解除绑定帧缓冲区
    }

    void SceneViewportPanel::OnBegin(const char* name)
    {
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(1, 0)); // 设置 Gui 窗口样式：边界 = 0
        EditorPanel::OnBegin(name);
    }

    void SceneViewportPanel::OnEnd()
    {
        EditorPanel::OnEnd();
        ImGui::PopStyleVar();
    }

    void SceneViewportPanel::OnGUI()
    {
        ImVec2 viewportPanelSize = ImGui::GetContentRegionAvail();  // 当前面板大小
        m_ViewportSize = { viewportPanelSize.x, viewportPanelSize.y };      // 视口大小
        
        uint32_t textureID = m_Framebuffer->GetColorAttachmentRendererID(); // 颜色缓冲区 0 ID

        ImGui::Image(reinterpret_cast<void*>(textureID), ImVec2{ m_ViewportSize.x, m_ViewportSize.y }, ImVec2(0, 1), ImVec2(1, 0)); // 场景视口图像
    }

    void SceneViewportPanel::OnEvent(Event& event)
    {
        m_EditorCamera.OnEvent(event);
    }
}
