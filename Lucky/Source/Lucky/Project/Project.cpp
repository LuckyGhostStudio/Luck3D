#include "lcpch.h"
#include "Project.h"
#include "ProjectSerializer.h"

namespace Lucky
{
    Ref<Project> Project::s_ActiveProject = nullptr;

    Ref<Project> Project::Load(const std::filesystem::path& lcprojPath)
    {
        if (!std::filesystem::exists(lcprojPath))
        {
            LF_CORE_ERROR("Project::Load - .lcproj file not found: '{0}'", lcprojPath.string());
            return nullptr;
        }

        Ref<Project> project = ProjectSerializer::Deserialize(lcprojPath);
        if (!project)
        {
            return nullptr;
        }

        project->m_ProjectFilePath = std::filesystem::absolute(lcprojPath).lexically_normal();
        project->m_ProjectDirectory = project->m_ProjectFilePath.parent_path();

        SetActive(project);

        LF_CORE_INFO("Project::Load - Loaded '{0}' at '{1}'", project->m_Config.Name, project->m_ProjectDirectory.string());
        return project;
    }

    Ref<Project> Project::Create(const std::filesystem::path& projectDirectory, const ProjectConfig& config)
    {
        Ref<Project> project = Ref<Project>(new Project());
        project->m_ProjectDirectory = std::filesystem::absolute(projectDirectory).lexically_normal();
        project->m_Config = config;
        return project;
    }

    void Project::SetActive(Ref<Project> project)
    {
        s_ActiveProject = project;
    }

    void Project::ClearActive()
    {
        s_ActiveProject.reset();
    }

    std::filesystem::path Project::GetAssetDirectory() const
    {
        return (m_ProjectDirectory / m_Config.AssetDirectory).lexically_normal();
    }

    std::filesystem::path Project::GetAssetRegistryPath() const
    {
        return (m_ProjectDirectory / "AssetRegistry.lcr").lexically_normal();
    }

    std::filesystem::path Project::GetScriptModulePath() const
    {
        return (m_ProjectDirectory / m_Config.ScriptModulePath).lexically_normal();
    }

    std::filesystem::path Project::GetStartScenePath() const
    {
        if (m_Config.StartScene.empty())
        {
            return {};
        }
        return (m_ProjectDirectory / m_Config.StartScene).lexically_normal();
    }

    std::filesystem::path Project::ResolveAbsolute(const std::filesystem::path& relativePath) const
    {
        return (m_ProjectDirectory / relativePath).lexically_normal();
    }

    std::string Project::MakeRelative(const std::filesystem::path& absolutePath) const
    {
        std::error_code ec;
        std::filesystem::path rel = std::filesystem::relative(absolutePath, m_ProjectDirectory, ec);
        if (ec || rel.empty())
        {
            return {};
        }

        std::string relStr = rel.generic_string();
        if (relStr.rfind("..", 0) == 0)
        {
            return {};
        }

        return relStr;
    }
}
