#include "lcpch.h"
#include "Renderer2D.h"

#include "RenderCommand.h"
#include "VertexArray.h"
#include "Buffer.h"
#include "Shader.h"
#include "Renderer3D.h"

#include "Lucky/Asset/AssetManager.h"

#include <glm/gtc/matrix_transform.hpp>

#include <array>

namespace Lucky
{
    /// <summary>
    /// 单个 Quad 顶点数据（与 Sprite.vert 顶点属性布局严格一致）
    /// </summary>
    struct QuadVertex
    {
        glm::vec3 Position;         // 世界空间位置
        glm::vec4 Color;            // 顶点颜色
        glm::vec2 TexCoord;         // UV
        float TexIndex;             // 纹理槽索引
        float TilingFactor;         // 平铺倍数
        int EntityID;               // 实体 ID
    };

    /// <summary>
    /// Renderer2D 内部数据（对齐 Renderer3DData 的 s_Data 模式）
    /// </summary>
    struct Renderer2DData
    {
        // ---- 批次容量常量 ----
        static const uint32_t MaxQuads = 10000;
        static const uint32_t MaxVertices = MaxQuads * 4;
        static const uint32_t MaxIndices = MaxQuads * 6;
        static const uint32_t MaxTextureSlots = 32;

        // ---- Quad 批处理资源 ----
        Ref<VertexArray> QuadVertexArray;
        Ref<VertexBuffer> QuadVertexBuffer;
        Ref<Shader> SpriteShader;
        Ref<Shader> SpriteErrorShader;                          // 错误材质专用 Shader
        Ref<Texture2D> WhiteTexture;                            // 槽 0，用于纯色 Quad

        // ---- 材质（决定当前批次的 Shader / RenderState / 合批分组） ----
        Ref<Material> DefaultSpriteMaterial;                    // 默认 Sprite 材质（Init 时创建，EnsureAsset 落盘）
        Ref<Material> SpriteErrorMaterial;                      // Sprite 错误材质（材质丢失时使用）
        Ref<Material> CurrentMaterial;                          // 当前批次使用的材质（BeginScene 重置为默认材质）

        // ---- 顶点数据缓冲（CPU 端累积，Flush 时上传 GPU） ----
        uint32_t QuadIndexCount = 0;                            // 当前批次的索引数
        QuadVertex* QuadVertexBufferBase = nullptr;             // CPU 端缓冲区基地址（Init 时 new，Shutdown 时 delete[]）
        QuadVertex* QuadVertexBufferPtr = nullptr;              // 当前写入位置

        // ---- 纹理槽 ----
        std::array<Ref<Texture2D>, MaxTextureSlots> TextureSlots;
        uint32_t TextureSlotIndex = 1;                          // 0 保留给白色纹理

        // ---- 顶点局部坐标模板（4 个角，中心在原点，边长 1） ----
        // 顺序：左下、右下、右上、左上（与 IBO 索引模式 0,1,2,2,3,0 保持 CCW 绕序）
        glm::vec4 QuadVertexPositions[4] = {
            { -0.5f, -0.5f, 0.0f, 1.0f },
            {  0.5f, -0.5f, 0.0f, 1.0f },
            {  0.5f,  0.5f, 0.0f, 1.0f },
            { -0.5f,  0.5f, 0.0f, 1.0f }
        };

        // ---- 相机 VP 矩阵（BeginScene 时缓存，当前 Shader 通过 UBO 读取，这里仅备用） ----
        glm::mat4 ViewProjection = glm::mat4(1.0f);

        // ---- 统计 ----
        Renderer2D::Statistics Stats;
    };

    static Renderer2DData s_Data;

    // ================================ Init / Shutdown ================================

    void Renderer2D::Init()
    {
        // ---- 创建 VAO ----
        s_Data.QuadVertexArray = VertexArray::Create();

        // ---- 创建动态 VBO（每帧写入） ----
        s_Data.QuadVertexBuffer = VertexBuffer::Create(Renderer2DData::MaxVertices * sizeof(QuadVertex));
        s_Data.QuadVertexBuffer->SetLayout({
            { ShaderDataType::Float3, "a_Position" },
            { ShaderDataType::Float4, "a_Color" },
            { ShaderDataType::Float2, "a_TexCoord" },
            { ShaderDataType::Float, "a_TexIndex" },
            { ShaderDataType::Float, "a_TilingFactor" },
            { ShaderDataType::Int, "a_EntityID" }
        });
        s_Data.QuadVertexArray->AddVertexBuffer(s_Data.QuadVertexBuffer);

        // ---- CPU 端顶点缓冲 ----
        s_Data.QuadVertexBufferBase = new QuadVertex[Renderer2DData::MaxVertices];

        // ---- 静态 IBO（固定 0,1,2,2,3,0 模式，一次生成永不变） ----
        uint32_t* quadIndices = new uint32_t[Renderer2DData::MaxIndices];
        uint32_t offset = 0;
        for (uint32_t i = 0; i < Renderer2DData::MaxIndices; i += 6)
        {
            quadIndices[i + 0] = offset + 0;
            quadIndices[i + 1] = offset + 1;
            quadIndices[i + 2] = offset + 2;
            quadIndices[i + 3] = offset + 2;
            quadIndices[i + 4] = offset + 3;
            quadIndices[i + 5] = offset + 0;
            offset += 4;
        }
        Ref<IndexBuffer> ibo = IndexBuffer::Create(quadIndices, Renderer2DData::MaxIndices);
        s_Data.QuadVertexArray->SetIndexBuffer(ibo);
        delete[] quadIndices;

        // ---- 加载 Sprite / SpriteError Shader ----
        Renderer3D::GetShaderLibrary()->Load("Assets/Shaders/Internal/SpriteError");
        s_Data.SpriteShader = Renderer3D::GetShaderLibrary()->Get("Sprite");
        s_Data.SpriteErrorShader = Renderer3D::GetShaderLibrary()->Get("SpriteError");

        // ---- 复用 Renderer3D 的白色纹理作为槽 0 ----
        s_Data.WhiteTexture = Renderer3D::GetDefaultTexture(TextureDefault::White);
        s_Data.TextureSlots[0] = s_Data.WhiteTexture;

        // ---- 初始化采样器数组 uniform（一次性绑定 samplers[0..31] = {0, 1, ..., 31}） ----
        int samplers[Renderer2DData::MaxTextureSlots];
        for (uint32_t i = 0; i < Renderer2DData::MaxTextureSlots; ++i)
        {
            samplers[i] = (int)i;
        }
        s_Data.SpriteShader->Bind();
        s_Data.SpriteShader->SetIntArray("u_Textures", samplers, Renderer2DData::MaxTextureSlots);

        // ---- 创建默认 Sprite 材质：Sprite Shader + Alpha Blend + Cull Off + ZWrite Off ----
        s_Data.DefaultSpriteMaterial = CreateRef<Material>("Sprite-Default", s_Data.SpriteShader);
        {
            RenderState& state = s_Data.DefaultSpriteMaterial->GetRenderState();
            state.Cull = CullMode::Off;
            state.DepthWrite = false;
            state.DepthTest = DepthCompareFunc::Less;
            state.Blend = BlendMode::SrcAlpha_OneMinusSrcAlpha;
            state.Queue = RenderQueue::Transparent;
        }

        // ---- 将默认 Sprite 材质落盘到 Internal 目录（已存在则仅注册，不会覆盖用户修改） ----
        AssetManager::EnsureAsset(s_Data.DefaultSpriteMaterial, "Assets/Internal/Materials/Sprite-Default.lmat");

        // ---- 创建 Sprite 错误材质（纯运行时兜底，不落盘） ----
        s_Data.SpriteErrorMaterial = CreateRef<Material>("Sprite-Error", s_Data.SpriteErrorShader);
        {
            RenderState& state = s_Data.SpriteErrorMaterial->GetRenderState();
            state.Cull = CullMode::Off;
            state.DepthWrite = false;
            state.DepthTest = DepthCompareFunc::Less;
            state.Blend = BlendMode::SrcAlpha_OneMinusSrcAlpha;
            state.Queue = RenderQueue::Transparent;
        }
    }

    void Renderer2D::Shutdown()
    {
        delete[] s_Data.QuadVertexBufferBase;
        s_Data.QuadVertexBufferBase = nullptr;
        s_Data.QuadVertexBufferPtr = nullptr;

        // 清空 Ref 引用，让底层资源析构
        s_Data.QuadVertexArray.reset();
        s_Data.QuadVertexBuffer.reset();
        s_Data.SpriteShader.reset();
        s_Data.SpriteErrorShader.reset();
        s_Data.WhiteTexture.reset();
        s_Data.DefaultSpriteMaterial.reset();
        s_Data.SpriteErrorMaterial.reset();
        s_Data.CurrentMaterial.reset();
        for (auto& slot : s_Data.TextureSlots)
        {
            slot.reset();
        }
    }

    // ================================ BeginScene / EndScene / Flush ================================

    void Renderer2D::BeginScene(const EditorCamera& camera)
    {
        BeginScene(camera.GetViewMatrix(), camera.GetProjectionMatrix());
    }

    void Renderer2D::BeginScene(const glm::mat4& viewMatrix, const glm::mat4& projectionMatrix)
    {
        s_Data.ViewProjection = projectionMatrix * viewMatrix;

        // Sprite Shader 通过 UBO(binding=0) 读取相机数据，此处不需要 SetMat4
        // 相机 UBO 由 Renderer3D::BeginScene 已上传，Renderer2D 直接复用

        // 重置当前批次状态
        s_Data.QuadIndexCount = 0;
        s_Data.QuadVertexBufferPtr = s_Data.QuadVertexBufferBase;
        s_Data.TextureSlotIndex = 1;                            // 保留槽 0 = 白色纹理
        s_Data.CurrentMaterial = s_Data.DefaultSpriteMaterial;  // 默认材质作为首个批次
    }

    void Renderer2D::EndScene()
    {
        Flush();
    }

    void Renderer2D::Flush()
    {
        if (s_Data.QuadIndexCount == 0)
        {
            return;
        }

        // 上传 CPU 端顶点数据到 GPU
        uint32_t dataSize = static_cast<uint32_t>(
            reinterpret_cast<uint8_t*>(s_Data.QuadVertexBufferPtr) -
            reinterpret_cast<uint8_t*>(s_Data.QuadVertexBufferBase)
        );
        s_Data.QuadVertexBuffer->SetData(s_Data.QuadVertexBufferBase, dataSize);

        // 绑定所有已使用的纹理槽
        for (uint32_t i = 0; i < s_Data.TextureSlotIndex; ++i)
        {
            RenderCommand::BindTextureUnit(i, s_Data.TextureSlots[i]->GetRendererID());
        }

        // 选择批次材质（当前材质丢失时使用错误材质，以洋红色提示用户）
        const Ref<Material>& mat = s_Data.CurrentMaterial ? s_Data.CurrentMaterial : s_Data.SpriteErrorMaterial;

        // 应用材质的 RenderState??每次 Flush 都重新设置，确保与 Renderer3D 遗留状态隔离
        const RenderState& state = mat->GetRenderState();
        RenderCommand::SetCullMode(state.Cull);
        RenderCommand::SetDepthWrite(state.DepthWrite);
        RenderCommand::SetDepthFunc(state.DepthTest);
        RenderCommand::SetBlendMode(state.Blend);

        // 绑定 Shader（Shader 通过 UBO 读取相机数据，无需额外设置 u_ViewProjection）
        mat->GetShader()->Bind();

        // 应用材质的所有 uniform（自定义 Shader 可读到 u_MyParam 等）
        mat->Apply();

        // Draw
        RenderCommand::DrawIndexed(s_Data.QuadVertexArray, s_Data.QuadIndexCount);

        s_Data.Stats.DrawCalls++;
    }

    void Renderer2D::SetBatchMaterial(const Ref<Material>& material)
    {
        // nullptr 视为材质丢失，使用错误材质
        const Ref<Material>& target = material ? material : s_Data.SpriteErrorMaterial;

        // 相同材质实例 → 不断批
        if (target.get() == s_Data.CurrentMaterial.get())
        {
            return;
        }

        // 不同材质：先 Flush 当前批次，再切换
        Flush();
        s_Data.QuadIndexCount = 0;
        s_Data.QuadVertexBufferPtr = s_Data.QuadVertexBufferBase;
        s_Data.TextureSlotIndex = 1;
        s_Data.CurrentMaterial = target;
    }

    const Ref<Material>& Renderer2D::GetDefaultMaterial()
    {
        return s_Data.DefaultSpriteMaterial;
    }

    const Ref<Material>& Renderer2D::GetErrorMaterial()
    {
        return s_Data.SpriteErrorMaterial;
    }

    /// <summary>
    /// 内部：Flush 当前批次并重置批次状态，用于批次容量满 / 纹理槽满时的续批
    /// </summary>
    static void FlushAndReset()
    {
        Renderer2D::Flush();

        s_Data.QuadIndexCount = 0;
        s_Data.QuadVertexBufferPtr = s_Data.QuadVertexBufferBase;
        s_Data.TextureSlotIndex = 1;
    }

    /// <summary>
    /// 内部：提交纹理到槽位，若已存在则复用，若不存在且槽用满则触发 Flush
    /// </summary>
    /// <param name="texture">纹理（nullptr 返回 0，即白色槽）</param>
    /// <returns>纹理槽索引（作为顶点属性 a_TexIndex 的值）</returns>
    static float SubmitTexture(const Ref<Texture2D>& texture)
    {
        if (!texture)
        {
            return 0.0f;    // 槽 0 = 白色
        }

        // 查找是否已存在
        for (uint32_t i = 1; i < s_Data.TextureSlotIndex; ++i)
        {
            if (s_Data.TextureSlots[i]->GetRendererID() == texture->GetRendererID())
            {
                return static_cast<float>(i);
            }
        }

        // 槽满：Flush 并重置
        if (s_Data.TextureSlotIndex >= Renderer2DData::MaxTextureSlots)
        {
            FlushAndReset();
        }

        s_Data.TextureSlots[s_Data.TextureSlotIndex] = texture;
        return static_cast<float>(s_Data.TextureSlotIndex++);
    }

    // ================================ DrawQuad / DrawSprite ================================

    void Renderer2D::DrawQuad(const glm::mat4& transform, const glm::vec4& color, int entityID)
    {
        // 顶点数满：Flush 并重置
        if (s_Data.QuadIndexCount >= Renderer2DData::MaxIndices)
        {
            FlushAndReset();
        }

        constexpr glm::vec2 texCoords[4] = {
            { 0.0f, 0.0f }, { 1.0f, 0.0f }, { 1.0f, 1.0f }, { 0.0f, 1.0f }
        };

        for (int i = 0; i < 4; ++i)
        {
            s_Data.QuadVertexBufferPtr->Position = glm::vec3(transform * s_Data.QuadVertexPositions[i]);
            s_Data.QuadVertexBufferPtr->Color = color;
            s_Data.QuadVertexBufferPtr->TexCoord = texCoords[i];
            s_Data.QuadVertexBufferPtr->TexIndex = 0.0f;        // 白色槽
            s_Data.QuadVertexBufferPtr->TilingFactor = 1.0f;
            s_Data.QuadVertexBufferPtr->EntityID = entityID;
            s_Data.QuadVertexBufferPtr++;
        }

        s_Data.QuadIndexCount += 6;
        s_Data.Stats.QuadCount++;
    }

    void Renderer2D::DrawQuad(const glm::mat4& transform, const Ref<Texture2D>& texture,
                              const glm::vec4& tintColor, const glm::vec4& uvRect,
                              float tilingFactor, bool flipX, bool flipY, int entityID)
    {
        // 顶点数满：Flush 并重置
        if (s_Data.QuadIndexCount >= Renderer2DData::MaxIndices)
        {
            FlushAndReset();
        }

        // 提交纹理并获取槽位索引
        const float texIndex = SubmitTexture(texture);

        // 根据 UVRect + Flip 展开 4 个角的 UV
        // FlipX：交换 u 的左右；FlipY：交换 v 的上下
        const float u0 = flipX ? uvRect.z : uvRect.x;
        const float u1 = flipX ? uvRect.x : uvRect.z;
        const float v0 = flipY ? uvRect.w : uvRect.y;
        const float v1 = flipY ? uvRect.y : uvRect.w;

        const glm::vec2 texCoords[4] = {
            { u0, v0 }, // 左下
            { u1, v0 }, // 右下
            { u1, v1 }, // 右上
            { u0, v1 }  // 左上
        };

        for (int i = 0; i < 4; ++i)
        {
            s_Data.QuadVertexBufferPtr->Position = glm::vec3(transform * s_Data.QuadVertexPositions[i]);
            s_Data.QuadVertexBufferPtr->Color = tintColor;
            s_Data.QuadVertexBufferPtr->TexCoord = texCoords[i];
            s_Data.QuadVertexBufferPtr->TexIndex = texIndex;
            s_Data.QuadVertexBufferPtr->TilingFactor = tilingFactor;
            s_Data.QuadVertexBufferPtr->EntityID = entityID;
            s_Data.QuadVertexBufferPtr++;
        }

        s_Data.QuadIndexCount += 6;
        s_Data.Stats.QuadCount++;
    }

    // ================================ Statistics ================================

    Renderer2D::Statistics Renderer2D::GetStats()
    {
        return s_Data.Stats;
    }

    void Renderer2D::ResetStats()
    {
        s_Data.Stats = Statistics();
    }
}
