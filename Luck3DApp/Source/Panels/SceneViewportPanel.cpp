#include "SceneViewportPanel.h"

#include "Lucky/Renderer/RenderCommand.h"

#include "Lucky/Core/Input/Input.h"
#include "Lucky/Scene/SelectionManager.h"
#include "Lucky/Scene/Entity.h"
#include "Lucky/Scene/Components/Components.h"
#include "Lucky/Renderer/GizmoRenderer.h"
#include "Lucky/Editor/EditorPreferences.h"

#include "imgui/imgui.h"
#include "ImGuizmo.h"
#include "glm/gtc/type_ptr.hpp"

namespace Lucky
{
    SceneViewportPanel::SceneViewportPanel(const Ref<Scene>& scene)
        : m_Scene(scene), 
        m_EditorCamera(30.0f, 1280.0f / 720.0f, 0.01f, 1000.0f)
    {
        FramebufferSpecification fbSpec; // 帧缓冲区规范

        fbSpec.Attachments =
        {
            FramebufferTextureFormat::RGBA8,        // 颜色缓冲区 0
            FramebufferTextureFormat::RED_INTEGER,  // 颜色缓冲区 1：作为 id 实现鼠标点击拾取
            FramebufferTextureFormat::Depth         // 深度缓冲区
        };

        fbSpec.Width = 1280;
        fbSpec.Height = 720;

        m_Framebuffer = Framebuffer::Create(fbSpec);   // 创建帧缓冲区
    }

    void SceneViewportPanel::SetScene(const Ref<Scene>& scene)
    {
        m_Scene = scene;
    }

    void SceneViewportPanel::OnUpdate(DeltaTime dt)
    {
        if (FramebufferSpecification spec = m_Framebuffer->GetSpecification();
            m_ViewportSize.x > 0.0f && m_ViewportSize.y > 0.0f &&
            (spec.Width != m_ViewportSize.x || spec.Height != m_ViewportSize.y))
        {
            m_Framebuffer->Resize((uint32_t)m_ViewportSize.x, (uint32_t)m_ViewportSize.y);  // 重置帧缓冲区大小
            m_EditorCamera.SetViewportSize(m_ViewportSize.x, m_ViewportSize.y);             // 重置编辑器相机视口大小
        }

        m_EditorCamera.OnUpdate(dt);    // 更新编辑器相机
        
        m_Framebuffer->Bind();          // 绑定帧缓冲区

        const ColorSettings& colors = EditorPreferences::Get().GetColors();
        RenderCommand::SetClearColor(colors.ViewportClearColor);
        RenderCommand::Clear();

        m_Framebuffer->ClearAttachment(1, -1);  // 清除 Entity ID 缓冲区为 -1（无实体）

        m_Scene->OnUpdate(dt, m_EditorCamera);   // 更新场景
        
        // ---- Gizmo ----
        GizmoRenderer::BeginScene(m_EditorCamera);
        {
            // 坐标系无限网格
            GizmoRenderer::DrawInfiniteGrid(m_EditorCamera);
        
            // 灯光 Gizmo TODO 只绘制选中项
            auto dirLights = m_Scene->GetAllEntitiesWith<TransformComponent, DirectionalLightComponent>();
            for (auto entity : dirLights)
            {
                auto [transform, light] = dirLights.get<TransformComponent, DirectionalLightComponent>(entity);
                GizmoRenderer::DrawDirectionalLightGizmo(transform.Translation, transform.GetForward(), light.Color);
            }
            
            auto pointLights = m_Scene->GetAllEntitiesWith<TransformComponent, PointLightComponent>();
            for (auto entity : pointLights)
            {
                auto [transform, light] = pointLights.get<TransformComponent, PointLightComponent>(entity);
                GizmoRenderer::DrawPointLightGizmo(transform.Translation, light.Range, light.Color);
            }
            
            auto spotLights = m_Scene->GetAllEntitiesWith<TransformComponent, SpotLightComponent>();
            for (auto entity : spotLights)
            {
                auto [transform, light] = spotLights.get<TransformComponent, SpotLightComponent>(entity);
                GizmoRenderer::DrawSpotLightGizmo(transform.Translation, transform.GetForward(), light.Range, light.InnerCutoffAngle, light.OuterCutoffAngle, light.Color);
            }
        }
        GizmoRenderer::EndScene();
        
        m_Framebuffer->Unbind();    // 解除绑定帧缓冲区
    }

    void SceneViewportPanel::OnBegin(const char* name)
    {
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(1, 0)); // 设置 Gui 窗口样式：边界 = 0
        EditorPanel::OnBegin(name);
    }

    void SceneViewportPanel::OnEnd()
    {
        EditorPanel::OnEnd();
        ImGui::PopStyleVar();
    }

    void SceneViewportPanel::OnGUI()
    {
        auto viewportMinRegion = ImGui::GetWindowContentRegionMin();    // 视口可用区域最小值（视口左上角相对于视口左上角位置）
        auto viewportMaxRegion = ImGui::GetWindowContentRegionMax();    // 视口可用区域最大值（视口右下角相对于视口左上角位置）
        auto viewportOffset = ImGui::GetWindowPos();                    // 视口偏移量：视口面板左上角位置（相对于屏幕左上角）

        m_Bounds[0] = { viewportMinRegion.x + viewportOffset.x, viewportMinRegion.y + viewportOffset.y };
        m_Bounds[1] = { viewportMaxRegion.x + viewportOffset.x, viewportMaxRegion.y + viewportOffset.y };
        
        ImVec2 viewportPanelSize = ImGui::GetContentRegionAvail();  // 当前面板大小
        m_ViewportSize = { viewportPanelSize.x, viewportPanelSize.y };      // 视口大小
        
        uint32_t textureID = m_Framebuffer->GetColorAttachmentRendererID(); // 颜色缓冲区 0 ID

        ImGui::Image(reinterpret_cast<void*>(textureID), ImVec2{ m_ViewportSize.x, m_ViewportSize.y }, ImVec2(0, 1), ImVec2(1, 0)); // 场景视口图像
        
        UI_DrawGizmos();    // 绘制 Gizmo
    }

    void SceneViewportPanel::OnEvent(Event& event)
    {
        m_EditorCamera.OnEvent(event);
        
        EventDispatcher dispatcher(event);
        dispatcher.Dispatch<KeyPressedEvent>(LF_BIND_EVENT_FUNC(SceneViewportPanel::OnKeyPressed)); // 按键按下事件
        dispatcher.Dispatch<MouseButtonPressedEvent>(LF_BIND_EVENT_FUNC(SceneViewportPanel::OnMouseButtonPressed)); // 鼠标按键按下事件
    }

    void SceneViewportPanel::UI_DrawViewOrientationGizmo()
    {
        // 坐标轴指示器位置：视口右上角
        float viewManipulateSize = 200.0f;
        ImVec2 viewManipulatePos = ImVec2(m_Bounds[1].x - viewManipulateSize, m_Bounds[0].y);
    
        // 获取可修改的视图矩阵副本
        glm::mat4 viewMatrix = m_EditorCamera.GetViewMatrix();
    
        // 绘制坐标轴指示器（会修改 viewMatrix）
        ImGuizmo::ViewManipulate(
            glm::value_ptr(viewMatrix),
            m_EditorCamera.GetDistance(),
            viewManipulatePos,
            ImVec2(viewManipulateSize, viewManipulateSize),
            0x10101010  // 半透明背景
        );
    
        // 如果 ViewManipulate 修改了视图矩阵，同步回 EditorCamera
        if (ImGuizmo::IsUsingViewManipulate())
        {
            m_EditorCamera.SetViewMatrix(viewMatrix);
        }
    }

    void SceneViewportPanel::UI_DrawGizmos()
    {
        UI_DrawViewOrientationGizmo();   // 绘制编辑器相机视图坐标轴指示器
        
        UUID selectionID = SelectionManager::GetSelection();
        // 选中项存在 && Gizmo 类型存在
        if (selectionID != 0 && m_GizmoType != -1)
        {
            Entity entity = m_Scene->GetEntityWithUUID(selectionID);
            TransformComponent& transformComponent = entity.GetComponent<TransformComponent>();
            glm::mat4 transform = transformComponent.GetTransform();
            
            ImGuizmo::SetOrthographic(false);   // 透视投影
            ImGuizmo::AllowAxisFlip(false);     // 禁用坐标轴翻转
            ImGuizmo::SetDrawlist();            // 设置绘制列表

            // 设置绘制区域
            ImGuizmo::SetRect(m_Bounds[0].x, m_Bounds[0].y, m_Bounds[1].x - m_Bounds[0].x, m_Bounds[1].y - m_Bounds[0].y);

            // 编辑器相机
            glm::mat4 viewMatrix = m_EditorCamera.GetViewMatrix();              // 视图矩阵
            glm::mat4 projectionMatrix = m_EditorCamera.GetProjectionMatrix();  // 投影矩阵

            bool span = Input::IsKeyPressed(Key::LeftControl);  // Ctrl 刻度捕捉：操作时固定 delta 刻度
            float spanValue = 0.5f;     // 平移缩放间隔：0.5m

            // 旋转间隔值：5 度
            if (m_GizmoType == ImGuizmo::OPERATION::ROTATE)
            {
                spanValue = 5.0f;
            }

            float spanValues[3] = { spanValue, spanValue, spanValue };  // xyz 轴刻度捕捉值

            // 绘制操作
            ImGuizmo::Manipulate(
                glm::value_ptr(viewMatrix),         // 视图矩阵
                glm::value_ptr(projectionMatrix),   // 投影矩阵
                (ImGuizmo::OPERATION)m_GizmoType,       // 操作类型
                (ImGuizmo::MODE)m_GizmoMode,            // 坐标系：本地|世界
                glm::value_ptr(transform),          // transform 增量矩阵
                nullptr,
                span ? spanValues : nullptr             // 刻度捕捉值
            );

            // Gizmo 正在使用
            if (ImGuizmo::IsUsing())
            {
                glm::vec3 translation;
                glm::quat rotation;
                glm::vec3 scale;
                Math::DecomposeTransform(transform, translation, rotation, scale); // 分解 transform 矩阵

                switch (m_GizmoType)
                {
                    case ImGuizmo::OPERATION::TRANSLATE:
                        transformComponent.Translation = translation;   // 更新位置
                        break;
                    case ImGuizmo::OPERATION::ROTATE:
                        glm::vec3 originalRotationEuler = transformComponent.GetRotationEuler();
                    
                        glm::vec3 deltaRotationEuler = glm::eulerAngles(rotation) - originalRotationEuler;  // 计算旋转增量（弧度）

                        // 避免数值精度导致的漂移
                        if (fabs(deltaRotationEuler.x) < 0.001) deltaRotationEuler.x = 0.0f;
                        if (fabs(deltaRotationEuler.y) < 0.001) deltaRotationEuler.y = 0.0f;
                        if (fabs(deltaRotationEuler.z) < 0.001) deltaRotationEuler.z = 0.0f;
                    
                        // 累积旋转
                        glm::vec3 newRotationEuler = transformComponent.GetRotationEuler();
                        newRotationEuler += deltaRotationEuler;
                    
                        transformComponent.SetRotationEuler(newRotationEuler);    // 更新旋转
                        break;
                    case ImGuizmo::OPERATION::SCALE:
                        transformComponent.Scale = scale;   // 更新缩放
                        break;
                }
            }
        }
    }

    bool SceneViewportPanel::OnKeyPressed(KeyPressedEvent& e)
    {
        // 按键重复
        if (e.IsRepeat())
        {
            return false;
        }

        // Gizmo 不是正在使用
        if (!ImGuizmo::IsUsing())
        {
            // Gizmo 快捷键
            switch (e.GetKeyCode())
            {
            case Key::G:
                m_GizmoType = ImGuizmo::OPERATION::TRANSLATE;   // 平移
                break;
            case Key::R:
                m_GizmoType = ImGuizmo::OPERATION::ROTATE;      // 旋转
                break;
            case Key::S:
                m_GizmoType = ImGuizmo::OPERATION::SCALE;       // 缩放
                break;
            }
        }

        return false;
    }

    bool SceneViewportPanel::OnMouseButtonPressed(MouseButtonPressedEvent& e)
    {
        if (e.GetMouseButton() != Mouse::ButtonLeft)
        {
            return false;
        }

        // 跳过 Alt 键（相机操作）和 ImGuizmo 操作
        if (Input::IsKeyPressed(Key::LeftAlt) || ImGuizmo::IsOver())
        {
            return false;
        }

        // 获取鼠标在视口中的坐标
        auto [mx, my] = ImGui::GetMousePos();
        mx -= m_Bounds[0].x;
        my -= m_Bounds[0].y;

        auto viewportWidth = m_Bounds[1].x - m_Bounds[0].x;
        auto viewportHeight = m_Bounds[1].y - m_Bounds[0].y;

        // 翻转 Y 轴（ImGui Y 向下，OpenGL Y 向上）
        my = viewportHeight - my;

        int mouseX = (int)mx;
        int mouseY = (int)my;

        // 检查是否在视口范围内
        if (mouseX >= 0 && mouseY >= 0 && mouseX < (int)viewportWidth && mouseY < (int)viewportHeight)
        {
            m_Framebuffer->Bind();
            int pixelData = m_Framebuffer->GetPixel(1, mouseX, mouseY);
            m_Framebuffer->Unbind();

            if (pixelData == -1)
            {
                // 点击空白区域 → 取消选中
                SelectionManager::Deselect();
            }
            else
            {
                // 通过 entt::entity 获取 Entity → 选中
                entt::entity enttID = (entt::entity)(uint32_t)pixelData;

                if (m_Scene->IsEntityValid(enttID))
                {
                    Entity entity = { enttID, m_Scene.get() };
                    SelectionManager::Select(entity.GetUUID());
                }
            }
        }

        return false;
    }
}
