#pragma once

#include "Lucky/Renderer/Mesh.h"
#include "Lucky/Renderer/MeshFactory.h"

namespace Lucky
{
    using MeshRef = Ref<Mesh>;
    
    struct MeshFilterComponent
    {
        MeshRef Mesh;
        PrimitiveType Primitive = PrimitiveType::None;  // Temp
        
        MeshFilterComponent() = default;
        MeshFilterComponent(const MeshFilterComponent& other) = default;
        MeshFilterComponent(const MeshRef& mesh)
            : Mesh(mesh) {}
        
        MeshFilterComponent(PrimitiveType primitive)
            : Primitive(primitive)
        {
            switch (primitive)
            {
                case PrimitiveType::Cube:
                {
                    Mesh = MeshFactory::CreateCube();
                    Mesh->SetName("Cube");
                    break;
                }
                default:
                    break;
            }
        }
    };
}
