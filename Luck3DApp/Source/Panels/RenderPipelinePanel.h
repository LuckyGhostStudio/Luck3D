#pragma once

#include "Lucky/Editor/EditorPanel.h"

namespace Lucky
{
    class RenderPipelinePanel : public EditorPanel
    {
    public:
        RenderPipelinePanel() = default;
        ~RenderPipelinePanel() override = default;
        
        void OnUpdate(DeltaTime dt) override;
        void OnGUI() override;
    };
}