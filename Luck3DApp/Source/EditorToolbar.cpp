#include "EditorToolbar.h"

#include "Lucky/Editor/EditorIconManager.h"
#include "Lucky/Renderer/Texture.h"
#include "Lucky/Scene/Scene.h"
#include "Lucky/Scene/SceneManager.h"

#include "Lucky/UI/Toggles.h"
#include "Lucky/UI/Widgets.h"
#include <imgui/imgui.h>

#include <algorithm>

namespace Lucky
{
    namespace
    {
        constexpr float kSceneStateButtonHeight = 28.0f;    // Play / Pause 按钮高度
        constexpr float kSceneStateButtonWidth = 50.0f;     // Play / Pause 按钮宽度
        constexpr float kButtonSpacing = 2.0f;
    }

    float EditorToolbar::GetHeight()
    {
        // 工具条实际高度取"设计目标"与"ImGui 全局 WindowMinSize.y"的较大值：
        // 项目在主题里把 WindowMinSize 设成了 (50, 30)（见 Theme.h），ImGui 内部 Begin 会强制拉起窗口最小高度，
        // 如果这里返回小于 WindowMinSize.y 的值，DockSpace 扣除的高度就会小于工具条实际高度，导致工具条与 DockSpace 之间出现视觉空隙
        const float minSizeY = ImGui::GetStyle().WindowMinSize.y;
        return std::max(s_DesiredHeight, minSizeY);
    }

    void EditorToolbar::ImGuiRender()
    {
        const Ref<Scene>& scene = SceneManager::GetActiveScene();
        SceneState state = scene ? scene->GetState() : SceneState::Edit;
        bool isPlaying = (state != SceneState::Edit);

        const float totalHeight = GetHeight();

        ImGuiViewport* viewport = ImGui::GetMainViewport();
        ImGui::SetNextWindowPos(viewport->WorkPos);
        ImGui::SetNextWindowSize(ImVec2(viewport->WorkSize.x, totalHeight));
        ImGui::SetNextWindowViewport(viewport->ID);

        ImGuiWindowFlags flags =
            ImGuiWindowFlags_NoDecoration |
            ImGuiWindowFlags_NoMove |
            ImGuiWindowFlags_NoDocking |
            ImGuiWindowFlags_NoSavedSettings |
            ImGuiWindowFlags_NoBringToFrontOnFocus;

        // 工具条底色固定为 #191919，与 DockSpace 主体色形成明确的顶部工具区视觉分隔
        const ImVec4 toolbarBg = ImVec4(0.098f, 0.098f, 0.098f, 1.0f);
        ImGui::PushStyleColor(ImGuiCol_WindowBg, toolbarBg);
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));
        ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
        ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(0.0f, 0.0f));

        ImGui::Begin("##EditorToolbar", nullptr, flags);

        // 按钮尺寸和间距：每个按钮独立指定，未来非正方形按钮直接改各自 size
        const ImVec2 playSize(kSceneStateButtonWidth, kSceneStateButtonHeight);
        const ImVec2 pauseSize(kSceneStateButtonWidth, kSceneStateButtonHeight);

        // 水平居中所有按钮
        const float buttonsTotalWidth = playSize.x + kButtonSpacing + pauseSize.x;
        const float availWidth = ImGui::GetContentRegionAvail().x;
        ImGui::SetCursorPosX((availWidth - buttonsTotalWidth) * 0.5f);

        // 垂直居中：取工具条高度与按钮高度的差值一半
        const float cursorY = (totalHeight - kSceneStateButtonHeight) * 0.5f;
        ImGui::SetCursorPosY(cursorY);

        // ---- Playback 分段按钮条：Play / Pause（将来加 Step 只需把 count 改 3 并添加一个 Toggle）----
        UI::BeginSegmentedGroup("##Playback", 2, kButtonSpacing);

        // Play：isPlaying 是从 SceneManager 推导的本地副本，用临时 bool 吞掉 Toggle 的内部翻转，
        // 真正的播放/停止判断仍走原始 isPlaying。
        bool playingLocal = isPlaying;
        if (UI::ToggleIconButton("##Play", EditorIconManager::GetPlayIcon(), playingLocal, playSize))
        {
            if (!isPlaying)
            {
                SceneManager::OnScenePlay();
                if (m_PauseArmed)
                {
                    SceneManager::SetScenePaused(true);
                }
            }
            else
            {
                SceneManager::OnSceneStop();
                m_PauseArmed = false;   // Stop 时清零预暂停位（对齐 Unity）
            }
        }

        // Pause：ToggleIconButton 内部已自动翻转 m_PauseArmed，不要再手动翻转一次。
        if (UI::ToggleIconButton("##Pause", EditorIconManager::GetPauseIcon(), m_PauseArmed, pauseSize))
        {
            if (isPlaying)
            {
                SceneManager::SetScenePaused(m_PauseArmed);
            }
        }

        UI::EndSegmentedGroup();

        ImGui::End();

        ImGui::PopStyleVar(3);
        ImGui::PopStyleColor();
    }
}
