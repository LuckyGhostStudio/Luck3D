#pragma once

// 组件类型
#include "ComponentType.h"

// 组件
#include "IDComponent.h"
#include "NameComponent.h"
#include "TransformComponent.h"
#include "RelationshipComponent.h"

#include "MeshFilterComponent.h"
#include "MeshRendererComponent.h"

#include "LightComponent.h"
#include "PostProcessVolumeComponent.h"

namespace Lucky
{
    // ======== ComponentTrait 特化 ========

    template<> struct ComponentTrait<TransformComponent>
    {
        static constexpr ComponentType Type = ComponentType::Transform;
    };

    template<> struct ComponentTrait<LightComponent>
    {
        static constexpr ComponentType Type = ComponentType::Light;
    };

    template<> struct ComponentTrait<MeshFilterComponent>
    {
        static constexpr ComponentType Type = ComponentType::MeshFilter;
    };

    template<> struct ComponentTrait<MeshRendererComponent>
    {
        static constexpr ComponentType Type = ComponentType::MeshRenderer;
    };

    template<> struct ComponentTrait<PostProcessVolumeComponent>
    {
        static constexpr ComponentType Type = ComponentType::PostProcessVolume;
    };
}