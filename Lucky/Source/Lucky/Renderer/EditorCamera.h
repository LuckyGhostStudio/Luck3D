#pragma once

#include "Lucky/Core/DeltaTime.h"
#include "Lucky/Core/Events/Event.h"
#include "Lucky/Core/Events/MouseEvent.h"
#include "Camera.h"

#include <glm/glm.hpp>

namespace Lucky
{
    /// <summary>
    /// 编辑器相机
    /// </summary>
    class EditorCamera : public Camera
    {
    private:
        /// <summary>
        /// 更新视图矩阵
        /// </summary>
        void UpdateView();

        /// <summary>
        /// 鼠标滚轮滚动时调用
        /// </summary>
        /// <param name="e">鼠标滚动事件</param>
        /// <returns>处理结果</returns>
        bool OnMouseScroll(MouseScrolledEvent& e);

        void ViewPan(const glm::vec2& delta);
        void ViewRotate(const glm::vec2& delta);
        void ViewZoom(float delta);

        /// <summary>
        /// 计算相机位置
        /// </summary>
        /// <returns></returns>
        glm::vec3 CalculatePosition() const;

        std::pair<float, float> PanSpeed() const;
        float RotationSpeed() const;
        float ZoomSpeed() const;
    public:
        EditorCamera() = default;

        /// <summary>
        /// 编辑器相机
        /// </summary>
        /// <param name="fov">垂直张角</param>
        /// <param name="aspectRatio">屏幕宽高比</param>
        /// <param name="nearClip">近裁剪平面</param>
        /// <param name="farClip">远裁剪平面</param>
        EditorCamera(float fov, float aspectRatio, float nearClip, float farClip);

        /// <summary>
        /// 更新相机：每帧
        /// </summary>
        /// <param name="dt">帧间隔</param>
        void OnUpdate(DeltaTime dt);

        /// <summary>
        /// 事件回调函数
        /// </summary>
        /// <param name="e">事件</param>
        void OnEvent(Event& e);

        void SetViewportSize(float width, float height);

        /// <summary>
        /// 由视口面板每帧告知：鼠标当前是否悬在 Scene 视口里。
        /// 用于"按下瞬间"的拦截：只有悬在视口内按下 Alt+左键 / 中键，才会进入拖动态；
        /// 一旦进入拖动态，鼠标可以移出视口继续拖动直到松开（Unity 一致行为）。
        /// 必要性：OnUpdate 用 Input 轮询鼠标键，不走事件分发，面板的 Hovered 守卫拦不住，
        /// 否则鼠标在 Hierarchy / Inspector 按 Alt+左键 / 中键也会让场景相机旋转或平移。
        /// </summary>
        void SetViewportHovered(bool hovered) { m_ViewportHovered = hovered; }

        float GetDistance() const { return m_Distance; }
        void SetDistance(float distance) { m_Distance = distance; }

        const glm::mat4& GetViewMatrix() const { return m_ViewMatrix; }
        
        /// <summary>
        /// 从外部修改后的视图矩阵反推内部参数（用于 ViewManipulate 集成）
        /// </summary>
        /// <param name="viewMatrix">被 ViewManipulate 修改后的视图矩阵</param>
        void SetViewMatrix(const glm::mat4& viewMatrix);
        
        glm::mat4 GetViewProjectionMatrix() const { return m_ProjectionMatrix * m_ViewMatrix; }

        glm::vec3 GetUpDirection() const;
        glm::vec3 GetRightDirection() const;
        glm::vec3 GetForwardDirection() const;
        const glm::vec3& GetPosition() const { return m_Position; }
        glm::quat GetOrientation() const;

        float GetPitch() const { return m_Pitch; }
        float GetYaw() const { return m_Yaw; }
        float GetViewportHeight() const { return m_ViewportHeight; }
    private:
        glm::mat4 m_ViewMatrix;                 // 视图矩阵

        glm::vec3 m_Position = { 0.0f, 0.0f, 0.0f };    // 相机位置
        glm::vec3 m_FocalPoint = { 0.0f, 0.0f, 0.0f };  // 焦点位置

        glm::vec2 m_InitialMousePosition = { 0.0f, 0.0f };  // 鼠标初始位置

        float m_Distance = 5.0f;            // 相机与焦点距离
        float m_Pitch = 0.44f;
        float m_Yaw = -0.62f;

        float m_ViewportWidth = 1280.0f;    // 视口宽
        float m_ViewportHeight = 720;       // 视口高

        // ---- 操作状态（Unity 风格：中键 Pan / Alt+左键 Rotate）----
        // 由面板每帧 SetViewportHovered 注入；仅在按下瞬间作为进入拖动态的门控
        bool m_ViewportHovered = false;
        bool m_IsPanning = false;       // 中键拖动态
        bool m_IsRotating = false;      // Alt+左键拖动态
    };
}