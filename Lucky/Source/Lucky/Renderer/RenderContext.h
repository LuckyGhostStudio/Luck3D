#pragma once

#include "Lucky/Core/Base.h"
#include "Framebuffer.h"
#include "Mesh.h"
#include "Material.h"
#include "Renderer3D.h"

#include <glm/glm.hpp>
#include <vector>
#include <unordered_set>

namespace Lucky
{
    /// <summary>
    /// 绘制命令：描述一次 DrawCall 所需的全部信息
    /// 从 Renderer3D::DrawMesh() 中收集，在 EndScene() 中排序后传递给各 Pass
    /// </summary>
    struct DrawCommand
    {
        glm::mat4 Transform;            // 模型变换矩阵
        Ref<Mesh> MeshData;             // 网格引用
        const SubMesh* SubMeshPtr;      // SubMesh 指针（指向 Mesh 内部数据，生命周期由 Mesh 保证）
        Ref<Material> MaterialData;     // 材质引用
        uint64_t SortKey = 0;           // 排序键
        float DistanceToCamera = 0.0f;  // 到相机的距离
        int EntityID = -1;              // Entity ID（用于鼠标拾取，-1 表示无效）
    };
    
    /// <summary>
    /// 描边绘制命令：仅包含 Silhouette 渲染所需的最小数据
    /// 从 DrawCommand 中提取，职责分离，避免 Outline Pass 依赖完整的 DrawCommand
    /// </summary>
    struct OutlineDrawCommand
    {
        glm::mat4 Transform;            // 模型变换矩阵
        Ref<Mesh> MeshData;             // 网格引用（通过 Ref 保证生命周期）
        const SubMesh* SubMeshPtr;      // SubMesh 指针（生命周期由 MeshData 的 Ref 保证）
    };
    
    /// <summary>
    /// 渲染上下文：包含一帧渲染所需的所有数据
    /// 由 Renderer3D 在 EndScene() / RenderOutline() 中构建，传递给 RenderPipeline
    /// 各 Pass 从中获取各自需要的数据
    /// </summary>
    struct RenderContext
    {
        // ---- DrawCommand 列表（已排序） ----
        const std::vector<DrawCommand>* OpaqueDrawCommands = nullptr;       // 不透明物体绘制命令（已按 SortKey 排序）
        // const std::vector<DrawCommand>* TransparentDrawCommands = nullptr; // 透明物体绘制命令（后续阶段）
        
        // ---- Outline 数据 ----
        const std::vector<OutlineDrawCommand>* OutlineDrawCommands = nullptr;   // 描边绘制命令
        const std::unordered_set<int>* OutlineEntityIDs = nullptr;              // 需要描边的 EntityID 集合
        glm::vec4 OutlineColor = glm::vec4(1.0f, 0.4f, 0.0f, 1.0f);           // 描边颜色
        float OutlineWidth = 2.0f;                                              // 描边宽度（像素）
        bool OutlineEnabled = true;                                             // 是否启用描边
        
        // ---- FBO 引用 ----
        Ref<Framebuffer> TargetFramebuffer;     // 主 FBO（OpaquePass / PickingPass / OutlineCompositePass 使用）
        
        // ---- 统计数据（可写） ----
        Renderer3D::Statistics* Stats = nullptr;    // 渲染统计（DrawCalls、TriangleCount）
    };
}
