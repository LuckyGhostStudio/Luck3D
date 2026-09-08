#include "GameViewportPanel.h"

#include "Lucky/Renderer/Renderer3D.h"
#include "Lucky/Renderer/Framebuffer.h"

#include "imgui/imgui.h"

namespace Lucky
{
    GameViewportPanel::GameViewportPanel(const Ref<Scene>& scene)
        : m_Scene(scene)
    {
        SetFlags(ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);    // 禁用滚动条

        SceneRendererSpec spec;
        spec.Width = 1280;
        spec.Height = 720;
        spec.EnableShadow = true;
        spec.EnablePicking = false;
        spec.EnableOutline = false;
        spec.EnableDebugVisualize = false;
        spec.EnablePostProcess = true;

        m_SceneRenderer = CreateRef<SceneRenderer>();
        m_SceneRenderer->Init(spec);

        // 订阅 SceneManager 的场景切换事件（Play/Stop 的副本切换、拖拽打开 .luck3d 均由此同步）
        m_SceneChangedSub = SceneManager::Subscribe([this](const Ref<Scene>& newScene)
        {
            m_Scene = newScene;
        });
    }

    GameViewportPanel::~GameViewportPanel()
    {
        SceneManager::Unsubscribe(m_SceneChangedSub);

        if (m_SceneRenderer)
        {
            m_SceneRenderer->Shutdown();
        }
    }

    void GameViewportPanel::OnUpdate(DeltaTime dt)
    {
        const Ref<Framebuffer>& framebuffer = m_SceneRenderer->GetFramebuffer();

        if (FramebufferSpecification spec = framebuffer->GetSpecification();
            m_ViewportSize.x > 0.0f && m_ViewportSize.y > 0.0f &&
            (spec.Width != m_ViewportSize.x || spec.Height != m_ViewportSize.y))
        {
            uint32_t w = static_cast<uint32_t>(m_ViewportSize.x);
            uint32_t h = static_cast<uint32_t>(m_ViewportSize.y);

            m_SceneRenderer->OnViewportResize(w, h);

            if (m_Scene)
            {
                m_Scene->OnViewportResize(w, h);
            }
        }

        if (!m_Scene)
        {
            return;
        }

        // Game 面板固定黑色清屏：无 Primary Camera 时看到的即是纯黑
        constexpr glm::vec4 blackClear{ 0.0f, 0.0f, 0.0f, 1.0f };
        m_SceneRenderer->SetClearColor(blackClear);

        // 使用场景内 Primary CameraComponent 渲染；无主相机时 Scene 内部直接 return，画面停留在黑色 Clear
        m_Scene->OnRenderRuntime(*m_SceneRenderer);
    }

    void GameViewportPanel::OnGUI()
    {
        ImVec2 viewportPanelSize = ImGui::GetContentRegionAvail();
        m_ViewportSize = { viewportPanelSize.x, viewportPanelSize.y };

        uint32_t textureID = m_SceneRenderer->GetFinalColorAttachmentID();
        ImGui::Image(reinterpret_cast<void*>(static_cast<uintptr_t>(textureID)), ImVec2{ m_ViewportSize.x, m_ViewportSize.y }, ImVec2{ 0, 1 }, ImVec2{ 1, 0 });
    }
}
