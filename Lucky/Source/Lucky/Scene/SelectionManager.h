#pragma once

#include "Lucky/Scene/Entity.h"

namespace Lucky
{
    /// <summary>
    /// 选中项管理器
    /// </summary>
    struct SelectionManager
    {
    private:
        inline static UUID s_SelectionID;
    public:
        static void Select(UUID selectionID);
        static bool IsSelected(UUID selectionID);
        static void Deselect();
        static UUID GetSelection();
    };
}