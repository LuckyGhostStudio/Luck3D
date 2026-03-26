#include "lcpch.h"
#include "SelectionManager.h"

namespace Lucky
{
    void SelectionManager::Select(UUID selectionID)
    {
        s_SelectionID = selectionID;
    }

    bool SelectionManager::IsSelected(UUID selectionID)
    {
        return s_SelectionID == selectionID;
    }

    void SelectionManager::Deselect()
    {
        s_SelectionID = 0;
    }

    UUID SelectionManager::GetSelection()
    {
        return s_SelectionID;
    }
}