#include "lcpch.h"
#include "SceneRenderer.h"

#include "Renderer3D.h"
#include "RenderPipeline.h"
#include "IBLPrecompute.h"
#include "ShadowAtlas.h"

#include "Passes/ShadowPass.h"
#include "Passes/OpaquePass.h"
#include "Passes/SkyboxPass.h"
#include "Passes/TransparentPass.h"
#include "Passes/Sprite2DPass.h"
#include "Passes/PickingPass.h"
#include "Passes/DebugVisualizePass.h"
#include "Passes/PostProcessPass.h"
#include "Passes/SilhouettePass.h"
#include "Passes/OutlineCompositePass.h"

#include "Effects/BloomEffect.h"
#include "Effects/FXAAEffect.h"
#include "Effects/VignetteEffect.h"

#include "Mesh.h"
#include "Material.h"
#include "Texture.h"

#include <glm/ext/matrix_transform.hpp>
#include <glm/ext/matrix_clip_space.hpp>

#include <algorithm>
#include <array>
#include <limits>

namespace Lucky
{
    SceneRenderer* SceneRenderer::s_Primary = nullptr;

    /// <summary>
    /// 计算点光源 6 面的 Light Space Matrix
    /// 使用 90° FOV 透视投影，覆盖 Cubemap 的每个面
    /// </summary>
    static std::array<glm::mat4, 6> CalcPointLightMatrices(const glm::vec3& lightPos, float farPlane)
    {
        float nearPlane = 0.1f;
        glm::mat4 projection = glm::perspective(glm::radians(90.0f), 1.0f, nearPlane, farPlane);

        std::array<glm::mat4, 6> matrices;

        matrices[0] = projection * glm::lookAt(lightPos, lightPos + glm::vec3( 1.0f, 0.0f, 0.0f), glm::vec3(0.0f, -1.0f, 0.0f));
        matrices[1] = projection * glm::lookAt(lightPos, lightPos + glm::vec3(-1.0f, 0.0f, 0.0f), glm::vec3(0.0f, -1.0f, 0.0f));
        matrices[2] = projection * glm::lookAt(lightPos, lightPos + glm::vec3( 0.0f, 1.0f, 0.0f), glm::vec3(0.0f, 0.0f,  1.0f));
        matrices[3] = projection * glm::lookAt(lightPos, lightPos + glm::vec3( 0.0f,-1.0f, 0.0f), glm::vec3(0.0f, 0.0f, -1.0f));
        matrices[4] = projection * glm::lookAt(lightPos, lightPos + glm::vec3( 0.0f, 0.0f, 1.0f), glm::vec3(0.0f, -1.0f, 0.0f));
        matrices[5] = projection * glm::lookAt(lightPos, lightPos + glm::vec3( 0.0f, 0.0f,-1.0f), glm::vec3(0.0f, -1.0f, 0.0f));

        return matrices;
    }

    void SceneRenderer::Init(const SceneRendererSpec& spec)
    {
        m_Spec = spec;

        FramebufferSpecification fbSpec;
        if (m_Spec.EnablePicking)
        {
            fbSpec.Attachments =
            {
                FramebufferTextureFormat::RGBA8,
                FramebufferTextureFormat::RED_INTEGER,
                FramebufferTextureFormat::Depth
            };
        }
        else
        {
            fbSpec.Attachments =
            {
                FramebufferTextureFormat::RGBA8,
                FramebufferTextureFormat::Depth
            };
        }
        fbSpec.Width = m_Spec.Width;
        fbSpec.Height = m_Spec.Height;

        m_Framebuffer = Framebuffer::Create(fbSpec);

        m_CameraUniformBuffer = UniformBuffer::Create(sizeof(CameraUBOData), 0);
        m_LightUniformBuffer = UniformBuffer::Create(sizeof(LightUBOData), 1);

        BuildPipeline();
    }

    void SceneRenderer::BuildPipeline()
    {
        // Shadow 分组：可选（缩略图 / 反射探针烘焙时关闭）
        if (m_Spec.EnableShadow)
        {
            m_Pipeline.AddPass(CreateRef<ShadowPass>());
        }

        // Main 分组：Opaque / Skybox / Transparent / Sprite2D 始终需要
        m_Pipeline.AddPass(CreateRef<OpaquePass>());
        m_Pipeline.AddPass(CreateRef<SkyboxPass>());
        m_Pipeline.AddPass(CreateRef<TransparentPass>());
        m_Pipeline.AddPass(CreateRef<Sprite2DPass>());

        // Picking 分组：可选
        if (m_Spec.EnablePicking)
        {
            m_Pipeline.AddPass(CreateRef<PickingPass>());
        }

        // Debug 分组：可选
        if (m_Spec.EnableDebugVisualize)
        {
            m_Pipeline.AddPass(CreateRef<DebugVisualizePass>());
        }

        // PostProcess 分组：可选
        if (m_Spec.EnablePostProcess)
        {
            auto postProcessPass = CreateRef<PostProcessPass>();
            m_Pipeline.AddPass(postProcessPass);

            auto bloomEffect = CreateRef<BloomEffect>();
            bloomEffect->Order = 0;
            bloomEffect->Enabled = false;
            auto vignetteEffect = CreateRef<VignetteEffect>();
            vignetteEffect->Order = 10;
            vignetteEffect->Enabled = false;
            auto fxaaEffect = CreateRef<FXAAEffect>();
            fxaaEffect->Order = 0;
            fxaaEffect->Enabled = false;

            postProcessPass->GetPostProcessStack().AddEffect(bloomEffect);
            postProcessPass->GetPostProcessStack().AddEffect(vignetteEffect);
            postProcessPass->GetPostProcessStack().AddEffect(fxaaEffect);
        }

        // Outline 分组：可选
        if (m_Spec.EnableOutline)
        {
            auto silhouettePass = CreateRef<SilhouettePass>();
            auto outlineCompositePass = CreateRef<OutlineCompositePass>();
            outlineCompositePass->SetSilhouettePass(silhouettePass);
            m_Pipeline.AddPass(silhouettePass);
            m_Pipeline.AddPass(outlineCompositePass);
        }

        m_Pipeline.Init();
    }

    void SceneRenderer::Shutdown()
    {
        m_Pipeline.Shutdown();
        m_CameraUniformBuffer.reset();
        m_LightUniformBuffer.reset();
        m_Framebuffer.reset();
    }

    void SceneRenderer::OnViewportResize(uint32_t width, uint32_t height)
    {
        if (width == 0 || height == 0)
        {
            return;
        }

        m_Framebuffer->Resize(width, height);
        m_Pipeline.Resize(width, height);
    }

    void SceneRenderer::BeginScene(const CameraRenderData& cam, const LightRenderData& lightData)
    {
        // 每帧重置本实例统计
        m_Stats = {};

        // 把本实例的 UBO 重新绑定到自己的 binding slot
        // 多个 SceneRenderer 共用相同的 binding 索引，谁 BeginScene 谁接管
        m_CameraUniformBuffer->Bind();
        m_LightUniformBuffer->Bind();

        // ---- 设置 Camera UBO ----
        m_CameraBuffer.ViewProjectionMatrix = cam.ProjectionMatrix * cam.ViewMatrix;
        m_CameraBuffer.InvProjectionMatrix  = glm::inverse(cam.ProjectionMatrix);
        m_CameraBuffer.Position = cam.Position;
        m_CameraUniformBuffer->SetData(&m_CameraBuffer, sizeof(CameraUBOData));

        // ---- 设置 Light UBO ----
        m_LightBuffer.DirectionalLightCount = lightData.DirectionalLightCount;
        m_LightBuffer.PointLightCount = lightData.PointLightCount;
        m_LightBuffer.SpotLightCount = lightData.SpotLightCount;

        for (int i = 0; i < lightData.DirectionalLightCount; ++i)
        {
            m_LightBuffer.DirectionalLights[i] = lightData.DirectionalLights[i];
        }
        for (int i = 0; i < lightData.PointLightCount; ++i)
        {
            m_LightBuffer.PointLights[i] = lightData.PointLights[i];
        }
        for (int i = 0; i < lightData.SpotLightCount; ++i)
        {
            m_LightBuffer.SpotLights[i] = lightData.SpotLights[i];
        }
        m_LightUniformBuffer->SetData(&m_LightBuffer, sizeof(LightUBOData));

        // ======== CSM 计算（仅透视投影下计算；正交投影自动关闭方向光阴影） ========
        m_ShadowEnabled = false;
        if (cam.Projection == ProjectionType::Perspective
            && lightData.DirectionalLightCount > 0
            && lightData.DirLightShadowType != ShadowType::None)
        {
            m_ShadowEnabled = true;
            m_ShadowBias = lightData.DirLightShadowBias;
            m_ShadowStrength = lightData.DirLightShadowStrength;
            m_ShadowShadowType = lightData.DirLightShadowType;
            m_CascadeCount = lightData.CascadeCount;
            m_ShadowMapResolution = lightData.ShadowMapResolution;

            glm::vec3 lightDir = glm::normalize(lightData.DirectionalLights[0].Direction);

            float cameraNear = cam.NearClip;

            float cascadeNearPlanes[s_MaxCascadeCount];
            float cascadeFarPlanes[s_MaxCascadeCount];

            for (int i = 0; i < m_CascadeCount; ++i)
            {
                cascadeNearPlanes[i] = (i == 0) ? cameraNear : cascadeFarPlanes[i - 1];
                cascadeFarPlanes[i] = cameraNear + lightData.ShadowDistance * lightData.CascadeSplits[i];
                m_CascadeFarPlanes[i] = cascadeFarPlanes[i];
            }

            float fov = cam.FOV;
            float aspectRatio = cam.AspectRatio;
            glm::mat4 cameraView = cam.ViewMatrix;

            for (int i = 0; i < m_CascadeCount; ++i)
            {
                // 1. 计算子视锥体的 8 个角点（世界空间）
                glm::mat4 subProjection = glm::perspective(glm::radians(fov), aspectRatio, cascadeNearPlanes[i], cascadeFarPlanes[i]);
                glm::mat4 invVP = glm::inverse(subProjection * cameraView);

                std::array<glm::vec3, 8> corners;
                int index = 0;
                for (int x = 0; x <= 1; ++x)
                {
                    for (int y = 0; y <= 1; ++y)
                    {
                        for (int z = 0; z <= 1; ++z)
                        {
                            glm::vec4 pt = invVP * glm::vec4(
                                2.0f * x - 1.0f,
                                2.0f * y - 1.0f,
                                2.0f * z - 1.0f,
                                1.0f
                            );
                            corners[index++] = glm::vec3(pt) / pt.w;
                        }
                    }
                }

                // 2. 子视锥体中心
                glm::vec3 center(0.0f);
                for (const auto& corner : corners)
                {
                    center += corner;
                }
                center /= 8.0f;

                // 3. 光源视图矩阵
                glm::vec3 up = glm::vec3(0.0f, 1.0f, 0.0f);
                if (std::abs(glm::dot(lightDir, up)) > 0.99f)
                {
                    up = glm::vec3(0.0f, 0.0f, 1.0f);
                }

                float radius = 0.0f;
                for (const auto& corner : corners)
                {
                    radius = std::max(radius, glm::length(corner - center));
                }
                glm::vec3 lightPos = center - lightDir * (radius + 50.0f);
                glm::mat4 lightView = glm::lookAt(lightPos, center, up);

                // 4. 光源空间 AABB
                float minX = std::numeric_limits<float>::max();
                float maxX = std::numeric_limits<float>::lowest();
                float minY = std::numeric_limits<float>::max();
                float maxY = std::numeric_limits<float>::lowest();
                float minZ = std::numeric_limits<float>::max();
                float maxZ = std::numeric_limits<float>::lowest();

                for (const auto& corner : corners)
                {
                    glm::vec4 lightSpaceCorner = lightView * glm::vec4(corner, 1.0f);
                    minX = std::min(minX, lightSpaceCorner.x);
                    maxX = std::max(maxX, lightSpaceCorner.x);
                    minY = std::min(minY, lightSpaceCorner.y);
                    maxY = std::max(maxY, lightSpaceCorner.y);
                    minZ = std::min(minZ, lightSpaceCorner.z);
                    maxZ = std::max(maxZ, lightSpaceCorner.z);
                }

                // 5. 扩展 Z 范围（确保光源"背后"的物体也能投射阴影）
                float zRange = maxZ - minZ;
                float zPadding = std::max(zRange * 2.0f, 50.0f);
                minZ -= zPadding;
                maxZ += zPadding * 0.1f;

                // 6. 正交投影矩阵
                glm::mat4 lightProjection = glm::ortho(minX, maxX, minY, maxY, -maxZ, -minZ);

                m_CascadeLightSpaceMatrices[i] = lightProjection * lightView;
            }
        }

        // ======== 点光源阴影矩阵计算 ========
        m_PointShadowCount = 0;
        for (int i = 0; i < lightData.PointLightCount && m_PointShadowCount < ShadowAtlas::s_MaxPointLightShadows; ++i)
        {
            if (lightData.PointLightShadows[i].Shadows != ShadowType::None)
            {
                const PointLightData& pointLight = lightData.PointLights[i];
                float farPlane = pointLight.Range;
                glm::vec3 lightPos = pointLight.Position;

                std::array<glm::mat4, 6> matrices = CalcPointLightMatrices(lightPos, farPlane);

                m_PointShadowData[m_PointShadowCount].LightIndex = i;
                m_PointShadowData[m_PointShadowCount].LightPos = lightPos;
                m_PointShadowData[m_PointShadowCount].FarPlane = farPlane;
                m_PointShadowData[m_PointShadowCount].ShadowBias = lightData.PointLightShadows[i].ShadowBias;
                m_PointShadowData[m_PointShadowCount].ShadowStrength = lightData.PointLightShadows[i].ShadowStrength;
                m_PointShadowData[m_PointShadowCount].ShadowType = static_cast<int>(lightData.PointLightShadows[i].Shadows);

                for (int face = 0; face < 6; ++face)
                {
                    m_PointShadowData[m_PointShadowCount].LightSpaceMatrices[face] = matrices[face];
                }

                ++m_PointShadowCount;
            }
        }

        // ======== 聚光灯阴影矩阵计算 ========
        m_SpotShadowCount = 0;
        for (int i = 0; i < lightData.SpotLightCount && m_SpotShadowCount < ShadowAtlas::s_MaxSpotLightShadows; ++i)
        {
            if (lightData.SpotLightShadows[i].Shadows != ShadowType::None)
            {
                const SpotLightData& spotLight = lightData.SpotLights[i];

                float halfFov = glm::acos(spotLight.OuterCutoff);
                float fov = halfFov * 2.0f;
                float aspectRatio = 1.0f;
                float nearPlane = 0.1f;
                float farPlane = spotLight.Range;

                glm::mat4 projection = glm::perspective(fov, aspectRatio, nearPlane, farPlane);

                glm::vec3 direction = glm::normalize(spotLight.Direction);
                glm::vec3 target = spotLight.Position + direction;

                glm::vec3 up = glm::vec3(0.0f, 1.0f, 0.0f);
                if (std::abs(glm::dot(direction, up)) > 0.99f)
                {
                    up = glm::vec3(0.0f, 0.0f, 1.0f);
                }

                glm::mat4 view = glm::lookAt(spotLight.Position, target, up);
                glm::mat4 lightSpaceMatrix = projection * view;

                m_SpotShadowData[m_SpotShadowCount].LightIndex = i;
                m_SpotShadowData[m_SpotShadowCount].LightSpaceMatrix = lightSpaceMatrix;
                m_SpotShadowData[m_SpotShadowCount].ShadowBias = lightData.SpotLightShadows[i].ShadowBias;
                m_SpotShadowData[m_SpotShadowCount].ShadowStrength = lightData.SpotLightShadows[i].ShadowStrength;
                m_SpotShadowData[m_SpotShadowCount].ShadowType = static_cast<int>(lightData.SpotLightShadows[i].Shadows);

                ++m_SpotShadowCount;
            }
        }

        // 清空绘制命令列表
        m_OpaqueDrawCommands.clear();
        m_TransparentDrawCommands.clear();
        m_SpriteDrawCommands.clear();

        // 缓存相机位置与矩阵
        m_CameraPosition = cam.Position;
        m_CameraViewMatrix = cam.ViewMatrix;
        m_CameraProjectionMatrix = cam.ProjectionMatrix;
    }

    void SceneRenderer::SubmitMesh(const glm::mat4& transform, Ref<Mesh>& mesh,
                                   const std::vector<Ref<Material>>& materials, int entityID)
    {
        const auto& vertices = mesh->GetVertices();
        uint32_t dataSize = sizeof(Vertex) * static_cast<uint32_t>(vertices.size());
        mesh->SetVertexBufferData(vertices.data(), dataSize);

        glm::vec3 objPos = glm::vec3(transform[3]);
        float distToCamera = glm::length(m_CameraPosition - objPos);

        for (const SubMesh& sm : mesh->GetSubMeshes())
        {
            Ref<Material> material = nullptr;
            if (sm.MaterialIndex < materials.size())
            {
                material = materials[sm.MaterialIndex];
            }

            if (!material || !material->GetShader())
            {
                material = Renderer3D::GetInternalErrorMaterial();
            }

            uint64_t shaderID = material->GetShader()->GetRendererID();
            uint64_t sortKey = (shaderID & 0xFFFF) << 48;

            DrawCommand cmd;
            cmd.Transform = transform;
            cmd.MeshData = mesh;
            cmd.SubMeshPtr = &sm;
            cmd.MaterialData = material;
            cmd.SortKey = sortKey;
            cmd.DistanceToCamera = distToCamera;
            cmd.EntityID = entityID;

            if (material->IsTransparent())
            {
                m_TransparentDrawCommands.push_back(cmd);
            }
            else
            {
                m_OpaqueDrawCommands.push_back(cmd);
            }
        }
    }

    void SceneRenderer::SubmitSprite(const glm::mat4& transform,
                                     const Ref<Texture2D>& texture,
                                     const glm::vec4& color,
                                     bool flipX,
                                     bool flipY,
                                     const glm::vec4& uvRect,
                                     float tilingFactor,
                                     const Ref<Material>& material,
                                     int sortingOrder,
                                     int entityID)
    {
        SpriteDrawCommand cmd;
        cmd.Transform = transform;
        cmd.Texture = texture;
        cmd.Color = color;
        cmd.FlipX = flipX;
        cmd.FlipY = flipY;
        cmd.UVRect = uvRect;
        cmd.TilingFactor = tilingFactor;
        cmd.MaterialData = material;
        cmd.SortingOrder = sortingOrder;
        cmd.EntityID = entityID;

        const glm::vec3 worldPos = glm::vec3(transform[3]);
        const glm::vec4 viewPos = m_CameraViewMatrix * glm::vec4(worldPos, 1.0f);
        cmd.DistanceToCamera = -viewPos.z;

        m_SpriteDrawCommands.push_back(cmd);
    }

    void SceneRenderer::EndScene()
    {
        // ---- 排序不透明物体（按 SortKey 升序，聚合相同 Shader） ----
        std::sort(m_OpaqueDrawCommands.begin(), m_OpaqueDrawCommands.end(), [](const DrawCommand& a, const DrawCommand& b)
        {
            return a.SortKey < b.SortKey;
        });

        // ---- 排序透明物体（按距离降序，从远到近） ----
        std::sort(m_TransparentDrawCommands.begin(), m_TransparentDrawCommands.end(), [](const DrawCommand& a, const DrawCommand& b)
        {
            return a.DistanceToCamera > b.DistanceToCamera;
        });

        // ---- 排序 Sprite（先 SortingOrder 升序，再距离从远到近） ----
        std::sort(m_SpriteDrawCommands.begin(), m_SpriteDrawCommands.end(), [](const SpriteDrawCommand& a, const SpriteDrawCommand& b)
        {
            if (a.SortingOrder != b.SortingOrder)
            {
                return a.SortingOrder < b.SortingOrder;
            }
            return a.DistanceToCamera > b.DistanceToCamera;
        });

        // ---- 构建 RenderContext ----
        RenderContext context;
        context.OpaqueDrawCommands = &m_OpaqueDrawCommands;
        context.TransparentDrawCommands = &m_TransparentDrawCommands;
        context.SpriteDrawCommands = &m_SpriteDrawCommands;
        context.TargetFramebuffer = m_Framebuffer;
        context.ClearColor = m_ClearColor;
        context.Stats = &m_Stats;

        // 阴影数据
        context.ShadowEnabled = m_ShadowEnabled;
        context.ShadowBias = m_ShadowBias;
        context.ShadowStrength = m_ShadowStrength;
        context.ShadowShadowType = m_ShadowShadowType;

        // CSM 数据
        context.CascadeCount = m_CascadeCount;
        context.ShadowMapResolution = m_ShadowMapResolution;
        context.CameraViewMatrix = m_CameraViewMatrix;
        context.CameraProjectionMatrix = m_CameraProjectionMatrix;
        for (int i = 0; i < m_CascadeCount; ++i)
        {
            context.CascadeLightSpaceMatrices[i] = m_CascadeLightSpaceMatrices[i];
            context.CascadeFarPlanes[i] = m_CascadeFarPlanes[i];
        }

        // 获取 Shadow Map 纹理 ID
        auto shadowPass = m_Pipeline.GetPass<ShadowPass>();
        if (shadowPass)
        {
            context.CascadeShadowMapArrayTextureID = shadowPass->GetShadowMapTextureID();
            context.TranslucentShadowMapTextureID = shadowPass->GetTranslucentShadowMapTextureID();
            bool hasTransparentObjects = context.TransparentDrawCommands && !context.TransparentDrawCommands->empty();
            context.TranslucentShadowEnabled = hasTransparentObjects;

            context.ShadowAtlasTextureID = shadowPass->GetShadowAtlasTextureID();
            context.ShadowAtlasSize = shadowPass->GetShadowAtlasSize();
        }

        // ---- 聚光灯阴影数据 ----
        context.ShadowData.SpotLightShadowCount = m_SpotShadowCount;
        for (int i = 0; i < m_SpotShadowCount; ++i)
        {
            context.ShadowData.SpotLights[i].LightIndex = m_SpotShadowData[i].LightIndex;
            context.ShadowData.SpotLights[i].LightSpaceMatrix = m_SpotShadowData[i].LightSpaceMatrix;
            context.ShadowData.SpotLights[i].ShadowBias = m_SpotShadowData[i].ShadowBias;
            context.ShadowData.SpotLights[i].ShadowStrength = m_SpotShadowData[i].ShadowStrength;
            context.ShadowData.SpotLights[i].ShadowType = m_SpotShadowData[i].ShadowType;

            if (shadowPass)
            {
                int tileIdx = shadowPass->GetShadowAtlas().GetSpotLightTileIndex(i);
                context.ShadowData.SpotLights[i].AtlasScaleBias = shadowPass->GetShadowAtlas().GetTile(tileIdx).ViewportScaleBias;
            }
        }

        // ---- 点光源阴影数据 ----
        context.ShadowData.PointLightShadowCount = m_PointShadowCount;
        for (int i = 0; i < m_PointShadowCount; ++i)
        {
            context.ShadowData.PointLights[i].LightIndex = m_PointShadowData[i].LightIndex;
            context.ShadowData.PointLights[i].LightPos = m_PointShadowData[i].LightPos;
            context.ShadowData.PointLights[i].FarPlane = m_PointShadowData[i].FarPlane;
            context.ShadowData.PointLights[i].ShadowBias = m_PointShadowData[i].ShadowBias;
            context.ShadowData.PointLights[i].ShadowStrength = m_PointShadowData[i].ShadowStrength;
            context.ShadowData.PointLights[i].ShadowType = m_PointShadowData[i].ShadowType;

            for (int face = 0; face < 6; ++face)
            {
                context.ShadowData.PointLights[i].LightSpaceMatrices[face] = m_PointShadowData[i].LightSpaceMatrices[face];

                if (shadowPass)
                {
                    int tileIdx = shadowPass->GetShadowAtlas().GetPointLightTileStart(i) + face;
                    context.ShadowData.PointLights[i].AtlasScaleBias[face] = shadowPass->GetShadowAtlas().GetTile(tileIdx).ViewportScaleBias;
                }
            }
        }

        // 天空盒数据
        context.SkyboxMaterial = m_Environment.SkyboxMaterial;
        context.SkyboxViewMatrix = m_CameraViewMatrix;
        context.SkyboxProjectionMatrix = m_CameraProjectionMatrix;

        // HDR / 后处理数据
        auto postProcessPass = m_Pipeline.GetPass<PostProcessPass>();
        if (postProcessPass)
        {
            context.HDR_FBO = postProcessPass->GetHDR_FBO();
        }
        context.PostProcess = m_PostProcess;

        // IBL 数据
        const IBLData& iblData = IBLPrecompute::GetIBLData();
        context.IBLEnabled = iblData.Valid;
        context.IrradianceMapID = iblData.IrradianceMapID;
        context.PrefilterMapID = iblData.PrefilterMapID;
        context.BRDFLUTID = iblData.BRDFLUTID;
        context.PrefilterMaxMipLevel = static_cast<float>(iblData.PrefilterMaxMipLevel);

        // 环境设置
        context.EnvironmentSource = m_Environment.Source;
        context.AmbientColor = m_Environment.AmbientColor;
        context.IBLDiffuseIntensity = m_Environment.DiffuseIntensity;
        context.IBLSpecularIntensity = m_Environment.SpecularIntensity;

        // 天空盒材质参数同步
        if (m_Environment.SkyboxMaterial)
        {
            const auto& skyMat = m_Environment.SkyboxMaterial;
            context.SkyExposure = skyMat->GetFloat("u_Exposure");
            const glm::vec4 tint4 = skyMat->GetFloat4("u_Tint");
            context.SkyTint = glm::vec3(tint4);
            context.SkyRotation = skyMat->GetFloat("u_Rotation");
        }
        else
        {
            context.SkyExposure = 1.0f;
            context.SkyTint = glm::vec3(1.0f);
            context.SkyRotation = 0.0f;
        }

        // ---- 执行 Shadow / Main / Debug / PostProcess 分组 ----
        m_Pipeline.ExecuteGroup("Shadow", context);
        m_Pipeline.ExecuteGroup("Main", context);
        m_Pipeline.ExecuteGroup("Debug", context);
        m_Pipeline.ExecuteGroup("PostProcess", context);

        // ======== 提取描边物体到独立列表 ========
        ExtractOutlineDrawCommands();

        // 清空 DrawCommands，生命周期在 EndScene() 结束
        m_OpaqueDrawCommands.clear();
        m_TransparentDrawCommands.clear();
        m_SpriteDrawCommands.clear();
    }

    void SceneRenderer::ExtractOutlineDrawCommands()
    {
        m_OutlineDrawCommands.clear();
        if (m_OutlineEntityIDs.empty())
        {
            return;
        }

        for (const DrawCommand& cmd : m_OpaqueDrawCommands)
        {
            if (m_OutlineEntityIDs.count(cmd.EntityID))
            {
                OutlineDrawCommand outlineCmd;
                outlineCmd.Transform = cmd.Transform;
                outlineCmd.MeshData = cmd.MeshData;
                outlineCmd.SubMeshPtr = cmd.SubMeshPtr;

                m_OutlineDrawCommands.push_back(outlineCmd);
            }
        }
        for (const DrawCommand& cmd : m_TransparentDrawCommands)
        {
            if (m_OutlineEntityIDs.count(cmd.EntityID))
            {
                OutlineDrawCommand outlineCmd;
                outlineCmd.Transform = cmd.Transform;
                outlineCmd.MeshData = cmd.MeshData;
                outlineCmd.SubMeshPtr = cmd.SubMeshPtr;

                m_OutlineDrawCommands.push_back(outlineCmd);
            }
        }
    }

    void SceneRenderer::RenderOutline()
    {
        if (!m_Spec.EnableOutline)
        {
            return;
        }

        RenderContext context;
        context.OutlineDrawCommands = &m_OutlineDrawCommands;
        context.OutlineEntityIDs = &m_OutlineEntityIDs;
        context.OutlineColor = m_OutlineColor;
        context.OutlineWidth = m_OutlineWidth;
        context.OutlineEnabled = m_OutlineEnabled;
        context.TargetFramebuffer = m_Framebuffer;
        context.Stats = &m_Stats;

        m_Pipeline.ExecuteGroup("Outline", context);

        m_OutlineDrawCommands.clear();
    }

    void SceneRenderer::SetPostProcessSettings(const PostProcessSettings& settings)
    {
        m_PostProcess = settings;

        // 同步效果参数到 PostProcessStack 中的各个 Effect
        auto postProcessPass = m_Pipeline.GetPass<PostProcessPass>();
        if (postProcessPass)
        {
            auto& stack = postProcessPass->GetPostProcessStack();

            auto bloom = stack.GetEffect<BloomEffect>();
            if (bloom)
            {
                bloom->Enabled = settings.BloomEnabled;
                bloom->Threshold = settings.BloomThreshold;
                bloom->Intensity = settings.BloomIntensity;
                bloom->Iterations = settings.BloomIterations;
            }

            auto fxaa = stack.GetEffect<FXAAEffect>();
            if (fxaa)
            {
                fxaa->Enabled = settings.FXAAEnabled;
            }

            auto vignette = stack.GetEffect<VignetteEffect>();
            if (vignette)
            {
                vignette->Enabled = settings.VignetteEnabled;
                vignette->VignetteIntensity = settings.VignetteIntensity;
                vignette->VignetteSmoothness = settings.VignetteSmoothness;
            }
        }
    }

    void SceneRenderer::SetEnvironmentSettings(const EnvironmentSettings& settings)
    {
        m_Environment = settings;
    }

    uint32_t SceneRenderer::GetFinalColorAttachmentID() const
    {
        return m_Framebuffer->GetColorAttachmentRendererID(0);
    }

    int SceneRenderer::ReadPixelEntityID(int x, int y) const
    {
        if (!m_Spec.EnablePicking)
        {
            return -1;
        }

        m_Framebuffer->Bind();
        int pixel = m_Framebuffer->GetPixel(1, x, y);
        m_Framebuffer->Unbind();
        return pixel;
    }
}