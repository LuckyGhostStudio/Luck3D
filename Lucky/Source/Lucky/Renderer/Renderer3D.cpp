#include "lcpch.h"
#include "Renderer3D.h"

#include "VertexArray.h"
#include "Shader.h"
#include "UniformBuffer.h"
#include "RenderCommand.h"

#include <glm/ext/matrix_transform.hpp>

namespace Lucky
{
    struct Vertex
    {
        glm::vec3 Position; // 位置
        glm::vec4 Color;    // 颜色
        //glm::vec3 Normal; // 法线
        glm::vec2 TexCoord; // 纹理坐标
        float TexIndex;     // 纹理索引
    };

    /// <summary>
    /// 渲染器数据
    /// </summary>
    struct Renderer3DData
    {
        static const uint32_t MeshTriangleCount = 12;   // 当前渲染网格三角形数量
        static const uint32_t MeshVertexCount = 8 * 3;  // 当前渲染网格顶点数
        static const uint32_t MeshIndexCount = 36;      // 当前渲染网格索引数
        static const uint32_t MaxTextureSlots = 32;     // 最大纹理槽数

        Ref<VertexArray> MeshVertexArray;   // 顶点数组
        Ref<VertexBuffer> MeshVertexBuffer; // 顶点缓冲区
        Ref<IndexBuffer> MeshIndexBuffer;   // 索引缓冲区
        Ref<Shader>    MeshShader;          // 着色器
        Ref<Texture2D> WhiteTexture;        // 白色纹理

        Vertex* MeshVertexBufferBase = nullptr; // 顶点缓冲区数据初始地址
        Vertex* MeshVertexBufferPtr = nullptr;  // 顶点缓冲区数据指针
        uint32_t IndexCount = 0;                // 当前已处理总索引个数

        std::array<Ref<Texture2D>, MaxTextureSlots> TextureSlots;   // 纹理槽列表 存储纹理
        uint32_t TextureSlotIndex = 1;                              // 纹理槽索引 0 = White

        std::vector<Vertex> MeshVertexData; // 顶点数据

        Renderer3D::Statistics Stats;       // 统计数据

        /// <summary>
        /// 相机数据
        /// </summary>
        struct CameraData
        {
            glm::mat4 ViewProjectionMatrix; // VP 矩阵
        };

        CameraData CameraBuffer;
        Ref<UniformBuffer> CameraUniformBuffer; // 相机 Uniform 缓冲区
    };

    static Renderer3DData s_Data;   // 渲染器数据

    void Renderer3D::Init()
    {
        s_Data.MeshVertexArray = CreateRef<VertexArray>();  // 创建顶点数组

        // 正方体顶点
        float cubeVertices[] =
        {
            //-----位置坐标-----  ---------颜色---------  ------法向量------
             0.5f, -0.5f,  0.5f, 1.0f, 0.0f, 1.0f, 1.0f,  1.0f,  0.0f,  0.0f,    // A 0
             0.5f, -0.5f,  0.5f, 1.0f, 0.0f, 1.0f, 1.0f,  0.0f, -1.0f,  0.0f,    // A 1
             0.5f, -0.5f,  0.5f, 1.0f, 0.0f, 1.0f, 1.0f,  0.0f,  0.0f,  1.0f,    // A 2

             0.5f, -0.5f, -0.5f, 1.0f, 0.0f, 1.0f, 1.0f,  1.0f,  0.0f,  0.0f,    // B 3
             0.5f, -0.5f, -0.5f, 1.0f, 0.0f, 1.0f, 1.0f,  0.0f, -1.0f,  0.0f,    // B 4
             0.5f, -0.5f, -0.5f, 1.0f, 0.0f, 1.0f, 1.0f,  0.0f,  0.0f, -1.0f,    // B 5

             0.5f,  0.5f, -0.5f, 1.0f, 0.0f, 1.0f, 1.0f,  1.0f,  0.0f,  0.0f,    // C 6
             0.5f,  0.5f, -0.5f, 1.0f, 0.0f, 1.0f, 1.0f,  0.0f,  1.0f,  0.0f,    // C 7
             0.5f,  0.5f, -0.5f, 1.0f, 0.0f, 1.0f, 1.0f,  0.0f,  0.0f, -1.0f,    // C 8

             0.5f,  0.5f,  0.5f, 1.0f, 0.0f, 1.0f, 1.0f,  1.0f,  0.0f,  0.0f,    // D 9
             0.5f,  0.5f,  0.5f, 1.0f, 0.0f, 1.0f, 1.0f,  0.0f,  1.0f,  0.0f,    // D 10
             0.5f,  0.5f,  0.5f, 1.0f, 0.0f, 1.0f, 1.0f,  0.0f,  0.0f,  1.0f,    // D 11

            -0.5f, -0.5f,  0.5f, 1.0f, 0.0f, 1.0f, 1.0f, -1.0f,  0.0f,  0.0f,    // E 12
            -0.5f, -0.5f,  0.5f, 1.0f, 0.0f, 1.0f, 1.0f,  0.0f, -1.0f,  0.0f,    // E 13
            -0.5f, -0.5f,  0.5f, 1.0f, 0.0f, 1.0f, 1.0f,  0.0f,  0.0f,  1.0f,    // E 14

            -0.5f, -0.5f, -0.5f, 1.0f, 0.0f, 1.0f, 1.0f, -1.0f,  0.0f,  0.0f,    // F 15
            -0.5f, -0.5f, -0.5f, 1.0f, 0.0f, 1.0f, 1.0f,  0.0f, -1.0f,  0.0f,    // F 16
            -0.5f, -0.5f, -0.5f, 1.0f, 0.0f, 1.0f, 1.0f,  0.0f,  0.0f, -1.0f,    // F 17

            -0.5f,  0.5f, -0.5f, 1.0f, 0.0f, 1.0f, 1.0f, -1.0f,  0.0f,  0.0f,    // G 18
            -0.5f,  0.5f, -0.5f, 1.0f, 0.0f, 1.0f, 1.0f,  0.0f,  1.0f,  0.0f,    // G 19
            -0.5f,  0.5f, -0.5f, 1.0f, 0.0f, 1.0f, 1.0f,  0.0f,  0.0f, -1.0f,    // G 20

            -0.5f,  0.5f,  0.5f, 1.0f, 0.0f, 1.0f, 1.0f, -1.0f,  0.0f,  0.0f,    // H 21
            -0.5f,  0.5f,  0.5f, 1.0f, 0.0f, 1.0f, 1.0f,  0.0f,  1.0f,  0.0f,    // H 22
            -0.5f,  0.5f,  0.5f, 1.0f, 0.0f, 1.0f, 1.0f,  0.0f,  0.0f,  1.0f,    // H 23
        };

        s_Data.MeshVertexBuffer = CreateRef<VertexBuffer>(s_Data.MeshVertexCount * sizeof(Vertex)); // 创建顶点缓冲

        // 设置顶点缓冲区布局
        s_Data.MeshVertexBuffer->SetLayout({
            {ShaderDataType::Float3, "a_Position"}, // 位置
            {ShaderDataType::Float4, "a_Color"},    // 颜色
            //{ShaderDataType::Float3, "a_Normal"}, // 法线
            {ShaderDataType::Float2, "a_TexCoord"}, // 纹理坐标
            {ShaderDataType::Float, "a_TexIndex"},  // 纹理索引
        });
        s_Data.MeshVertexArray->AddVertexBuffer(s_Data.MeshVertexBuffer);   // 添加顶点缓冲区到顶点数组

        s_Data.MeshVertexBufferBase = new Vertex[s_Data.MeshVertexCount];   // 顶点缓冲区数据

        // 顶点索引数据
        uint32_t cubeIndices[] =
        {
            0, 3, 6,    // A B C x+
            6, 9, 0,    // C D A x+
            18, 15, 12,    // G F E x-
            18, 21, 12,    // G H E x-
            22, 7, 19,    // H C G y+
            7, 10, 22,    // C D H y+
            13, 16, 4,    // E F B y-
            4, 1, 13,    // B A E y-
            23, 14, 2,    // H E A z+
            2, 11, 23,    // A D H z+
            20, 5, 17,    // G B F z-
            5, 20, 8,    // B G C z-
        };

        uint32_t* meshIndices = new uint32_t[s_Data.MeshIndexCount];    // 顶点索引

        // 设置顶点索引
        for (uint32_t i = 0; i < s_Data.MeshIndexCount; i++)
        {
            meshIndices[i] = cubeIndices[i];
        }

        s_Data.MeshIndexBuffer = CreateRef<IndexBuffer>(meshIndices, s_Data.MeshIndexCount);    // 创建索引缓冲
        s_Data.MeshVertexArray->SetIndexBuffer(s_Data.MeshIndexBuffer);                         // 设置索引缓冲
        delete[] meshIndices;   // 释放顶点索引数据

        s_Data.WhiteTexture = Texture2D::Create(1, 1);                      // 创建宽高为 1 的纹理
        uint32_t whitTextureData = 0xffffffff;                              // 255 白色
        s_Data.WhiteTexture->SetData(&whitTextureData, sizeof(uint32_t));   // 设置纹理数据 size = 1 * 1 * 4 == sizeof(uint32_t)
        s_Data.TextureSlots[0] = s_Data.WhiteTexture;                       // 0 号纹理槽为白色纹理（默认）

        // 纹理采样器 0 - 31
        int samplers[s_Data.MaxTextureSlots];
        for (uint32_t i = 0; i < s_Data.MaxTextureSlots; i++)
        {
            samplers[i] = i;
        }

        s_Data.MeshShader = CreateRef<Shader>("Assets/Shaders/TextureShader");  // 创建着色器

        // 设置顶点数据
        for (int i = 0; i < 24; i++)
        {
            Vertex vertex;

            vertex.Position = { cubeVertices[i * 10], cubeVertices[i * 10 + 1], cubeVertices[i * 10 + 2] };
            vertex.Color = { cubeVertices[i * 10 + 3], cubeVertices[i * 10 + 4], cubeVertices[i * 10 + 5], cubeVertices[i * 10 + 6] };
            //vertex.Normal = { cubeVertices[i * 10 + 7], cubeVertices[i * 10 + 8], cubeVertices[i * 10 + 9] };
            vertex.TexCoord = {};
            vertex.TexIndex = 0.0f;

            s_Data.MeshVertexData.push_back(vertex);
        }

        s_Data.CameraUniformBuffer = UniformBuffer::Create(sizeof(Renderer3DData::CameraData), 0);  // 创建相机 Uniform 缓冲区
    }

    void Renderer3D::Shutdown()
    {
        delete[] s_Data.MeshVertexBufferBase;
    }

    void Renderer3D::BeginScene(const EditorCamera& camera)
    {
        s_Data.CameraBuffer.ViewProjectionMatrix = camera.GetViewProjectionMatrix();                    // 设置 VP 矩阵
        s_Data.CameraUniformBuffer->SetData(&s_Data.CameraBuffer, sizeof(Renderer3DData::CameraData));  // 设置 Uniform 缓冲区数据

        s_Data.MeshVertexBufferPtr = s_Data.MeshVertexBufferBase;

        s_Data.TextureSlotIndex = 1;    // 纹理槽索引从 1 开始 0 为白色纹理
    }

    void Renderer3D::EndScene()
    {
        uint32_t dataSize = (uint32_t)((uint8_t*)s_Data.MeshVertexBufferPtr - (uint8_t*)s_Data.MeshVertexBufferBase);   // 数据大小（字节）
        s_Data.MeshVertexBuffer->SetData(s_Data.MeshVertexBufferBase, dataSize);    // 设置顶点缓冲区数据

        // 绑定所有纹理
        for (uint32_t i = 0; i < s_Data.TextureSlotIndex; i++)
        {
            s_Data.TextureSlots[i]->Bind(i);    // 绑定 i 号纹理槽
        }

        s_Data.MeshShader->Bind();   // 绑定着色器

        RenderCommand::DrawIndexed(s_Data.MeshVertexArray, s_Data.MeshIndexCount);  // 绘制索引
    }

    void Renderer3D::DrawMesh(const glm::mat4& transform, const glm::vec4& color)
    {
        constexpr int meshVertexCount = 24;  // 顶点个数

        for (int i = 0; i < meshVertexCount; i++)
        {
            s_Data.MeshVertexBufferPtr->Position = transform * glm::vec4(s_Data.MeshVertexData[i].Position, 1.0f);
            s_Data.MeshVertexBufferPtr->Color = color;
            s_Data.MeshVertexBufferPtr->TexCoord = s_Data.MeshVertexData[i].TexCoord;
            s_Data.MeshVertexBufferPtr->TexIndex = s_Data.MeshVertexData[i].TexIndex;

            s_Data.MeshVertexBufferPtr++;
        }

        // s_Data.MeshIndexCount += 6; // 索引个数增加
    }

    Renderer3D::Statistics Renderer3D::GetStats()
    {
        return s_Data.Stats;
    }

    void Renderer3D::ResetStats()
    {
        memset(&s_Data.Stats, 0, sizeof(Statistics));
    }
}