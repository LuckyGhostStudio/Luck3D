#pragma once

#include "Lucky/Core/Base.h"
#include "Lucky/Renderer/EditorCamera.h"
#include <glm/glm.hpp>

namespace Lucky
{
    /// <summary>
    /// Gizmo 渲染器：独立的渲染路径，不影响主渲染流程
    /// 使用线段批处理，一次 DrawCall 绘制所有线段
    /// </summary>
    class GizmoRenderer
    {
    public:
        static void Init();
        static void Shutdown();
        
        /// <summary>
        /// 开始 Gizmo 渲染（设置相机矩阵）
        /// </summary>
        static void BeginScene(const EditorCamera& camera);
        
        /// <summary>
        /// 结束 Gizmo 渲染（提交所有线段到 GPU）
        /// </summary>
        static void EndScene();
        
        // ---- 基础图元 ----
        
        static void DrawLine(const glm::vec3& start, const glm::vec3& end, const glm::vec4& color);
        static void DrawWireBox(const glm::vec3& position, const glm::vec3& size, const glm::vec4& color);
        static void DrawWireCircle(const glm::vec3& position, const glm::vec3& axis, float radius, const glm::vec4& color, int segments = 32);
        static void DrawWireSphere(const glm::vec3& position, float radius, const glm::vec4& color, int segments = 32);
       
        // ---- 场景 Gizmo ----
        
        static void DrawGrid(float size = 10.0f, int divisions = 10, const glm::vec4& color = glm::vec4(0.329f, 0.329f, 0.329f, 0.502f));
        static void DrawDirectionalLightGizmo(const glm::vec3& position, const glm::vec3& direction, const glm::vec3& color);
        static void DrawPointLightGizmo(const glm::vec3& position, float range, const glm::vec3& color);
        static void DrawSpotLightGizmo(const glm::vec3& position, const glm::vec3& direction, float range, float innerAngle, float outerAngle, const glm::vec3& color);
    };
}