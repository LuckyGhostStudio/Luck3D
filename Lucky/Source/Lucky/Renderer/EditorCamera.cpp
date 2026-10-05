#include "lcpch.h"
#include "EditorCamera.h"

#include "Lucky/Core/Input/Input.h"
#include "Lucky/Core/Input/KeyCodes.h"
#include "Lucky/Core/Input/MouseButtonCodes.h"

#define GLM_ENABLE_EXPERIMENTAL
#include <glm/gtx/quaternion.hpp>

namespace Lucky
{
    EditorCamera::EditorCamera(float fov, float aspectRatio, float nearClip, float farClip)
    {
        // 由 aspectRatio 反推初始视口尺寸（后续 SetViewportSize 会覆盖）
        m_ViewportHeight = 720.0f;
        m_ViewportWidth = 720.0f * aspectRatio;

        SetPerspective(fov, nearClip, farClip);
        Camera::SetViewportSize(static_cast<uint32_t>(m_ViewportWidth), static_cast<uint32_t>(m_ViewportHeight));

        UpdateView();   // 更新视图矩阵
    }

    void EditorCamera::SetViewportSize(float width, float height)
    {
        m_ViewportWidth = width;
        m_ViewportHeight = height;
        Camera::SetViewportSize(static_cast<uint32_t>(width), static_cast<uint32_t>(height));
    }

    void EditorCamera::UpdateView()
    {
        m_Position = CalculatePosition();           // 计算相机位置

        glm::quat orientation = GetOrientation();   // 计算相机方向
        // 计算视图矩阵
        m_ViewMatrix = glm::translate(glm::mat4(1.0f), m_Position) * glm::toMat4(orientation);  // 相机 Transform 矩阵
        m_ViewMatrix = glm::inverse(m_ViewMatrix);  // Transform 的逆矩阵
    }

    std::pair<float, float> EditorCamera::PanSpeed() const
    {
        float x = std::min(m_ViewportWidth / 1000.0f, 2.4f);        // x 最大移动速度 2.4f
        float xFactor = 0.0366f * (x * x) - 0.1778f * x + 0.3021f;

        float y = std::min(m_ViewportHeight / 1000.0f, 2.4f);       // y 最大移动速度 2.4f
        float yFactor = 0.0366f * (y * y) - 0.1778f * y + 0.3021f;

        return { xFactor, yFactor };
    }

    float EditorCamera::RotationSpeed() const
    {
        return 0.8f;
    }

    float EditorCamera::ZoomSpeed() const
    {
        float distance = m_Distance * 0.2f;
        distance = std::max(distance, 0.0f);    // 最小缩放速度 = 0

        float speed = distance * distance;
        speed = std::min(speed, 100.0f);        // 最大缩放速度 = 100

        return speed;
    }

    void EditorCamera::OnUpdate(DeltaTime dt)
    {
        const glm::vec2& mouse{ Input::GetMouseX(), Input::GetMouseY() };   // 当前鼠标位置
        glm::vec2 delta = (mouse - m_InitialMousePosition) * 0.003f;        // 鼠标移动增量 = 当前位置 - 初始位置
        m_InitialMousePosition = mouse;                                     // 初始鼠标位置

        // ---- Unity 风格操作：中键拖动 Pan / Alt+左键拖动 Rotate ----
        // 进入拖动态的前提：鼠标必须按下瞬间悬在 Scene 视口内（m_ViewportHovered）；
        // 进入态后忽略 Hovered，一直响应到松开按键（允许拖出视口外继续操作，Unity 一致）。
        bool middleDown = Input::IsMouseButtonPressed(Mouse::ButtonMiddle);
        bool leftDown   = Input::IsMouseButtonPressed(Mouse::ButtonLeft);
        bool altDown    = Input::IsKeyPressed(Key::LeftAlt) || Input::IsKeyPressed(Key::RightAlt);

        // Pan：中键
        if (!m_IsPanning)
        {
            // 进入态：中键按下 且 当前悬在视口里（避免在 Hierarchy / Inspector 按中键误触）
            if (middleDown && m_ViewportHovered)
            {
                m_IsPanning = true;
            }
        }
        else if (!middleDown)
        {
            // 退出态：松开中键
            m_IsPanning = false;
        }

        // Rotate：Alt + 左键
        if (!m_IsRotating)
        {
            // 进入态：Alt 和 左键 同时按下 且 悬在视口里
            if (altDown && leftDown && m_ViewportHovered)
            {
                m_IsRotating = true;
            }
        }
        else if (!leftDown || !altDown)
        {
            // 退出态：松开左键 或 松开 Alt 任一
            m_IsRotating = false;
        }

        if (m_IsPanning)
        {
            ViewPan(delta);     // 平移
        }
        else if (m_IsRotating)
        {
            ViewRotate(delta);  // 旋转
        }

        UpdateView();   // 更新视图矩阵
    }

    void EditorCamera::OnEvent(Event& e)
    {
        EventDispatcher dispatcher(e);
        dispatcher.Dispatch<MouseScrolledEvent>(LF_BIND_EVENT_FUNC(EditorCamera::OnMouseScroll));   // 鼠标滚轮事件
    }

    bool EditorCamera::OnMouseScroll(MouseScrolledEvent& e)
    {
        // 灵敏度：0.25 对应 Unity 默认手感（2026-10-06 调高，原值 0.1 偏迟钝）
        float delta = e.GetYOffset() * 0.25f;   // 滚轮Y偏移量
        ViewZoom(delta);                        // 视图缩放
        UpdateView();                           // 更新视图

        return true;    // 消费事件：避免其他面板（如 ProjectAssets 的缩略图缩放）同帧重复响应
    }

    void EditorCamera::ViewPan(const glm::vec2& delta)
    {
        auto [xSpeed, ySpeed] = PanSpeed();

        m_FocalPoint += -GetRightDirection() * delta.x * xSpeed * m_Distance;
        m_FocalPoint += GetUpDirection() * delta.y * ySpeed * m_Distance;
    }

    void EditorCamera::ViewRotate(const glm::vec2& delta)
    {
        float yawSign = GetUpDirection().y < 0 ? -1.0f : 1.0f;

        m_Yaw += yawSign * delta.x * RotationSpeed();
        m_Pitch += delta.y * RotationSpeed();
    }

    void EditorCamera::ViewZoom(float delta)
    {
        m_Distance -= delta * ZoomSpeed();

        if (m_Distance < 1.0f)
        {
            m_FocalPoint += GetForwardDirection();
            m_Distance = 1.0f;
        }
    }

    void EditorCamera::SetViewMatrix(const glm::mat4& viewMatrix)
    {
        m_ViewMatrix = viewMatrix;
    
        // 从 view 矩阵反推相机参数
        glm::mat4 invView = glm::inverse(viewMatrix);
    
        // 相机位置 = 逆视图矩阵的平移列
        m_Position = glm::vec3(invView[3]);
    
        // 从逆视图矩阵提取旋转，反推 pitch 和 yaw
        glm::vec3 forward = -glm::vec3(invView[2]); // 相机前方向（-Z）
        m_Pitch = asin(-forward.y);                 // 俯仰角
        m_Yaw = atan2(forward.x, forward.z);        // 偏航角
    
        // 焦点 = 相机位置 + 前方向 × 距离
        m_FocalPoint = m_Position + forward * m_Distance;
    }

    glm::vec3 EditorCamera::GetUpDirection() const
    {
        return glm::rotate(GetOrientation(), glm::vec3(0.0f, 1.0f, 0.0f));  // 相机 up y+
    }

    glm::vec3 EditorCamera::GetRightDirection() const
    {
        return glm::rotate(GetOrientation(), glm::vec3(1.0f, 0.0f, 0.0f));  // 相机 rigth x+
    }

    glm::vec3 EditorCamera::GetForwardDirection() const
    {
        return glm::rotate(GetOrientation(), glm::vec3(0.0f, 0.0f, -1.0f)); // 相机 forward z-
    }

    glm::vec3 EditorCamera::CalculatePosition() const
    {
        return m_FocalPoint - GetForwardDirection() * m_Distance;   // 相机位置 = 焦点 - 相机到焦点到距离
    }

    glm::quat EditorCamera::GetOrientation() const
    {
        return glm::quat(glm::vec3(-m_Pitch, -m_Yaw, 0.0f));
    }
}