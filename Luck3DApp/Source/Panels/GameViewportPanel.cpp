#include "GameViewportPanel.h"

#include "Lucky/Renderer/RenderCommand.h"
#include "Lucky/Renderer/Renderer3D.h"

#include "imgui/imgui.h"

namespace Lucky
{
    GameViewportPanel::GameViewportPanel(const Ref<Scene>& scene)
        : m_Scene(scene)
    {
        SetFlags(ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);    // 禁用滚动条

        FramebufferSpecification fbSpec;

        fbSpec.Attachments =
        {
            FramebufferTextureFormat::RGBA8,        // 颜色缓冲区
            FramebufferTextureFormat::Depth         // 深度缓冲区
        };

        fbSpec.Width = 1280;
        fbSpec.Height = 720;

        m_Framebuffer = Framebuffer::Create(fbSpec);

        // 订阅 SceneManager 的场景切换事件（Play/Stop 的副本切换、拖拽打开 .luck3d 均由此同步）
        m_SceneChangedSub = SceneManager::Subscribe([this](const Ref<Scene>& newScene)
        {
            m_Scene = newScene;
        });
    }

    GameViewportPanel::~GameViewportPanel()
    {
        SceneManager::Unsubscribe(m_SceneChangedSub);
    }

    void GameViewportPanel::OnUpdate(DeltaTime dt)
    {
        if (FramebufferSpecification spec = m_Framebuffer->GetSpecification();
            m_ViewportSize.x > 0.0f && m_ViewportSize.y > 0.0f &&
            (spec.Width != m_ViewportSize.x || spec.Height != m_ViewportSize.y))
        {
            m_Framebuffer->Resize(static_cast<uint32_t>(m_ViewportSize.x), static_cast<uint32_t>(m_ViewportSize.y));

            // 同步场景中非固定宽高比的相机
            // 注意：不调用 Renderer3D::ResizePipeline，pipeline 尺寸由 Scene 面板负责
            if (m_Scene)
            {
                m_Scene->OnViewportResize(static_cast<uint32_t>(m_ViewportSize.x), static_cast<uint32_t>(m_ViewportSize.y));
            }
        }

        if (!m_Scene)
        {
            return;
        }

        m_Framebuffer->Bind();

        // Game 面板固定黑色清屏：无 Primary Camera 时看到的即是纯黑
        constexpr glm::vec4 blackClear{ 0.0f, 0.0f, 0.0f, 1.0f };
        RenderCommand::SetClearColor(blackClear);
        RenderCommand::Clear();

        // 覆盖 Scene 面板留下的全局状态：把 Target FBO 指向自己、清空描边集合
        Renderer3D::SetTargetFramebuffer(m_Framebuffer);
        Renderer3D::SetClearColor(blackClear);
        Renderer3D::SetOutlineEntities({});

        // 使用场景内 Primary CameraComponent 渲染；无主相机时 Scene 内部直接 return，画面停留在黑色 Clear
        m_Scene->OnRenderRuntime();

        m_Framebuffer->Unbind();
    }

    void GameViewportPanel::OnGUI()
    {
        ImVec2 viewportPanelSize = ImGui::GetContentRegionAvail();
        m_ViewportSize = { viewportPanelSize.x, viewportPanelSize.y };

        uint32_t textureID = m_Framebuffer->GetColorAttachmentRendererID();
        ImGui::Image(reinterpret_cast<void*>(static_cast<uintptr_t>(textureID)), ImVec2{ m_ViewportSize.x, m_ViewportSize.y }, ImVec2{ 0, 1 }, ImVec2{ 1, 0 });
    }
}
