#include "lcpch.h"
#include "ScreenQuad.h"

#include "RenderCommand.h"

namespace Lucky
{
    Ref<VertexArray> ScreenQuad::s_VAO = nullptr;
    Ref<VertexBuffer> ScreenQuad::s_VBO = nullptr;
    
    void ScreenQuad::Init()
    {
        // 全屏四边形顶点数据：位置 (x, y) + 纹理坐标 (u, v)
        float quadVertices[] = {
            // Position    // TexCoord
            -1.0f, -1.0f,  0.0f, 0.0f,   // 左下
             1.0f, -1.0f,  1.0f, 0.0f,   // 右下
             1.0f,  1.0f,  1.0f, 1.0f,   // 右上
            
            -1.0f, -1.0f,  0.0f, 0.0f,   // 左下
             1.0f,  1.0f,  1.0f, 1.0f,   // 右上
            -1.0f,  1.0f,  0.0f, 1.0f    // 左上
        };
        
        s_VAO = VertexArray::Create();
        
        s_VBO = VertexBuffer::Create(quadVertices, sizeof(quadVertices));
        s_VBO->SetLayout({
            { ShaderDataType::Float2, "a_Position" },
            { ShaderDataType::Float2, "a_TexCoord" }
        });
        
        s_VAO->AddVertexBuffer(s_VBO);
    }
    
    void ScreenQuad::Shutdown()
    {
        s_VAO = nullptr;
        s_VBO = nullptr;
    }
    
    void ScreenQuad::Draw()
    {
        s_VAO->Bind();
        RenderCommand::DrawArrays(s_VAO, 6);
    }
}
