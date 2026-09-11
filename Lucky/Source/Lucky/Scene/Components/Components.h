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
#include "SpriteRendererComponent.h"

#include "LightComponent.h"
#include "PostProcessVolumeComponent.h"
#include "CameraComponent.h"
#include "ScriptComponent.h"

namespace Lucky
{
    // ======== ComponentTrait 特化 ========

    template<> struct ComponentTrait<NameComponent>
    {
        static constexpr ComponentType Type = ComponentType::Name;
    };

    template<> struct ComponentTrait<RelationshipComponent>
    {
        static constexpr ComponentType Type = ComponentType::Relationship;
    };

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

    template<> struct ComponentTrait<SpriteRendererComponent>
    {
        static constexpr ComponentType Type = ComponentType::SpriteRenderer;
    };

    template<> struct ComponentTrait<PostProcessVolumeComponent>
    {
        static constexpr ComponentType Type = ComponentType::PostProcessVolume;
    };

    template<> struct ComponentTrait<CameraComponent>
    {
        static constexpr ComponentType Type = ComponentType::Camera;
    };

    template<> struct ComponentTrait<ScriptComponent>
    {
        static constexpr ComponentType Type = ComponentType::Script;
    };
}