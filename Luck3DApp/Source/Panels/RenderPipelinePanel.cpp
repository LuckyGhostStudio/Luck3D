#include "RenderPipelinePanel.h"

#include "Lucky/Renderer/SceneRenderer.h"
#include "Lucky/Renderer/RenderPipeline.h"

#include "Lucky/UI/Widgets.h"
#include "Lucky/UI/PropertyGrid.h"
#include "Lucky/UI/DrawUtils.h"
#include "Lucky/UI/ScopedGuards.h"
#include "Lucky/UI/UICore.h"

#include <imgui/imgui.h>

namespace Lucky
{
    void RenderPipelinePanel::OnUpdate(DeltaTime dt)
    {
    }

    void RenderPipelinePanel::OnGUI()
    {
        UI::ShiftCursorY(2.0f);
        
        // 显示统计数据（读取主 SceneRenderer）
        if (UI::BeginPrimaryCollapsing("Statistics"))
        {
            SceneRenderer* primary = SceneRenderer::GetPrimary();
            RendererStats stats = primary ? primary->GetStats() : RendererStats{};

            UI::PropertyReadOnlyString("Draw Calls", std::to_string(stats.DrawCalls).c_str());
            UI::PropertyReadOnlyString("Triangles", std::to_string(stats.TriangleCount).c_str());
            UI::PropertyReadOnlyString("Vertices", std::to_string(stats.GetTotalVertexCount()).c_str());
            
            UI::EndPrimaryCollapsing();
        }
        
        // 显示 Pass 列表，按分组显示
        if (UI::BeginPrimaryCollapsing("Passes"))
        {
            SceneRenderer* primaryRenderer = SceneRenderer::GetPrimary();
            if (!primaryRenderer)
            {
                UI::EndPrimaryCollapsing();
                return;
            }
            RenderPipeline& pipeline = primaryRenderer->GetPipeline();
            const std::vector<Ref<RenderPass>>& passes = pipeline.GetPasses();
            
            std::string currentGroup = "";
            int passIndex = 0;
        
            for (const Ref<RenderPass>& pass : passes)
            {
                const std::string& group = pass->GetGroup();
                bool groupOpened = false;
        
                // 分组变化时显示分组标题
                if (group != currentGroup)
                {
                    currentGroup = group;
                    
                    // 分组名
                    {
                        UI::ScopedFont boldFont(ImGui::GetIO().Fonts->Fonts[0]);    // TODO 封装 Fonts
                        UI::ShiftCursorX(4.0f);
                        ImGui::TextColored(ImVec4(0.6f, 0.8f, 1.0f, 1.0f), "%s", group.c_str());
                    }
                }
                
                // 显示 Pass 信息
                const std::string& strPassID = std::format("{} {}", passIndex, pass->GetName());
                if (UI::BeginCollapsing(strPassID.c_str()))
                {
                    UI::PropertyCheckbox("Enable", pass->Enabled);
                    
                    UI::EndCollapsing();
                }
                
                passIndex++;
            }
            
            UI::EndPrimaryCollapsing();
        }
        
        UI::Draw::HorizontalLine();
    }
}
