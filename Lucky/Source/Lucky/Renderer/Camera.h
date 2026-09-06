#pragma once

#include <glm/glm.hpp>

namespace Lucky
{
    /// <summary>
    /// 投影类型
    /// </summary>
    enum class ProjectionType : uint8_t
    {
        Perspective = 0,        // 透视投影
        Orthographic            // 正交投影
    };

    /// <summary>
    /// 相机基类：管理投影矩阵与投影参数（Perspective / Orthographic）
    /// 不感知视图矩阵、位置、朝向 —— 那些由派生类或外部 Transform 承担
    /// </summary>
    class Camera
    {
    public:
        Camera();
        virtual ~Camera() = default;

        // ---- 一次性设置投影 ----

        /// <summary>
        /// 设置为透视投影
        /// </summary>
        /// <param name="verticalFOV">垂直张角（度）</param>
        /// <param name="nearClip">近裁剪面</param>
        /// <param name="farClip">远裁剪面</param>
        void SetPerspective(float verticalFOV, float nearClip, float farClip);

        /// <summary>
        /// 设置为正交投影
        /// </summary>
        /// <param name="size">正交视口垂直大小（世界空间单位）</param>
        /// <param name="nearClip">近裁剪面</param>
        /// <param name="farClip">远裁剪面</param>
        void SetOrthographic(float size, float nearClip, float farClip);

        /// <summary>
        /// 更新视口宽高，触发投影矩阵重算
        /// </summary>
        void SetViewportSize(uint32_t width, uint32_t height);

        // ---- Projection Type ----
        ProjectionType GetProjectionType() const { return m_ProjectionType; }
        void SetProjectionType(ProjectionType type);

        // ---- Perspective ----
        float GetPerspectiveVerticalFOV() const { return m_PerspectiveFOV; }
        void SetPerspectiveVerticalFOV(float fov);
        float GetPerspectiveNearClip() const { return m_PerspectiveNear; }
        void SetPerspectiveNearClip(float nearClip);
        float GetPerspectiveFarClip() const { return m_PerspectiveFar; }
        void SetPerspectiveFarClip(float farClip);

        // ---- Orthographic ----
        float GetOrthographicSize() const { return m_OrthographicSize; }
        void SetOrthographicSize(float size);
        float GetOrthographicNearClip() const { return m_OrthographicNear; }
        void SetOrthographicNearClip(float nearClip);
        float GetOrthographicFarClip() const { return m_OrthographicFar; }
        void SetOrthographicFarClip(float farClip);

        // ---- Aspect / Projection Matrix ----
        float GetAspectRatio() const { return m_AspectRatio; }
        const glm::mat4& GetProjectionMatrix() const { return m_ProjectionMatrix; }
    protected:
        /// <summary>
        /// 根据当前 ProjectionType 与参数重算投影矩阵
        /// </summary>
        void RecalculateProjection();
    protected:
        ProjectionType m_ProjectionType = ProjectionType::Perspective;

        float m_PerspectiveFOV = 45.0f;             // 垂直张角（度）
        float m_PerspectiveNear = 0.01f;            // 透视近裁剪面
        float m_PerspectiveFar = 1000.0f;           // 透视远裁剪面

        float m_OrthographicSize = 10.0f;           // 正交视口垂直大小
        float m_OrthographicNear = -1.0f;           // 正交近裁剪面
        float m_OrthographicFar = 1000.0f;          // 正交远裁剪面

        float m_AspectRatio = 1.0f;                 // 宽高比

        glm::mat4 m_ProjectionMatrix = glm::mat4(1.0f);
    };
}