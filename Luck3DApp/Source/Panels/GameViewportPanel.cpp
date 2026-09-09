#include "GameViewportPanel.h"

#include "Lucky/Renderer/Renderer3D.h"
#include "Lucky/Renderer/Framebuffer.h"

#include "Lucky/Scene/Entity.h"
#include "Lucky/Scene/Components/Components.h"

#include "Lucky/UI/ScopedGuards.h"
#include "Lucky/UI/UICore.h"
#include "Lucky/UI/Widgets.h"

#include "imgui/imgui.h"

namespace Lucky
{
    namespace
    {
        /// <summary>
        /// Game 视口分辨率模式
        /// </summary>
        enum class GameViewResolutionMode : uint8_t
        {
            FreeAspect = 0,     // RT 尺寸 = 面板尺寸
            AspectRatio,        // 相机 aspect 固定；RT 按 aspect 收缩到面板短边
            FixedResolution     // RT 尺寸固定；面板显示时等比缩放
        };

        struct GameViewResolutionPreset
        {
            const char* Label;                  // 下拉框显示文本
            GameViewResolutionMode Mode;        // 模式
            uint32_t Width;                     // FixedResolution 时的固定宽；AspectRatio 时的比例分子
            uint32_t Height;                    // FixedResolution 时的固定高；AspectRatio 时的比例分母
        };

        constexpr GameViewResolutionPreset s_ResolutionPresets[] =
        {
            { "Free Aspect",     GameViewResolutionMode::FreeAspect,      0,    0    },
            { "16:9",            GameViewResolutionMode::AspectRatio,     16,   9    },
            { "16:10",           GameViewResolutionMode::AspectRatio,     16,   10   },
            { "4:3",             GameViewResolutionMode::AspectRatio,     4,    3    },
            { "5:4",             GameViewResolutionMode::AspectRatio,     5,    4    },
            { "9:16 (Portrait)", GameViewResolutionMode::AspectRatio,     9,    16   },
            { "1920x1080",       GameViewResolutionMode::FixedResolution, 1920, 1080 },
            { "1280x720",        GameViewResolutionMode::FixedResolution, 1280, 720  },
            { "1080x1920",       GameViewResolutionMode::FixedResolution, 1080, 1920 },
        };

        constexpr int s_ResolutionPresetCount = static_cast<int>(sizeof(s_ResolutionPresets) / sizeof(s_ResolutionPresets[0]));

        /// <summary>
        /// 计算 RT 目标尺寸
        /// </summary>
        glm::uvec2 ComputeRTSize(const GameViewResolutionPreset& preset, const glm::vec2& panelSize)
        {
            if (panelSize.x <= 0.0f || panelSize.y <= 0.0f)
            {
                return { 0, 0 };
            }

            switch (preset.Mode)
            {
                case GameViewResolutionMode::FreeAspect:
                {
                    return { static_cast<uint32_t>(panelSize.x), static_cast<uint32_t>(panelSize.y) };
                }
                case GameViewResolutionMode::AspectRatio:
                {
                    float targetAspect = static_cast<float>(preset.Width) / static_cast<float>(preset.Height);
                    float panelAspect = panelSize.x / panelSize.y;

                    if (panelAspect > targetAspect)
                    {
                        // 面板过宽 → RT 高度取满，宽度按比例收缩
                        uint32_t h = static_cast<uint32_t>(panelSize.y);
                        uint32_t w = static_cast<uint32_t>(panelSize.y * targetAspect);
                        return { w, h };
                    }

                    // 面板过高 → RT 宽度取满，高度按比例收缩
                    uint32_t w = static_cast<uint32_t>(panelSize.x);
                    uint32_t h = static_cast<uint32_t>(panelSize.x / targetAspect);
                    return { w, h };
                }
                case GameViewResolutionMode::FixedResolution:
                {
                    return { preset.Width, preset.Height };
                }
            }
            return { 0, 0 };
        }

        /// <summary>
        /// 计算 RT 在面板中的显示尺寸（等比缩放到面板范围内，剩余作为黑边）
        /// </summary>
        glm::vec2 ComputeDisplaySize(const glm::uvec2& rtSize, const glm::vec2& panelSize)
        {
            if (rtSize.x == 0 || rtSize.y == 0 || panelSize.x <= 0.0f || panelSize.y <= 0.0f)
            {
                return { 0.0f, 0.0f };
            }

            float rtAspect = static_cast<float>(rtSize.x) / static_cast<float>(rtSize.y);
            float panelAspect = panelSize.x / panelSize.y;

            if (panelAspect > rtAspect)
            {
                return { panelSize.y * rtAspect, panelSize.y };
            }
            return { panelSize.x, panelSize.x / rtAspect };
        }
    }

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
        const GameViewResolutionPreset& preset = s_ResolutionPresets[m_ResolutionIndex];
        glm::uvec2 rtSize = ComputeRTSize(preset, m_ViewportSize);

        // RT 尺寸变化时触发 resize（避免 Fixed Resolution 下每帧无谓重建 FBO）
        if (rtSize.x > 0 && rtSize.y > 0 && rtSize != m_LastRTSize)
        {
            m_SceneRenderer->OnViewportResize(rtSize.x, rtSize.y);
            if (m_Scene)
            {
                m_Scene->OnViewportResize(rtSize.x, rtSize.y);
            }
            m_LastRTSize = rtSize;
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
        // 面板外围空白填充色 #282828
        UI::ScopedColor windowBg(ImGuiCol_WindowBg, ImVec4{ 0.157f, 0.157f, 0.157f, 1.0f });

        // ---- 顶部 ToolBar ----
        float toolBarHeight = 34.0f;
        {
            UI::ScopedColor bgColor(ImGuiCol_ChildBg, { 0.235f, 0.235f, 0.235f, 1.0f });
            ImGui::BeginChild("ToolBar", { 0, toolBarHeight }, false, ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);
            {
                UI::ShiftCursor(4.0f, 4.0f);

                ImGui::SetNextItemWidth(140.0f);

                const char* labels[s_ResolutionPresetCount];
                for (int i = 0; i < s_ResolutionPresetCount; ++i)
                {
                    labels[i] = s_ResolutionPresets[i].Label;
                }
                UI::DropdownList(m_ResolutionIndex, labels, s_ResolutionPresetCount);
            }
            ImGui::EndChild();
        }

        // ---- RT 图像区域 ----
        ImVec2 avail = ImGui::GetContentRegionAvail();
        m_ViewportSize = { avail.x, avail.y };

        const GameViewResolutionPreset& preset = s_ResolutionPresets[m_ResolutionIndex];
        glm::uvec2 rtSize = ComputeRTSize(preset, m_ViewportSize);
        glm::vec2 displaySize = ComputeDisplaySize(rtSize, m_ViewportSize);

        if (displaySize.x <= 0.0f || displaySize.y <= 0.0f)
        {
            return;
        }

        // 居中显示
        ImVec2 cursorStart = ImGui::GetCursorScreenPos();
        ImVec2 centerOffset = { (avail.x - displaySize.x) * 0.5f, (avail.y - displaySize.y) * 0.5f };
        ImGui::SetCursorScreenPos({ cursorStart.x + centerOffset.x, cursorStart.y + centerOffset.y });

        uint32_t textureID = m_SceneRenderer->GetFinalColorAttachmentID();
        ImGui::Image(reinterpret_cast<void*>(static_cast<uintptr_t>(textureID)), ImVec2{ displaySize.x, displaySize.y }, ImVec2{ 0, 1 }, ImVec2{ 1, 0 });
    }
}
