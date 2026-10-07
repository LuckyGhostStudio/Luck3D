#include "lcpch.h"
#include "AssetPreviewRenderer.h"

#include "Lucky/Renderer/SceneRenderer.h"
#include "Lucky/Renderer/Renderer3D.h"
#include "Lucky/Renderer/Mesh.h"
#include "Lucky/Renderer/MeshFactory.h"
#include "Lucky/Renderer/Material.h"
#include "Lucky/Renderer/CameraRenderData.h"
#include "Lucky/Renderer/LightRenderData.h"

#include <glm/gtc/matrix_transform.hpp>

namespace Lucky
{
    namespace
    {
        constexpr uint32_t s_PreviewSize = 128;
        constexpr float    s_PreviewFovDeg = 30.0f;     // 预览相机 FOV
        constexpr float    s_MaterialFillRatio = 1.1f;  // 材质球填充系数（相机距离 = 半径/sin(FOV/2) × 系数，越大球越小）
        constexpr float    s_MeshFillRatio = 1.28f;     // 网格预览填充系数（同上，越大模型占框越小）

        struct PreviewData
        {
            Ref<SceneRenderer> Renderer;        // 专用离屏渲染器
            Ref<Mesh> SphereMesh;               // 内置球体，供材质预览使用

            CameraRenderData FixedCamera{};     // 固定相机（每次按目标重算）
            LightRenderData FixedLight{};       // 固定布光
        };
        static Scope<PreviewData> s_Data;

        /// <summary>
        /// 按目标中心点和相机距离构造 FixedCamera：相机位于目标左上前方，朝目标看
        /// 预览全部用透视投影，FOV 固定 30°
        /// </summary>
        void BuildFixedCamera(const glm::vec3& target, float distance)
        {
            constexpr float fovDeg = s_PreviewFovDeg;
            // 方位：左上前方，俯角约 31°（对齐 Unity 资产预览的斜俯视角度）
            const glm::vec3 viewDir = glm::normalize(glm::vec3(-1.0f, 0.85f, 1.0f));
            glm::vec3 camPos = target + viewDir * distance;
            glm::mat4 view = glm::lookAt(camPos, target, glm::vec3(0.0f, 1.0f, 0.0f));
            glm::mat4 proj = glm::perspective(glm::radians(fovDeg), 1.0f, 0.01f, distance * 10.0f);

            s_Data->FixedCamera.ViewMatrix = view;
            s_Data->FixedCamera.ProjectionMatrix = proj;
            s_Data->FixedCamera.Position = camPos;
            s_Data->FixedCamera.Projection = ProjectionType::Perspective;
            s_Data->FixedCamera.FOV = fovDeg;
            s_Data->FixedCamera.AspectRatio = 1.0f;
            s_Data->FixedCamera.NearClip = 0.01f;
        }
    }

    void AssetPreviewRenderer::Init()
    {
        s_Data = CreateScope<PreviewData>();

        // 专用 SceneRenderer：128x128，关掉所有可选 Pass
        SceneRendererSpec spec;
        spec.Width = s_PreviewSize;
        spec.Height = s_PreviewSize;
        spec.EnableShadow = false;
        spec.EnablePicking = false;
        spec.EnableOutline = false;
        spec.EnableDebugVisualize = false;
        spec.EnablePostProcess = false;

        s_Data->Renderer = CreateRef<SceneRenderer>();
        s_Data->Renderer->Init(spec);
        s_Data->Renderer->SetClearColor(glm::vec4(0.322f, 0.322f, 0.322f, 1.0f));   // 预览背景 #525252

        // 球体网格（Material 预览用）
        s_Data->SphereMesh = MeshFactory::CreateSphere();

        // 固定布光：一盏方向光（左上方来、俯角偏平，球的右下部留出明显背光阴影，对齐 Unity 预览观感）
        s_Data->FixedLight.DirectionalLightCount = 1;
        DirectionalLightData& mainLight = s_Data->FixedLight.DirectionalLights[0];
        mainLight.Direction = glm::normalize(glm::vec3(0.75f, -0.9f, -0.35f));
        mainLight.Color = glm::vec3(1.0f);
        mainLight.Intensity = 1.2f;

        LF_CORE_INFO("AssetPreviewRenderer::Init - Preview renderer ready (size={0})", s_PreviewSize);
    }

    void AssetPreviewRenderer::Shutdown()
    {
        if (s_Data && s_Data->Renderer)
        {
            s_Data->Renderer->Shutdown();
        }
        s_Data.reset();
    }

    const Ref<Framebuffer>& AssetPreviewRenderer::RenderMaterial(const Ref<Material>& material)
    {
        static Ref<Framebuffer> s_Null;
        if (!material || !s_Data || !s_Data->SphereMesh)
        {
            return s_Null;
        }

        // 球体半径 0.5，按填充系数换算相机距离（半径 / sin(FOV/2) × 系数）
        float distance = 0.5f / std::sin(glm::radians(s_PreviewFovDeg * 0.5f)) * s_MaterialFillRatio;
        BuildFixedCamera(glm::vec3(0.0f), distance);

        s_Data->Renderer->BeginScene(s_Data->FixedCamera, s_Data->FixedLight);
        std::vector<Ref<Material>> materials = { material };
        Ref<Mesh> sphere = s_Data->SphereMesh;      // SubmitMesh 第二参是非 const 引用，必须本地变量承接
        s_Data->Renderer->SubmitMesh(glm::mat4(1.0f), sphere, materials);
        s_Data->Renderer->EndScene();

        return s_Data->Renderer->GetFramebuffer();
    }

    const Ref<Framebuffer>& AssetPreviewRenderer::RenderMesh(const Ref<Mesh>& mesh)
    {
        static Ref<Framebuffer> s_Null;
        if (!mesh || !s_Data)
        {
            return s_Null;
        }

        const AABB& bounds = mesh->GetBoundingBox();
        float radius = glm::length(bounds.Max - bounds.Min) * 0.5f;
        if (radius < 0.001f)
        {
            radius = 0.5f;     // 退化 Mesh 兜底，防止除零
        }
        float distance = radius / std::sin(glm::radians(s_Data->FixedCamera.FOV) * 0.5f) * s_MeshFillRatio;
        BuildFixedCamera(bounds.GetCenter(), distance);

        s_Data->Renderer->BeginScene(s_Data->FixedCamera, s_Data->FixedLight);
        std::vector<Ref<Material>> materials = { Renderer3D::GetDefaultMaterial() };
        Ref<Mesh> target = mesh;
        s_Data->Renderer->SubmitMesh(glm::mat4(1.0f), target, materials);
        s_Data->Renderer->EndScene();

        return s_Data->Renderer->GetFramebuffer();
    }

    uint32_t AssetPreviewRenderer::GetPreviewSize()
    {
        return s_PreviewSize;
    }
}
