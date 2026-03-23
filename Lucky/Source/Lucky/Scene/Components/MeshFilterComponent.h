#pragma once

#include "Lucky/Renderer/Mesh.h"

namespace Lucky
{
    using MeshRef = Ref<Mesh>;
    
    struct MeshFilterComponent
    {
        MeshRef Mesh;
        
        MeshFilterComponent() = default;
        MeshFilterComponent(const MeshFilterComponent& other) = default;
        MeshFilterComponent(const MeshRef& mesh)
            : Mesh(mesh) {}
    };
}
