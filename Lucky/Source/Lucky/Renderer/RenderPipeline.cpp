#include "lcpch.h"
#include "RenderPipeline.h"

namespace Lucky
{
    void RenderPipeline::Init()
    {
        for (auto& pass : m_Passes)
        {
            pass->Init();
        }
    }
    
    void RenderPipeline::Shutdown()
    {
        m_Passes.clear();
    }
    
    void RenderPipeline::AddPass(const Ref<RenderPass>& pass)
    {
        m_Passes.push_back(pass);
    }
    
    void RenderPipeline::RemovePass(const std::string& name)
    {
        std::erase_if(m_Passes, [&name](const Ref<RenderPass>& pass)
        {
            return pass->GetName() == name;
        });
    }
    
    void RenderPipeline::Execute(const RenderContext& context)
    {
        for (auto& pass : m_Passes)
        {
            if (!pass->Enabled)
            {
                continue;
            }
            
            pass->Execute(context);
        }
    }
    
    void RenderPipeline::Resize(uint32_t width, uint32_t height)
    {
        for (auto& pass : m_Passes)
        {
            pass->Resize(width, height);
        }
    }
}
