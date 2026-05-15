#pragma once

#include "Lucky/Renderer/Mesh.h"
#include "Lucky/Renderer/MeshFactory.h"

namespace Lucky
{
    using MeshRef = Ref<Mesh>;
    
    struct MeshFilterComponent
    {
        MeshRef Mesh;                                       // 运行时 Mesh 实例（Mesh 继承 Asset，内含 Handle）
        PrimitiveType Primitive = PrimitiveType::None;      // 内置图元类型（None 表示使用外部模型）
        
        MeshFilterComponent() = default;
        MeshFilterComponent(const MeshFilterComponent& other) = default;
        MeshFilterComponent(const MeshRef& mesh)
            : Mesh(mesh) {}
        
        MeshFilterComponent(PrimitiveType primitiveType)
            : Primitive(primitiveType)
        {
            Mesh = MeshFactory::CreatePrimitive(primitiveType);
        }
    };
}
