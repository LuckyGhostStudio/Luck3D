#pragma once

#include "Lucky/Renderer/Material.h"

namespace Lucky
{
    class MaterialEditor
    {
    public:
        static void OnGUI(const Ref<Material>& material);
    };
}