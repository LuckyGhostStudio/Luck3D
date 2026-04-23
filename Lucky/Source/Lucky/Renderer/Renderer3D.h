#pragma once

#include "Camera.h"
#include "EditorCamera.h"
#include "Framebuffer.h"

#include "Texture.h"
#include "Mesh.h"
#include "Material.h"

#include <unordered_set>

namespace Lucky
{
    constexpr static int s_MaxDirectionalLights = 4;
    constexpr static int s_MaxPointLights = 8;
    constexpr static int s_MaxSpotLights = 4;
    
    /// <summary>
    /// 方向光 GPU 数据
    /// </summary>
    struct DirectionalLightData
    {
        glm::vec3 Direction = glm::vec3(0.0f, -1.0f, 0.0f); // 光照方向（世界空间）
        float Intensity = 1.0f;                             // 光照强度
        glm::vec3 Color = glm::vec3(1.0f);                  // 光照颜色
        char padding[4];                                    // 填充到 16 字节对齐
    };
        
    /// <summary>
    /// 点光源 GPU 数据
    /// </summary>
    struct PointLightData
    {
        glm::vec3 Position = glm::vec3(0.0f);   // 位置
        float Intensity = 1.0f;                 // 强度
        glm::vec3 Color = glm::vec3(1.0f);      // 颜色
        float Range = 10.0f;                    // 范围
    };
        
    /// <summary>
    /// 聚光灯 GPU 数据
    /// </summary>
    struct SpotLightData
    {
        glm::vec3 Position = glm::vec3(0.0f);               // 位置
        float Intensity = 1.0f;                             // 强度
        glm::vec3 Direction = glm::vec3(0.0f, -1.0f, 0.0f); // 方向
        float Range = 10.0f;                                // 范围
        glm::vec3 Color = glm::vec3(1.0f);                  // 颜色
        float InnerCutoff = 0.9763f;                        // 内锥角（全亮区域）cos(12.5°)
        float OuterCutoff = 0.9537f;                        // 外锥角（衰减到 0 的边界）cos(17.5°)
        char padding[12];                                   // 填充到 16 字节对齐
    };
    
    /// <summary>
    /// 场景光照数据：从 Scene 传递给 Renderer3D
    /// </summary>
    struct SceneLightData
    {
        int DirectionalLightCount = 0;
        DirectionalLightData DirectionalLights[s_MaxDirectionalLights]; // 方向光数组
        
        int PointLightCount = 0;
        PointLightData PointLights[s_MaxPointLights];                   // 点光源数组
        
        int SpotLightCount = 0;
        SpotLightData SpotLights[s_MaxSpotLights];                      // 聚光灯数组
    };
    
    class Renderer3D
    {
    public:
        /// <summary>
        /// 初始化渲染器
        /// </summary>
        static void Init();

        static void Shutdown();

        /// <summary>
        /// 开始渲染场景
        /// </summary>
        /// <param name="camera">编辑器相机</param>
        /// <param name="lightData">光照数据</param>
        static void BeginScene(const EditorCamera& camera, const SceneLightData& lightData);

        /// <summary>
        /// 开始渲染场景：设置场景参数
        /// </summary>
        /// <param name="camera">相机</param>
        /// <param name="transform">Transform</param>
        static void BeginScene(const Camera& camera, const glm::mat4& transform);

        /// <summary>
        /// 结束渲染场景
        /// </summary>
        static void EndScene();
        
        /// <summary>
        /// 绘制网格（使用指定材质列表）
        /// 材质列表的索引对应 SubMesh 的 MaterialIndex
        /// </summary>
        /// <param name="transform">模型变换矩阵</param>
        /// <param name="mesh">网格</param>
        /// <param name="materials">材质列表</param>
        /// <param name="entityID">实体 ID（用于鼠标拾取，-1 表示无效）</param>
        static void DrawMesh(const glm::mat4& transform, Ref<Mesh>& mesh, const std::vector<Ref<Material>>& materials, int entityID = -1);

        /// <summary>
        /// 统计数据
        /// </summary>
        struct Statistics
        {
            uint32_t DrawCalls = 0;     // 绘制调用次数
            uint32_t TriangleCount = 0; // 三角形个数

            /// <summary>
            /// 返回总顶点个数
            /// </summary>
            /// <returns></returns>
            uint32_t GetTotalVertexCount() const { return TriangleCount * 3; }

            /// <summary>
            /// 返回总索引个数
            /// </summary>
            /// <returns></returns>
            uint32_t GetTotalIndexCount() const { return TriangleCount * 6; }
        };

        static Statistics GetStats();

        /// <summary>
        /// 重置统计数据
        /// </summary>
        static void ResetStats();
        
        static Ref<ShaderLibrary>& GetShaderLibrary();
        static Ref<Material>& GetInternalErrorMaterial();
        static Ref<Material>& GetDefaultMaterial();
        static const Ref<Texture2D>& GetDefaultTexture(TextureDefault type);
        
        /// <summary>
        /// 设置主 FBO 引用（描边合成后需要重新绑定）
        /// 在 SceneViewportPanel::OnUpdate 中调用，位于 Framebuffer::Bind() 之后
        /// </summary>
        static void SetTargetFramebuffer(const Ref<Framebuffer>& framebuffer);
        
        /// <summary>
        /// 设置需要描边的实体 ID 集合
        /// 在 SceneViewportPanel::OnUpdate 中调用，位于 BeginScene() 之前
        /// 空集合表示无选中
        /// </summary>
        static void SetOutlineEntities(const std::unordered_set<int>& entityIDs);
        
        /// <summary>
        /// 设置描边颜色
        /// </summary>
        static void SetOutlineColor(const glm::vec4& color);
        
        /// <summary>
        /// 同步 Silhouette FBO 大小（视口 Resize 时调用）
        /// </summary>
        static void ResizeSilhouetteFBO(uint32_t width, uint32_t height);
        
        /// <summary>
        /// 渲染描边（在 Gizmo 之后调用，确保描边覆盖在 Gizmo 之上）
        /// 执行 Silhouette 渲染 + 边缘检测描边合成
        /// 调用后会清空 DrawCommands
        /// </summary>
        static void RenderOutline();
    };
}