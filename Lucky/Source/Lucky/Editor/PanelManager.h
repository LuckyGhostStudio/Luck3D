#pragma once

#include "Lucky/Core/Base.h"
#include "Lucky/Core/Hash.h"

#include "EditorPanel.h"

namespace Lucky
{
    struct PanelData
    {
        const char* ID = "";
        const char* Name = "";
        Ref<EditorPanel> Panel = nullptr;
        bool IsOpen = false;
    };

    /// <summary>
    /// ±à¼­Æ÷Ãæ°å¹ÜÀíÆ÷
    /// </summary>
    class PanelManager
    {
    public:
        PanelManager() = default;
        ~PanelManager();

        PanelData* GetPanelData(uint32_t panelID);
        const PanelData* GetPanelData(uint32_t panelID) const;

        void RemovePanel(const char* strID);

        void OnImGuiRender();

        void OnEvent(Event& e);
    public:
        template<typename TPanel>
        Ref<TPanel> AddPanel(const PanelData& panelData)
        {
            static_assert(std::is_base_of<EditorPanel, TPanel>::value, "PanelManager::AddPanel requires TPanel to inherit from EditorPanel");

            uint32_t id = Hash::GenerateFNVHash(panelData.ID);
            if (m_Panels.find(id) != m_Panels.end())
            {
                LF_CORE_ERROR("A panel with the id '{0}' has already been added.", panelData.ID);
                return nullptr;
            }

            m_Panels[id] = panelData;
            return RefAs<EditorPanel, TPanel>(panelData.Panel);
        }

        template<typename TPanel, typename... Args>
        Ref<TPanel> AddPanel(const char* strID, bool isOpenByDefault, Args&&... args)
        {
            return AddPanel<TPanel>(PanelData{ strID, strID, CreateRef<TPanel>(std::forward<Args>(args)...), isOpenByDefault });
        }

        template<typename TPanel, typename... Args>
        Ref<TPanel> AddPanel(const char* strID, const char* displayName, bool isOpenByDefault, Args&&... args)
        {
            return AddPanel<TPanel>(PanelData{ strID, displayName, CreateRef<TPanel>(std::forward<Args>(args)...), isOpenByDefault });
        }

        template<typename TPanel>
        Ref<TPanel> GetPanel(const char* strID)
        {
            static_assert(std::is_base_of<EditorPanel, TPanel>::value, "PanelManager::AddPanel requires TPanel to inherit from EditorPanel");

            uint32_t id = Hash::GenerateFNVHash(strID);

            if (m_Panels.find(id) == m_Panels.end())
            {
                LF_CORE_ERROR("Couldn't find panel with id '{0}'", strID);
            }

            return RefAs<EditorPanel, TPanel>(m_Panels.at(id).Panel);
        }
    private:
        std::unordered_map<uint32_t, PanelData> m_Panels;
    };
}