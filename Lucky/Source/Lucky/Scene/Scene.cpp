#include "lcpch.h"
#include "Scene.h"

#include "Lucky/Renderer/Renderer3D.h"
#include "Lucky/Renderer/Renderer2D.h"
#include "Lucky/Renderer/SceneRenderer.h"
#include "Lucky/Renderer/IBLPrecompute.h"
#include "Lucky/Renderer/TextureCube.h"

#include "Components/Components.h"
#include "ComponentRegistry.h"

#include "Entity.h"

#include "Lucky/Scripting/ScriptEngine.h"

namespace Lucky
{
    Scene::Scene(const std::string& name)
        : Asset(name)
    {
        // 从渲染器获取默认天空盒材质
        m_EnvironmentSettings.SkyboxMaterial = Renderer3D::GetDefaultSkyboxMaterial();
    }

    Scene::~Scene()
    {
        
    }

    Entity Scene::CreateEntity(const std::string& name)
    {
        return CreateEntity({}, name);
    }

    Entity Scene::CreateEntity(UUID uuid, const std::string& name)
    {
        Entity entity = { m_Registry.create(), this };  // 创建实体

        entity.AddComponent<IDComponent>(uuid);     // 添加 ID 组件（默认组件）
        entity.AddComponent<NameComponent>(name);   // 添加 Name 组件（默认组件）
        entity.AddComponent<TransformComponent>();  // 添加 Transform 组件（默认组件）
        entity.AddComponent<RelationshipComponent>();   // 添加 Relationship 组件（默认组件）

        m_EntityIDMap[uuid] = entity;   // 添加到 m_EntityIDMap

        // 无 parent 参数 → 默认为根节点，追加到根节点顺序列表末尾
        m_RootEntityOrder.push_back(uuid);

        LF_TRACE("Created Entity: [ENTT = {0}, UUID {1}, Name {2}]", static_cast<uint32_t>(entity), uuid, name);

        return entity;
    }

    Entity Scene::CreateEntity(const std::string& name, Entity parent)
    {
        return CreateEntity({}, name, parent);
    }

    Entity Scene::CreateEntity(UUID uuid, const std::string& name, Entity parent)
    {
        Entity entity = { m_Registry.create(), this };  // 创建实体

        entity.AddComponent<IDComponent>(uuid);
        entity.AddComponent<NameComponent>(name);
        entity.AddComponent<TransformComponent>();
        entity.AddComponent<RelationshipComponent>();

        m_EntityIDMap[uuid] = entity;

        if (parent)
        {
            // 有父节点：直接建立父子关系，不添加到根节点列表
            entity.SetParentUUID(parent.GetUUID());
            parent.GetChildren().push_back(uuid);
        }
        else
        {
            // 无父节点：追加到根节点列表末尾
            m_RootEntityOrder.push_back(uuid);
        }

        LF_TRACE("Created Entity: [ENTT = {0}, UUID {1}, Name {2}]", static_cast<uint32_t>(entity), uuid, name);

        return entity;
    }

    void Scene::DestroyEntity(Entity entity)
    {
        if (!entity)
        {
            return;
        }

        // 拷贝子节点列表后再遍历：避免遍历过程中原始列表被修改导致跳过子节点
        std::vector<UUID> children = entity.GetChildren();
        for (UUID childID : children)
        {
            Entity child = GetEntityWithUUID(childID);
            DestroyEntity(child);
        }
        
        // 将当前节点从父节点移除
        Entity parent = entity.GetParent();
        if (parent)
        {
            parent.RemoveChild(entity);
        }
        
        UUID id = entity.GetUUID();

        // 如果是根节点，从根节点列表移除（非根节点为 no-op）
        RemoveRootEntity(id);
        
        LF_TRACE("Removed Entity: [ENTT = {0}, UUID {1}, Name {2}]", static_cast<uint32_t>(entity), id, entity.GetName());
        
        m_Registry.destroy(entity);
        m_EntityIDMap.erase(id);    // 从 m_EntityIDMap 移除
    }

    Ref<Scene> Scene::Copy(const Ref<Scene>& other)
    {
        LF_CORE_ASSERT(other, "Scene::Copy - source scene must not be null");

        Ref<Scene> newScene = CreateRef<Scene>(other->GetName());

        // ---- Step A：场景级字段 ----
        newScene->m_ViewportWidth = other->m_ViewportWidth;
        newScene->m_ViewportHeight = other->m_ViewportHeight;
        newScene->m_EnvironmentSettings = other->m_EnvironmentSettings;

        entt::registry& srcRegistry = other->m_Registry;
        entt::registry& dstRegistry = newScene->m_Registry;

        // ---- Step B：按 UUID 建实体（不拷组件） ----
        // 建完后 m_EntityIDMap 就位；CreateEntity 会把每个实体追加到 m_RootEntityOrder 末尾，顺序在 Step D 修正
        auto idView = srcRegistry.view<IDComponent>();
        for (auto srcEntity : idView)
        {
            UUID uuid = srcRegistry.get<IDComponent>(srcEntity).ID;
            const std::string& name = srcRegistry.get<NameComponent>(srcEntity).Name;
            newScene->CreateEntity(uuid, name);
        }

        // ---- Step C：逐组件类型值拷贝 ----
        // 从 ComponentRegistry 取得全部已注册组件的 CopyFn
        // 不走 Entity::AddComponent，避免触发 OnComponentAdded 特化对副本"再初始化"（会破坏 MeshRenderer 材质列表 / CameraComponent aspect 等已稳定的数据）
        for (auto srcEntity : idView)
        {
            UUID uuid = srcRegistry.get<IDComponent>(srcEntity).ID;
            entt::entity dstEntity = static_cast<entt::entity>(newScene->m_EntityIDMap.at(uuid));

            ComponentRegistry::ForEach([&](const ComponentDescriptor& desc)
            {
                if (desc.Copy)
                {
                    desc.Copy(dstRegistry, dstEntity, srcRegistry, srcEntity);
                }
            });
        }

        // ---- Step D：重建根节点顺序 ----
        newScene->m_RootEntityOrder = other->m_RootEntityOrder;

        return newScene;
    }
    
    void Scene::UpdateTransformHierarchy()
    {
        auto view = m_Registry.view<TransformComponent, RelationshipComponent>();
        for (auto entityID : view)
        {
            auto& relationship = view.get<RelationshipComponent>(entityID);

            // 只处理根节点（没有父节点的实体）
            if (relationship.Parent == 0)
            {
                UpdateWorldTransformRecursive(Entity{ entityID, this }, glm::mat4(1.0f));
            }
        }
    }

    void Scene::UpdateWorldTransformRecursive(Entity entity, const glm::mat4& parentWorldTransform)
    {
        auto& transform = entity.GetComponent<TransformComponent>();

        // 计算世界矩阵 = 父世界矩阵 × 局部矩阵
        transform.WorldTransform = parentWorldTransform * transform.GetLocalTransform();

        // 递归更新所有子节点
        for (UUID childID : entity.GetChildren())
        {
            Entity child = TryGetEntityWithUUID(childID);
            if (child)
            {
                UpdateWorldTransformRecursive(child, transform.WorldTransform);
            }
        }
    }

    void Scene::OnUpdateEditor(DeltaTime dt)
    {
        UpdateTransformHierarchy();
    }

    void Scene::OnUpdateRuntime(DeltaTime dt)
    {
        if (m_State == SceneState::Play)
        {
            auto view = m_Registry.view<ScriptComponent>();
            for (entt::entity entityHandle : view)
            {
                Entity entity{ entityHandle, this };
                ScriptEngine::OnUpdateEntityScript(entity, dt);
            }
        }

        UpdateTransformHierarchy();
    }

    void Scene::OnRenderEditor(EditorCamera& camera, SceneRenderer& renderer)
    {
        CameraRenderData cam;
        cam.ViewMatrix = camera.GetViewMatrix();
        cam.ProjectionMatrix = camera.GetProjectionMatrix();
        cam.Position = camera.GetPosition();
        cam.Projection = ProjectionType::Perspective;
        cam.NearClip = camera.GetPerspectiveNearClip();
        cam.FOV = camera.GetPerspectiveVerticalFOV();
        cam.AspectRatio = camera.GetAspectRatio();

        RenderSceneImpl(cam, renderer);
    }

    void Scene::OnRenderRuntime(SceneRenderer& renderer)
    {
        Entity primary = GetPrimaryCameraEntity();
        if (!primary)
        {
            return;
        }

        const auto& transform = primary.GetComponent<TransformComponent>();
        const auto& cameraComp = primary.GetComponent<CameraComponent>();

        CameraRenderData cam;
        cam.ViewMatrix = glm::inverse(transform.GetWorldTransform());
        cam.ProjectionMatrix = cameraComp.Camera.GetProjectionMatrix();
        cam.Position = transform.GetWorldPosition();
        cam.Projection = cameraComp.Camera.GetProjectionType();
        cam.NearClip = cameraComp.Camera.GetPerspectiveNearClip();
        cam.FOV = cameraComp.Camera.GetPerspectiveVerticalFOV();
        cam.AspectRatio = cameraComp.Camera.GetAspectRatio();

        RenderSceneImpl(cam, renderer);
    }

    void Scene::OnRuntimeStart()
    {
        ScriptEngine::OnRuntimeStart(this);

        auto view = m_Registry.view<ScriptComponent>();
        for (entt::entity entityHandle : view)
        {
            Entity entity{ entityHandle, this };
            const ScriptComponent& sc = entity.GetComponent<ScriptComponent>();

            if (sc.ClassName.empty())
            {
                continue;
            }
            if (!ScriptEngine::EntityScriptClassExists(sc.ClassName))
            {
                LF_CORE_WARN("Scene::OnRuntimeStart - script class '{0}' not found for entity '{1}'", sc.ClassName, entity.GetName());
                continue;
            }

            ScriptEngine::OnCreateEntityScript(entity, sc.ClassName);
        }

        m_State = SceneState::Play;
    }

    void Scene::OnRuntimeStop()
    {
        ScriptEngine::OnRuntimeStop();
        m_State = SceneState::Edit;
    }

    void Scene::RenderSceneImpl(const CameraRenderData& cam, SceneRenderer& renderer)
    {
        // 收集所有光源数据
        LightRenderData lightData;
        
        // 收集所有光源实体（统一查询）
        {
            auto lightView = m_Registry.view<TransformComponent, LightComponent>();
            for (auto entity : lightView)
            {
                auto [transform, light] = lightView.get<TransformComponent, LightComponent>(entity);

                switch (light.Type)
                {
                    case LightType::Directional:
                    {
                        if (lightData.DirectionalLightCount >= s_MaxDirectionalLights)
                        {
                            break;
                        }

                        DirectionalLightData& dirLight = lightData.DirectionalLights[lightData.DirectionalLightCount];
                        dirLight.Direction = transform.GetWorldForward();
                        dirLight.Color = light.Color;
                        dirLight.Intensity = light.Intensity;

                        // 收集第一个方向光的阴影参数（当前仅支持单光源阴影）
                        if (lightData.DirectionalLightCount == 0)
                        {
                            lightData.DirLightShadowType = light.Shadows;
                            lightData.DirLightShadowBias = light.ShadowBias;
                            lightData.DirLightShadowStrength = light.ShadowStrength;

                            // CSM 参数
                            lightData.CascadeCount = light.CascadeCount;
                            lightData.ShadowDistance = light.ShadowDistance;
                            lightData.ShadowMapResolution = light.ShadowMapResolution;
                            for (int i = 0; i < s_MaxCascadeCount; ++i)
                            {
                                lightData.CascadeSplits[i] = light.CascadeSplits[i];
                            }
                        }

                        lightData.DirectionalLightCount++;
                        break;
                    }
                    case LightType::Point:
                    {
                        if (lightData.PointLightCount >= s_MaxPointLights)
                        {
                            break;
                        }

                        PointLightData& pointLight = lightData.PointLights[lightData.PointLightCount];
                        pointLight.Position = transform.GetWorldPosition();
                        pointLight.Color = light.Color;
                        pointLight.Intensity = light.Intensity;
                        pointLight.Range = light.Range;

                        // 收集点光源阴影参数
                        lightData.PointLightShadows[lightData.PointLightCount].Shadows = light.Shadows;
                        lightData.PointLightShadows[lightData.PointLightCount].ShadowBias = light.ShadowBias;
                        lightData.PointLightShadows[lightData.PointLightCount].ShadowStrength = light.ShadowStrength;

                        lightData.PointLightCount++;
                        break;
                    }
                    case LightType::Spot:
                    {
                        if (lightData.SpotLightCount >= s_MaxSpotLights)
                        {
                            break;
                        }

                        SpotLightData& spotLight = lightData.SpotLights[lightData.SpotLightCount];
                        spotLight.Position = transform.GetWorldPosition();
                        spotLight.Direction = transform.GetWorldForward();
                        spotLight.Color = light.Color;
                        spotLight.Intensity = light.Intensity;
                        spotLight.Range = light.Range;
                        spotLight.InnerCutoff = glm::cos(glm::radians(light.InnerCutoffAngle));
                        spotLight.OuterCutoff = glm::cos(glm::radians(light.OuterCutoffAngle));

                        // 收集聚光灯阴影参数
                        lightData.SpotLightShadows[lightData.SpotLightCount].Shadows = light.Shadows;
                        lightData.SpotLightShadows[lightData.SpotLightCount].ShadowBias = light.ShadowBias;
                        lightData.SpotLightShadows[lightData.SpotLightCount].ShadowStrength = light.ShadowStrength;

                        lightData.SpotLightCount++;
                        break;
                    }
                }
            }
        }
        
        renderer.BeginScene(cam, lightData);
        {
            // ---- 收集后处理参数 ----
            PostProcessSettings postProcessSettings;
            {
                auto volumeView = m_Registry.view<PostProcessVolumeComponent>();
                float highestPriority = -1e38f;

                for (auto entity : volumeView)
                {
                    auto& volume = volumeView.get<PostProcessVolumeComponent>(entity);

                    if (volume.IsGlobal && volume.Priority > highestPriority)
                    {
                        highestPriority = volume.Priority;

                        postProcessSettings.Tonemap = volume.Tonemap;
                        postProcessSettings.Exposure = volume.Exposure;
                        postProcessSettings.BloomEnabled = volume.BloomEnabled;
                        postProcessSettings.BloomThreshold = volume.BloomThreshold;
                        postProcessSettings.BloomIntensity = volume.BloomIntensity;
                        postProcessSettings.BloomIterations = volume.BloomIterations;
                        postProcessSettings.FXAAEnabled = volume.FXAAEnabled;
                        postProcessSettings.VignetteEnabled = volume.VignetteEnabled;
                        postProcessSettings.VignetteIntensity = volume.VignetteIntensity;
                        postProcessSettings.VignetteSmoothness = volume.VignetteSmoothness;
                    }
                }
            }
            renderer.SetPostProcessSettings(postProcessSettings);
            
            // ---- 检测 EnvironmentSettings 变化并触发 IBL 重生成 ----
            // IBL 是场景级共享资源，由 Scene 单一驱动源触发（避免多个 SceneRenderer 重复触发）
            const Ref<Material>& skyboxMat = m_EnvironmentSettings.SkyboxMaterial;
            int reflectionRes = m_EnvironmentSettings.ReflectionResolution;
            if (skyboxMat != m_LastSkyboxMaterialForIBL || reflectionRes != m_LastReflectionResolutionForIBL)
            {
                if (skyboxMat)
                {
                    Ref<TextureCube> cubemap = skyboxMat->GetTextureCube("u_SkyboxMap");
                    if (cubemap)
                    {
                        IBLPrecompute::GenerateFromCubemap(cubemap->GetRendererID(), reflectionRes);
                        LF_CORE_INFO("IBL data regenerated (skybox or resolution changed)");
                    }
                }
                m_LastSkyboxMaterialForIBL = skyboxMat;
                m_LastReflectionResolutionForIBL = reflectionRes;
            }
            
            // 传递环境设置到渲染器
            renderer.SetEnvironmentSettings(m_EnvironmentSettings);

            // 获取同时拥有 TransformComponent MeshFilterComponent MeshRendererComponent 的实体
            auto meshGroup = m_Registry.group<TransformComponent>(entt::get<MeshFilterComponent, MeshRendererComponent>);

            for (auto entity : meshGroup)
            {
                auto [transform, meshFilter, meshRenderer] = meshGroup.get<TransformComponent, MeshFilterComponent, MeshRendererComponent>(entity);

                renderer.SubmitMesh(transform.GetWorldTransform(), meshFilter.Mesh, meshRenderer.Materials, static_cast<int>(static_cast<uint32_t>(entity)));    // 绘制网格
            }

            // 收集 Sprite（TransformComponent + SpriteRendererComponent 组合）
            auto spriteView = m_Registry.view<TransformComponent, SpriteRendererComponent>();
            for (auto entity : spriteView)
            {
                auto [transform, sprite] = spriteView.get<TransformComponent, SpriteRendererComponent>(entity);

                renderer.SubmitSprite(
                    transform.GetWorldTransform(),
                    sprite.Texture,
                    sprite.Color,
                    sprite.FlipX,
                    sprite.FlipY,
                    sprite.UVRect,
                    sprite.TilingFactor,
                    sprite.Material,
                    sprite.SortingOrder,
                    static_cast<int>(static_cast<uint32_t>(entity)));
            }
        }
        renderer.EndScene();
    }

    void Scene::OnViewportResize(uint32_t width, uint32_t height)
    {
        m_ViewportWidth = width;
        m_ViewportHeight = height;

        auto view = m_Registry.view<CameraComponent>();
        for (auto entity : view)
        {
            auto& cam = view.get<CameraComponent>(entity);
            if (!cam.FixedAspectRatio)
            {
                cam.Camera.SetViewportSize(width, height);
            }
        }
    }

    Entity Scene::GetPrimaryCameraEntity()
    {
        auto view = m_Registry.view<TransformComponent, CameraComponent>();
        for (auto entity : view)
        {
            const auto& cam = view.get<CameraComponent>(entity);
            if (cam.Primary)
            {
                return Entity{ entity, this };
            }
        }
        return Entity{};
    }

    Entity Scene::GetEntityWithUUID(UUID id)
    {
        // Temp Debug
        if (m_EntityIDMap.find(id) == m_EntityIDMap.end())
        {
	        LF_CORE_ERROR("Invalid entity ID {0} or entity doesn't exist in scene", id);
        }
        
        LF_CORE_ASSERT(m_EntityIDMap.find(id) != m_EntityIDMap.end(), "Invalid entity ID or entity doesn't exist in scene!");
        return m_EntityIDMap.at(id);
    }

    Entity Scene::TryGetEntityWithUUID(UUID id)
    {
        if (const auto it = m_EntityIDMap.find(id); it != m_EntityIDMap.end())
        {
            return it->second;
        }
        return Entity{};
    }

    // ---- 根节点顺序管理 ----

    void Scene::InsertRootEntity(UUID entityID, int index)
    {
        // 先确保不重复（同级排序场景会先删后插）
        auto it = std::find(m_RootEntityOrder.begin(), m_RootEntityOrder.end(), entityID);
        if (it != m_RootEntityOrder.end())
        {
            m_RootEntityOrder.erase(it);
        }

        if (index < 0 || index >= static_cast<int>(m_RootEntityOrder.size()))
        {
            m_RootEntityOrder.push_back(entityID);
        }
        else
        {
            m_RootEntityOrder.insert(m_RootEntityOrder.begin() + index, entityID);
        }
    }

    void Scene::RemoveRootEntity(UUID entityID)
    {
        auto it = std::find(m_RootEntityOrder.begin(), m_RootEntityOrder.end(), entityID);
        if (it != m_RootEntityOrder.end())
        {
            m_RootEntityOrder.erase(it);
        }
    }

    int Scene::GetEntityIndexInParent(Entity entity)
    {
        if (!entity)
        {
            return -1;
        }

        UUID id = entity.GetUUID();
        Entity parent = entity.GetParent();

        if (parent)
        {
            const std::vector<UUID>& siblings = parent.GetChildren();
            auto it = std::find(siblings.begin(), siblings.end(), id);
            if (it != siblings.end())
            {
                return static_cast<int>(std::distance(siblings.begin(), it));
            }
        }
        else
        {
            auto it = std::find(m_RootEntityOrder.begin(), m_RootEntityOrder.end(), id);
            if (it != m_RootEntityOrder.end())
            {
                return static_cast<int>(std::distance(m_RootEntityOrder.begin(), it));
            }
        }

        return -1;
    }

    void Scene::ClearAllEntities()
    {
        for (auto& [uuid, entity] : m_EntityIDMap)
        {
            DestroyEntity(entity);
        }

        // 兵底清理：DestroyEntity 中的 RemoveRootEntity 已逐一清空，此处保险重置
        m_RootEntityOrder.clear();
    }

    template<typename TComponent>
    void Scene::OnComponentAdded(Entity entity, TComponent& component)
    {
        static_assert(sizeof(TComponent) == 0);
    }

    template<>
    void Scene::OnComponentAdded<IDComponent>(Entity entity, IDComponent& component)
    {

    }
    
    template<>
    void Scene::OnComponentAdded<NameComponent>(Entity entity, NameComponent& component)
    {

    }
    
    template<>
    void Scene::OnComponentAdded<TransformComponent>(Entity entity, TransformComponent& component)
    {

    }
    
    template<>
    void Scene::OnComponentAdded<MeshFilterComponent>(Entity entity, MeshFilterComponent& component)
    {

    }

    template<>
    void Scene::OnComponentAdded<RelationshipComponent>(Entity entity, RelationshipComponent& component)
    {

    }
    
    template<>
    void Scene::OnComponentAdded<MeshRendererComponent>(Entity entity, MeshRendererComponent& component)
    {
        // 根据 SubMesh 数量初始化材质列表
        if (entity.HasComponent<MeshFilterComponent>())
        {
            auto& meshFilter = entity.GetComponent<MeshFilterComponent>();
            if (meshFilter.Mesh)
            {
                // 查找最大的 MaterialIndex
                uint32_t requiredCount = 0;
                for (const SubMesh& sm : meshFilter.Mesh->GetSubMeshes())
                {
                    requiredCount = std::max(requiredCount, sm.MaterialIndex + 1);
                }

                // 不存在的材质使用默认材质
                component.Materials.resize(requiredCount);
                for (auto& mat : component.Materials)
                {
                    if (!mat)
                    {
                        mat = Renderer3D::GetDefaultMaterial();
                    }
                }
            }
        }
    }
    
    template<>
    void Scene::OnComponentAdded<LightComponent>(Entity entity, LightComponent& component)
    {
        
    }
    
    template<>
    void Scene::OnComponentAdded<PostProcessVolumeComponent>(Entity entity, PostProcessVolumeComponent& component)
    {
        
    }
    
    template<>
    void Scene::OnComponentAdded<SpriteRendererComponent>(Entity entity, SpriteRendererComponent& component)
    {
        
        if (!component.Material)
        {
            component.Material = Renderer2D::GetDefaultMaterial();
        }
    }
    
    template<>
    void Scene::OnComponentAdded<CameraComponent>(Entity entity, CameraComponent& component)
    {
        if (m_ViewportWidth > 0 && m_ViewportHeight > 0)
        {
            component.Camera.SetViewportSize(m_ViewportWidth, m_ViewportHeight);
        }
    }

    template<>
    void Scene::OnComponentAdded<ScriptComponent>(Entity entity, ScriptComponent& component)
    {

    }
    
    // TODO 添加新组件
}