#pragma once

#include "Lucky/Core/Base.h"

#include <filesystem>
#include <string>

namespace Lucky
{
    /// <summary>
    /// 项目配置：与 .lcproj 文件字段一一对应
    /// </summary>
    struct ProjectConfig
    {
        std::string Name = "Untitled";                                  // 项目名
        std::string EngineVersion = "0.1.0";                            // 记录用引擎版本
        std::string AssetDirectory = "Assets";                          // Assets 子目录名（相对项目根）
        std::string ScriptModulePath = "Binaries/Assembly-CSharp.dll";  // 用户 C# 程序集（相对项目根）
        std::string DefaultNamespace = "Sandbox";                       // 新建脚本的默认命名空间
        std::string StartScene;                                         // 启动场景（相对项目根，可为空）
    };

    /// <summary>
    /// 项目：表示一个已加载的 Luck3D 项目
    /// 提供项目根目录 + 所有项目内路径的绝对/相对路径查询
    /// 由 Project::Load / Project::Create 创建，由 Project::SetActive 设为全局 Active
    /// </summary>
    class Project
    {
    public:
        // ======== 生命周期 ========

        /// <summary>
        /// 从 .lcproj 文件加载项目
        /// </summary>
        /// <param name="lcprojPath">.lcproj 文件绝对路径</param>
        /// <returns>加载成功返回 Project 并设为 Active，失败返回 nullptr</returns>
        static Ref<Project> Load(const std::filesystem::path& lcprojPath);

        /// <summary>
        /// 创建一个内存中的项目（不落盘）
        /// </summary>
        /// <param name="projectDirectory">项目根目录</param>
        /// <param name="config">项目配置</param>
        static Ref<Project> Create(const std::filesystem::path& projectDirectory, const ProjectConfig& config);

        // ======== Active 单例 ========

        /// <summary>
        /// 获取当前 Active 项目（未加载时返回 nullptr）
        /// </summary>
        static const Ref<Project>& GetActive() { return s_ActiveProject; }

        /// <summary>
        /// 设为 Active 项目
        /// </summary>
        static void SetActive(Ref<Project> project);

        /// <summary>
        /// 清除 Active 项目
        /// </summary>
        static void ClearActive();

        // ======== 只读查询 ========

        /// <summary>
        /// 项目根目录（绝对路径）
        /// </summary>
        const std::filesystem::path& GetProjectDirectory() const { return m_ProjectDirectory; }

        /// <summary>
        /// .lcproj 文件绝对路径（Create 时为空）
        /// </summary>
        const std::filesystem::path& GetProjectFilePath() const { return m_ProjectFilePath; }

        /// <summary>
        /// 项目配置（只读）
        /// </summary>
        const ProjectConfig& GetConfig() const { return m_Config; }

        // ======== 派生路径 ========

        /// <summary>
        /// Assets 目录绝对路径 = ProjectDirectory / AssetDirectory
        /// </summary>
        std::filesystem::path GetAssetDirectory() const;

        /// <summary>
        /// AssetRegistry.lcr 文件绝对路径（固定在项目根下）
        /// </summary>
        std::filesystem::path GetAssetRegistryPath() const;

        /// <summary>
        /// 用户 C# 程序集绝对路径 = ProjectDirectory / ScriptModulePath
        /// </summary>
        std::filesystem::path GetScriptModulePath() const;

        /// <summary>
        /// 启动场景绝对路径（StartScene 为空时返回空 path）
        /// </summary>
        std::filesystem::path GetStartScenePath() const;

        /// <summary>
        /// 把项目内相对路径转为绝对路径
        /// </summary>
        /// <param name="relativePath">相对项目根的路径</param>
        std::filesystem::path ResolveAbsolute(const std::filesystem::path& relativePath) const;

        /// <summary>
        /// 把绝对路径转为项目内相对路径（正斜杠格式）；若不在项目内返回空字符串
        /// </summary>
        std::string MakeRelative(const std::filesystem::path& absolutePath) const;

    private:
        Project() = default;

        friend class ProjectSerializer;

    private:
        std::filesystem::path m_ProjectDirectory;   // 项目根绝对路径
        std::filesystem::path m_ProjectFilePath;    // .lcproj 绝对路径（Create 时为空）
        ProjectConfig m_Config;                     // 项目配置

        static Ref<Project> s_ActiveProject;        // 全局唯一 Active
    };
}
