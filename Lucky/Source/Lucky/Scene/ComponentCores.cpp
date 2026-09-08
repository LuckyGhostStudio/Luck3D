#include "lcpch.h"

#include "ComponentRegistry.h"
#include "Entity.h"
#include "Components/Components.h"

namespace Lucky
{
    namespace
    {
        /// <summary>
        /// 通用组件深拷贝：从 srcEntity 拷贝 T 类型组件到 dstEntity（若存在）
        /// 直接走 entt::registry::emplace_or_replace，不触发 OnComponentAdded
        /// </summary>
        template<typename TComponent>
        void CopyComponentValue(entt::registry& dst, entt::entity dstEnt, entt::registry& src, entt::entity srcEnt)
        {
            if (src.has<TComponent>(srcEnt))
            {
                const TComponent& srcComp = src.get<TComponent>(srcEnt);
                dst.emplace_or_replace<TComponent>(dstEnt, srcComp);
            }
        }
    }

    void ComponentRegistry::RegisterAllCores()
    {
        // 注册顺序即 Inspector / Serializer 输出顺序
        // 保持与 InspectorPanel / SceneSerializer 中的原枚举顺序一致，避免 YAML 键序变化

        // ---- NameComponent ----
        {
            ComponentDescriptor desc;
            desc.Type = ComponentType::Name;
            desc.Name = "Name";
            desc.Copy = &CopyComponentValue<NameComponent>;
            desc.Has = [](Entity e) { return e.HasComponent<NameComponent>(); };
            RegisterCore(std::move(desc));
        }

        // ---- TransformComponent ----
        {
            ComponentDescriptor desc;
            desc.Type = ComponentType::Transform;
            desc.Name = "Transform";
            desc.Copy = &CopyComponentValue<TransformComponent>;
            desc.Has = [](Entity e) { return e.HasComponent<TransformComponent>(); };
            RegisterCore(std::move(desc));
        }

        // ---- RelationshipComponent ----
        {
            ComponentDescriptor desc;
            desc.Type = ComponentType::Relationship;
            desc.Name = "Relationship";
            desc.Copy = &CopyComponentValue<RelationshipComponent>;
            desc.Has = [](Entity e) { return e.HasComponent<RelationshipComponent>(); };
            RegisterCore(std::move(desc));
        }

        // ---- LightComponent ----
        {
            ComponentDescriptor desc;
            desc.Type = ComponentType::Light;
            desc.Name = "Light";
            desc.Copy = &CopyComponentValue<LightComponent>;
            desc.Has = [](Entity e) { return e.HasComponent<LightComponent>(); };
            RegisterCore(std::move(desc));
        }

        // ---- MeshFilterComponent ----
        {
            ComponentDescriptor desc;
            desc.Type = ComponentType::MeshFilter;
            desc.Name = "Mesh Filter";
            desc.Copy = &CopyComponentValue<MeshFilterComponent>;
            desc.Has = [](Entity e) { return e.HasComponent<MeshFilterComponent>(); };
            RegisterCore(std::move(desc));
        }

        // ---- MeshRendererComponent ----
        {
            ComponentDescriptor desc;
            desc.Type = ComponentType::MeshRenderer;
            desc.Name = "Mesh Renderer";
            desc.Copy = &CopyComponentValue<MeshRendererComponent>;
            desc.Has = [](Entity e) { return e.HasComponent<MeshRendererComponent>(); };
            RegisterCore(std::move(desc));
        }

        // ---- SpriteRendererComponent ----
        {
            ComponentDescriptor desc;
            desc.Type = ComponentType::SpriteRenderer;
            desc.Name = "Sprite Renderer";
            desc.Copy = &CopyComponentValue<SpriteRendererComponent>;
            desc.Has = [](Entity e) { return e.HasComponent<SpriteRendererComponent>(); };
            RegisterCore(std::move(desc));
        }

        // ---- PostProcessVolumeComponent ----
        {
            ComponentDescriptor desc;
            desc.Type = ComponentType::PostProcessVolume;
            desc.Name = "Post Process Volume";
            desc.Copy = &CopyComponentValue<PostProcessVolumeComponent>;
            desc.Has = [](Entity e) { return e.HasComponent<PostProcessVolumeComponent>(); };
            RegisterCore(std::move(desc));
        }

        // ---- CameraComponent ----
        {
            ComponentDescriptor desc;
            desc.Type = ComponentType::Camera;
            desc.Name = "Camera";
            desc.Copy = &CopyComponentValue<CameraComponent>;
            desc.Has = [](Entity e) { return e.HasComponent<CameraComponent>(); };
            RegisterCore(std::move(desc));
        }
    }
}
