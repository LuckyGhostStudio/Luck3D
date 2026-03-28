#pragma once

#include "Lucky/Renderer/Material.h"

namespace Lucky
{
    /// <summary>
    /// 网格渲染器组件：持有材质列表，材质索引对应 Mesh 中 SubMesh 的 MaterialIndex
    /// </summary>
    struct MeshRendererComponent
    {
        std::vector<Ref<Material>> Materials;  // 材质列表

        MeshRendererComponent() = default;
        MeshRendererComponent(const MeshRendererComponent& other) = default;

        /// <summary>
        /// 获取指定索引的材质，索引越界返回 nullptr
        /// </summary>
        /// <param name="index">索引</param>
        /// <returns>材质</returns>
        Ref<Material> GetMaterial(uint32_t index) const
        {
            if (index < Materials.size())
            {
                return Materials[index];
            }
            return nullptr;
        }

        /// <summary>
        /// 设置指定索引的材质，自动扩展列表大小
        /// </summary>
        /// <param name="index">索引</param>
        /// <param name="material">材质</param>
        void SetMaterial(uint32_t index, const Ref<Material>& material)
        {
            if (index >= Materials.size())
            {
                Materials.resize(index + 1);
            }
            Materials[index] = material;
        }
    };
}
