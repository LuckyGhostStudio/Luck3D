#include "lcpch.h"
#include "PostProcessStack.h"

namespace Lucky
{
    void PostProcessStack::Init(uint32_t width, uint32_t height)
    {
        FramebufferSpecification spec;
        spec.Width = width;
        spec.Height = height;
        spec.Attachments = { FramebufferTextureFormat::RGBA16F };

        m_PingFBO = Framebuffer::Create(spec);
        m_PongFBO = Framebuffer::Create(spec);

        // 初始化所有效果
        for (auto& effect : m_Effects)
        {
            effect->Init();
        }
    }

    void PostProcessStack::Shutdown()
    {
        m_Effects.clear();
        m_PingFBO.reset();
        m_PongFBO.reset();
    }

    void PostProcessStack::AddEffect(const Ref<PostProcessEffect>& effect)
    {
        m_Effects.push_back(effect);
        
        // 如果 Stack 已经初始化（Ping-Pong FBO 已创建），立即初始化效果
        if (m_PingFBO)
        {
            effect->Init();
        }
    }

    void PostProcessStack::RemoveEffect(const std::string& name)
    {
        std::erase_if(m_Effects, [&name](const Ref<PostProcessEffect>& effect)
        {
            return effect->GetName() == name;
        });
    }

    uint32_t PostProcessStack::Execute(uint32_t sourceTexture, PostProcessSpace space, uint32_t width, uint32_t height)
    {
        // 收集指定空间的启用效果，并按 Order 排序
        std::vector<Ref<PostProcessEffect>> activeEffects;
        for (auto& effect : m_Effects)
        {
            if (effect->Enabled && effect->GetSpace() == space)
            {
                activeEffects.push_back(effect);
            }
        }

        std::sort(activeEffects.begin(), activeEffects.end(), [](const auto& a, const auto& b)
        {
	        return a->Order < b->Order;
        });

        // 如果没有启用的效果，直接返回原始纹理
        if (activeEffects.empty())
        {
            return sourceTexture;
        }

        // Ping-Pong 执行
        uint32_t currentTexture = sourceTexture;
        bool usePing = true;

        for (auto& effect : activeEffects)
        {
            Ref<Framebuffer> destFBO = usePing ? m_PingFBO : m_PongFBO;

            effect->Execute(currentTexture, destFBO, width, height);

            currentTexture = destFBO->GetColorAttachmentRendererID(0);
            usePing = !usePing;
        }

        return currentTexture;
    }

    void PostProcessStack::Resize(uint32_t width, uint32_t height)
    {
        if (m_PingFBO)
        {
            m_PingFBO->Resize(width, height);
        }
        if (m_PongFBO)
        {
            m_PongFBO->Resize(width, height);
        }

        for (auto& effect : m_Effects)
        {
            effect->Resize(width, height);
        }
    }
}