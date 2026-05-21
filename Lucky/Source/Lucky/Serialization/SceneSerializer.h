#pragma once

#include "Lucky/Scene/Scene.h"

namespace Lucky
{
    /// <summary>
    /// 场景序列化器：全静态方法，负责场景的 YAML 序列化/反序列化
    /// </summary>
    class SceneSerializer
    {
    public:
        /// <summary>
        /// 将场景序列化到 .luck3d 文件
        /// </summary>
        /// <param name="scene">场景</param>
        /// <param name="filepath">文件输出路径</param>
        static void Serialize(const Ref<Scene>& scene, const std::string& filepath);

        /// <summary>
        /// 从 .luck3d 文件反序列化场景
        /// </summary>
        /// <param name="scene">目标场景（将被填充数据）</param>
        /// <param name="filepath">文件路径</param>
        /// <returns>是否成功</returns>
        static bool Deserialize(const Ref<Scene>& scene, const std::string& filepath);

        /// <summary>
        /// 运行时序列化：序列化为二进制（预留）
        /// </summary>
        /// <param name="scene">场景</param>
        /// <param name="filepath">文件输出路径</param>
        static void SerializeRuntime(const Ref<Scene>& scene, const std::string& filepath);

        /// <summary>
        /// 运行时反序列化：二进制反序列化（预留）
        /// </summary>
        /// <param name="scene">目标场景</param>
        /// <param name="filepath">文件路径</param>
        /// <returns>是否成功</returns>
        static bool DeserializeRuntime(const Ref<Scene>& scene, const std::string& filepath);
    };
}