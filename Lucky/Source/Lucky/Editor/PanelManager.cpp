#include "lpch.h"
#include "PanelManager.h"

namespace Lucky
{
    PanelManager::~PanelManager()
    {
        m_Panels.clear();
    }

    PanelData* PanelManager::GetPanelData(uint32_t panelID)
    {
        return &m_Panels.at(panelID);
    }

    const PanelData* PanelManager::GetPanelData(uint32_t panelID) const
    {
        return &m_Panels.at(panelID);
    }

    void PanelManager::RemovePanel(const char* strID)
    {
        uint32_t id = Hash::GenerateFNVHash(strID);
        if (m_Panels.find(id) == m_Panels.end())
        {
            LF_CORE_ERROR("Couldn't find panel with id '{0}'", strID);
        }

        m_Panels.erase(id);
    }

    void PanelManager::OnImGuiRender()
    {
        for (auto& [id, panelData] : m_Panels)
        {
            if (panelData.IsOpen)
            {
                panelData.Panel->OnImGuiRender(panelData.Name, panelData.IsOpen);
            }
        }
    }

    void PanelManager::OnEvent(Event& e)
    {
        for (auto& [id, panelData] : m_Panels)
        {
            panelData.Panel->OnEvent(e);
        }
    }
}