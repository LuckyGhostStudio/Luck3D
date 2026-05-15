#pragma once

#include "Lucky/Renderer/Material.h"
#include "Lucky/Asset/AssetHandle.h"

#include <yaml-cpp/yaml.h>

namespace Lucky
{
    /// <summary>
    /// 材质序列化器：支持内联序列化（到 Emitter/Node）和独立文件序列化（.mat）
    /// </summary>
    class MaterialSerializer
    {
    public:
        // ---- 内联序列化（供 SceneSerializer 内部使用）----

        /// <summary>
        /// 序列化材质到 YAML Emitter（内联模式）
        /// </summary>
        /// <param name="out">YAML 发射器</param>
        /// <param name="material">材质</param>
        static void Serialize(YAML::Emitter& out, const Ref<Material>& material);

        /// <summary>
        /// 从 YAML 节点反序列化材质（内联模式）
        /// </summary>
        /// <param name="materialNode">材质 YAML 节点</param>
        /// <returns>反序列化的材质（失败返回 nullptr）</returns>
        static Ref<Material> Deserialize(const YAML::Node& materialNode);

        // ---- 独立文件序列化（.mat 文件）----

        /// <summary>
        /// 将材质序列化到独立 .mat 文件
        /// 文件包含完整的材质定义（Handle、Shader、RenderState、Properties）
        /// </summary>
        /// <param name="material">要序列化的材质</param>
        /// <param name="filepath">输出文件路径（如 "Assets/Materials/Metal.mat"）</param>
        /// <returns>序列化是否成功</returns>
        static bool SerializeToFile(const Ref<Material>& material, const std::string& filepath);

        /// <summary>
        /// 从独立 .mat 文件反序列化材质
        /// </summary>
        /// <param name="filepath">材质文件路径</param>
        /// <returns>反序列化的材质（失败返回 nullptr）</returns>
        static Ref<Material> DeserializeFromFile(const std::string& filepath);
    };
}