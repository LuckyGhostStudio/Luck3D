#pragma once

#include "Lucky/Renderer/Material.h"

#include <yaml-cpp/yaml.h>

namespace Lucky
{
    /// <summary>
    /// 材质序列化器 当前用于内联到场景文件中 TODO to .mat 文件
    /// </summary>
    class MaterialSerializer
    {
    public:
        /// <summary>
        /// 序列化到 YAML Emitter
        /// </summary>
        /// <param name="out">YAML 发射器</param>
        /// <param name="material">材质</param>
        static void Serialize(YAML::Emitter& out, const Ref<Material>& material);

        /// <summary>
        /// 反序列化材质
        /// </summary>
        /// <param name="materialNode">材质 YAML 节点</param>
        /// <returns>反序列化的材质（失败返回 nullptr）</returns>
        static Ref<Material> Deserialize(const YAML::Node& materialNode);
    };
}