#include "lcpch.h"
#include "GizmoRenderer.h"

#include "VertexArray.h"
#include "Shader.h"
#include "RenderCommand.h"

#include "glm/ext/scalar_constants.hpp"

namespace Lucky
{
    /// <summary>
    /// Gizmo 顶点
    /// </summary>
    struct GizmoVertex
    {
        glm::vec3 Position;
        glm::vec4 Color;
    };

    struct GizmoRendererData
    {
        static constexpr uint32_t MaxLines = 10000;
        static constexpr uint32_t MaxVertices = MaxLines * 2;
        
        Ref<VertexArray> LineVertexArray;
        Ref<VertexBuffer> LineVertexBuffer;
        Ref<Shader> LineShader;
        
        GizmoVertex* LineVertexBufferBase = nullptr;     // 顶点缓冲区基地址
        GizmoVertex* LineVertexBufferPtr = nullptr;      // 当前写入位置
        uint32_t LineVertexCount = 0;                    // 当前线段顶点数
        
        glm::vec3 CameraPosition;   // 相机位置
    };

    static GizmoRendererData s_GizmoData;

    void GizmoRenderer::Init()
    {
        // 创建线段 VAO/VBO
        s_GizmoData.LineVertexArray = VertexArray::Create();
        s_GizmoData.LineVertexBuffer = VertexBuffer::Create(sizeof(GizmoVertex) * GizmoRendererData::MaxVertices);
        s_GizmoData.LineVertexBuffer->SetLayout({
            { ShaderDataType::Float3, "a_Position" },
            { ShaderDataType::Float4, "a_Color" }
        });
        s_GizmoData.LineVertexArray->AddVertexBuffer(s_GizmoData.LineVertexBuffer);
        
        // 分配 CPU 端顶点缓冲区
        s_GizmoData.LineVertexBufferBase = new GizmoVertex[GizmoRendererData::MaxVertices];
        
        // 加载 Gizmo Shader
        s_GizmoData.LineShader = Shader::Create("Assets/Shaders/GizmoLine");
    }

    void GizmoRenderer::Shutdown()
    {
        
    }

    void GizmoRenderer::BeginScene(const EditorCamera& camera)
    {
        // 重置顶点缓冲区
        s_GizmoData.LineVertexBufferPtr = s_GizmoData.LineVertexBufferBase;
        s_GizmoData.LineVertexCount = 0;
        
        s_GizmoData.CameraPosition = camera.GetPosition();  // 缓存相机位置 用于朝向相机的 Gizmo
        
        // 相机矩阵通过 UBO (binding=0) 已经设置 TODO 通用渲染操作移动到 Renderer.h
    }

    void GizmoRenderer::EndScene()
    {
        if (s_GizmoData.LineVertexCount == 0)
        {
            return;
        }
        
        // 上传顶点数据到 GPU
        uint32_t dataSize = (uint32_t)((uint8_t*)s_GizmoData.LineVertexBufferPtr - (uint8_t*)s_GizmoData.LineVertexBufferBase);
        s_GizmoData.LineVertexBuffer->SetData(s_GizmoData.LineVertexBufferBase, dataSize);
        
        // 绑定 Shader
        s_GizmoData.LineShader->Bind();
        
        // 设置线宽
        RenderCommand::SetLineWidth(1.5f);
        
        // 绘制 Line 顶点数组
        RenderCommand::DrawLines(s_GizmoData.LineVertexArray, s_GizmoData.LineVertexCount);
    }
    
    // ---- 基础图元 ----
    
    void GizmoRenderer::DrawLine(const glm::vec3& start, const glm::vec3& end, const glm::vec4& color)
    {
        if (s_GizmoData.LineVertexCount >= GizmoRendererData::MaxVertices)
        {
            return;
        }
        
        s_GizmoData.LineVertexBufferPtr->Position = start;
        s_GizmoData.LineVertexBufferPtr->Color = color;
        s_GizmoData.LineVertexBufferPtr++;
        
        s_GizmoData.LineVertexBufferPtr->Position = end;
        s_GizmoData.LineVertexBufferPtr->Color = color;
        s_GizmoData.LineVertexBufferPtr++;
        
        s_GizmoData.LineVertexCount += 2;
    }
    
    void GizmoRenderer::DrawWireBox(const glm::vec3& position, const glm::vec3& size, const glm::vec4& color)
    {
        glm::vec3 half = size * 0.5f;
    
        // 8 个顶点
        glm::vec3 v[8] = {
            position + glm::vec3(-half.x, -half.y, -half.z),
            position + glm::vec3( half.x, -half.y, -half.z),
            position + glm::vec3( half.x,  half.y, -half.z),
            position + glm::vec3(-half.x,  half.y, -half.z),
            position + glm::vec3(-half.x, -half.y,  half.z),
            position + glm::vec3( half.x, -half.y,  half.z),
            position + glm::vec3( half.x,  half.y,  half.z),
            position + glm::vec3(-half.x,  half.y,  half.z),
        };
    
        // 12 条边
        // 底面
        DrawLine(v[0], v[1], color); DrawLine(v[1], v[2], color);
        DrawLine(v[2], v[3], color); DrawLine(v[3], v[0], color);
        // 顶面
        DrawLine(v[4], v[5], color); DrawLine(v[5], v[6], color);
        DrawLine(v[6], v[7], color); DrawLine(v[7], v[4], color);
        // 竖边
        DrawLine(v[0], v[4], color); DrawLine(v[1], v[5], color);
        DrawLine(v[2], v[6], color); DrawLine(v[3], v[7], color);
    }

    void GizmoRenderer::DrawWireCircle(const glm::vec3& position, const glm::vec3& axis, float radius, const glm::vec4& color, int segments)
    {
        // 参数校验
        segments = std::max(segments, 3);
        radius = std::max(radius, 0.0f);
    
        constexpr float PI = glm::pi<float>();
    
        // 计算圆平面的正交基向量
        glm::vec3 normalizedAxis = glm::normalize(axis);
        glm::vec3 up = glm::abs(normalizedAxis.y) < 0.99f ? glm::vec3(0, 1, 0) : glm::vec3(1, 0, 0);
        glm::vec3 right = glm::normalize(glm::cross(normalizedAxis, up));
        up = glm::normalize(glm::cross(right, normalizedAxis));
    
        // 生成圆周点并绘制线段
        glm::vec3 firstPoint;
        glm::vec3 prevPoint;
    
        for (int seg = 0; seg < segments; ++seg)
        {
            float theta = 2.0f * PI * static_cast<float>(seg) / static_cast<float>(segments);
            float cosTheta = cos(theta);
            float sinTheta = sin(theta);
        
            // 计算圆周上的点
            glm::vec3 point = position + radius * (cosTheta * right + sinTheta * up);
        
            if (seg == 0)
            {
                firstPoint = point;
            }
            else
            {
                DrawLine(prevPoint, point, color);
            }
        
            prevPoint = point;
        }
    
        // 闭合圆：连接最后一个点到第一个点
        DrawLine(prevPoint, firstPoint, color);
    }

    void GizmoRenderer::DrawWireSphere(const glm::vec3& position, float radius, const glm::vec4& color, int segments)
    {
        // 参数校验
        segments = std::max(segments, 3);
        radius = std::max(radius, 0.0f);
    
        // 绘制三个轴对齐的圆环
        // XY平面（Z轴法线）
        DrawWireCircle(position, glm::vec3(0.0f, 0.0f, 1.0f), radius, color, segments);
        // XZ平面（Y轴法线）
        DrawWireCircle(position, glm::vec3(0.0f, 1.0f, 0.0f), radius, color, segments);
        // YZ平面（X轴法线）
        DrawWireCircle(position, glm::vec3(1.0f, 0.0f, 0.0f), radius, color, segments);
    
        // 视角平面圆环（始终面向相机）
        glm::vec3 viewDir = glm::normalize(position - s_GizmoData.CameraPosition);
        DrawWireCircle(position, viewDir, radius, color, segments);
    }

    // ---- 场景 Gizmo ----
    
    void GizmoRenderer::DrawGrid(float size, int divisions, const glm::vec4& color)
    {
        float step = size / divisions;
        float halfSize = size / 2.0f;
    
        for (int i = 0; i <= divisions; ++i)
        {
            float pos = -halfSize + i * step;
        
            if (i == divisions / 2)
            {
                continue;
            }
            
            // X 方向线段
            DrawLine(glm::vec3(pos, 0.0f, -halfSize), glm::vec3(pos, 0.0f, halfSize), color);
            // Z 方向线段
            DrawLine(glm::vec3(-halfSize, 0.0f, pos), glm::vec3(halfSize, 0.0f, pos), color);
        }
    
        // 高亮中心轴线
        DrawLine(glm::vec3(-halfSize, 0.0f, 0.0f), glm::vec3(halfSize, 0.0f, 0.0f), glm::vec4(1.0f, 0.2f, 0.322f, 1.0f));   // X 轴红色
        DrawLine(glm::vec3(0.0f, 0.0f, -halfSize), glm::vec3(0.0f, 0.0f, halfSize), glm::vec4(0.157f, 0.565f, 1.0f, 1.0f)); // Z 轴蓝色
    }
    
    void GizmoRenderer::DrawDirectionalLightGizmo(const glm::vec3& position, const glm::vec3& direction, const glm::vec3& color)
    {
        glm::vec4 c = glm::vec4(color, 1.0f);
        float length = 1.5f;
    
        // 绘制多条平行箭头表示方向光
        glm::vec3 dir = glm::normalize(direction);
    
        // 计算垂直于方向的两个轴
        glm::vec3 up = glm::abs(dir.y) < 0.99f ? glm::vec3(0, 1, 0) : glm::vec3(1, 0, 0);
        glm::vec3 right = glm::normalize(glm::cross(dir, up));
        up = glm::normalize(glm::cross(right, dir));
        
        DrawLine(position, position + dir * length, c);
    }
    
    void GizmoRenderer::DrawPointLightGizmo(const glm::vec3& position, float range, const glm::vec3& color)
    {
        glm::vec4 c = glm::vec4(color, 1.0f);
        
        // TODO 位置使用图标
        
        // 动态计算segments：基于range使圆环光滑
        constexpr float PI = glm::pi<float>();
        constexpr float desiredArcLength = 0.1f; // 期望每个段的弧长（世界单位），可调
        int segments = std::clamp(static_cast<int>(2.0f * PI * range / desiredArcLength), 8, 64);
        
        // 光照范围
        glm::vec3 viewDir = glm::normalize(position - s_GizmoData.CameraPosition);
        DrawWireCircle(position, viewDir, range, c, segments);
    }
    
    void GizmoRenderer::DrawSpotLightGizmo(const glm::vec3& position, const glm::vec3& direction, float range, float innerAngle, float outerAngle, const glm::vec3& color)
    {
        glm::vec4 c = glm::vec4(color, 1.0f);
    
        // 动态计算segments：基于range使圆环光滑
        constexpr float PI = glm::pi<float>();
        constexpr float desiredArcLength = 0.1f; // 期望每个段的弧长（世界单位），可调
        int segments = std::clamp(static_cast<int>(2.0f * PI * range / desiredArcLength), 8, 64);
        
        // 归一化方向
        glm::vec3 dir = glm::normalize(direction);
        
        // 计算圆环中心
        glm::vec3 center = position + dir * range;
        
        // 计算外锥角半径（outerAngle是半角度数）
        float radiusOuter = range * tan(glm::radians(outerAngle));
        
        // 计算内锥角半径（innerAngle是半角度数）
        float radiusInner = range * tan(glm::radians(innerAngle));
        
        // 绘制外锥角圆环
        DrawWireCircle(center, dir, radiusOuter, c, segments);
        
        // 绘制内锥角圆环
        DrawWireCircle(center, dir, radiusInner, c, segments);
        
        // 计算圆环平面正交基向量（同DrawWireCircle）
        glm::vec3 up = glm::abs(dir.y) < 0.99f ? glm::vec3(0, 1, 0) : glm::vec3(1, 0, 0);
        glm::vec3 right = glm::normalize(glm::cross(dir, up));
        up = glm::normalize(glm::cross(right, dir));
        
        // 绘制四条线：从position到外锥角圆环上的四个点
        for (int i = 0; i < 4; ++i)
        {
            float theta = i * PI / 2.0f; // 0, π/2, π, 3π/2
            float cosTheta = cos(theta);
            float sinTheta = sin(theta);
            
            // 计算圆环上的点
            glm::vec3 point = center + radiusOuter * (cosTheta * right + sinTheta * up);
            
            // 绘制线
            DrawLine(position, point, c);
        }
    }
}
