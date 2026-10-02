#pragma once

#include "Asset.h"

#include <string>

namespace Lucky
{
    /// <summary>
    /// 脚本资产：代表项目里的一个 .cs 文件
    /// 只承载该脚本的类名词干，不做类解析与实例化（那是 ScriptEngine 的职责）
    /// </summary>
    class Script : public Asset
    {
    public:
        /// <summary>
        /// 构造脚本资产
        /// </summary>
        /// <param name="className">文件名词干（如 "PlayerController"），同时作为资产显示名</param>
        Script(const std::string& className)
            : m_ClassName(className)
        {
            SetName(className);
        }

        static AssetType StaticAssetType() { return AssetType::Script; }

        AssetType GetAssetType() const override { return AssetType::Script; }

        /// <summary>
        /// 获取类名词干（取自文件名，如 "PlayerController"）
        /// 该名字是否对应程序集中真实存在的类，由 ScriptEngine 负责校验
        /// </summary>
        const std::string& GetClassName() const { return m_ClassName; }

    private:
        std::string m_ClassName;    // 文件名词干
    };
}
