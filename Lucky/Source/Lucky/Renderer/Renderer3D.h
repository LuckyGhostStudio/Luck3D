#pragma once

#include "Camera.h"
#include "EditorCamera.h"

#include "Texture.h"
#include "Mesh.h"
#include "Material.h"

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
        static void DrawMesh(const glm::mat4& transform, Ref<Mesh>& mesh, const std::vector<Ref<Material>>& materials);

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
    };
}