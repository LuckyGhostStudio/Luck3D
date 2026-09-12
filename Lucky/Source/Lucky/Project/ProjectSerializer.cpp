#include "lcpch.h"
#include "ProjectSerializer.h"

#include <fstream>
#include <yaml-cpp/yaml.h>

namespace Lucky
{
    bool ProjectSerializer::Serialize(const Ref<Project>& project, const std::filesystem::path& filepath)
    {
        if (!project)
        {
            LF_CORE_ERROR("ProjectSerializer::Serialize - project is null");
            return false;
        }

        const ProjectConfig& config = project->GetConfig();

        YAML::Emitter out;
        out << YAML::BeginMap;
        out << YAML::Key << "Project" << YAML::Value << YAML::BeginMap;
        out << YAML::Key << "Name" << YAML::Value << config.Name;
        out << YAML::Key << "EngineVersion" << YAML::Value << config.EngineVersion;
        out << YAML::Key << "AssetDirectory" << YAML::Value << config.AssetDirectory;
        out << YAML::Key << "ScriptModulePath" << YAML::Value << config.ScriptModulePath;
        out << YAML::Key << "DefaultNamespace" << YAML::Value << config.DefaultNamespace;
        out << YAML::Key << "StartScene" << YAML::Value << config.StartScene;
        out << YAML::EndMap;
        out << YAML::EndMap;

        std::ofstream fout(filepath);
        if (!fout)
        {
            LF_CORE_ERROR("ProjectSerializer::Serialize - Failed to open file for write: '{0}'", filepath.string());
            return false;
        }

        fout << out.c_str();
        LF_CORE_INFO("ProjectSerializer::Serialize - Saved '{0}'", filepath.string());
        return true;
    }

    Ref<Project> ProjectSerializer::Deserialize(const std::filesystem::path& filepath)
    {
        if (!std::filesystem::exists(filepath))
        {
            LF_CORE_ERROR("ProjectSerializer::Deserialize - File not found: '{0}'", filepath.string());
            return nullptr;
        }

        YAML::Node root;
        try
        {
            root = YAML::LoadFile(filepath.string());
        }
        catch (const YAML::Exception& e)
        {
            LF_CORE_ERROR("ProjectSerializer::Deserialize - YAML parse error in '{0}': {1}", filepath.string(), e.what());
            return nullptr;
        }

        YAML::Node projectNode = root["Project"];
        if (!projectNode || !projectNode.IsMap())
        {
            LF_CORE_ERROR("ProjectSerializer::Deserialize - Missing 'Project' node: '{0}'", filepath.string());
            return nullptr;
        }

        ProjectConfig config;

        if (!projectNode["Name"] || !projectNode["AssetDirectory"])
        {
            LF_CORE_ERROR("ProjectSerializer::Deserialize - Missing required field(s) 'Name' or 'AssetDirectory': '{0}'", filepath.string());
            return nullptr;
        }

        config.Name = projectNode["Name"].as<std::string>();
        config.AssetDirectory = projectNode["AssetDirectory"].as<std::string>();

        if (projectNode["EngineVersion"])
        {
            config.EngineVersion = projectNode["EngineVersion"].as<std::string>();
        }
        if (projectNode["ScriptModulePath"])
        {
            config.ScriptModulePath = projectNode["ScriptModulePath"].as<std::string>();
        }
        if (projectNode["DefaultNamespace"])
        {
            config.DefaultNamespace = projectNode["DefaultNamespace"].as<std::string>();
        }
        if (projectNode["StartScene"])
        {
            config.StartScene = projectNode["StartScene"].as<std::string>();
        }

        Ref<Project> project = Ref<Project>(new Project());
        project->m_Config = config;
        return project;
    }
}
