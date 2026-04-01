#pragma once

#include "Lucky.h"

#include "EditorDockSpace.h"
#include "Lucky/Editor/PanelManager.h"

#include <filesystem>

namespace Lucky
{
    class EditorLayer : public Layer
    {
    public:
        EditorLayer();

        ~EditorLayer() override = default;

        void OnAttach() override;

        void OnDetach() override;

        void OnUpdate(DeltaTime dt) override;

        void OnImGuiRender() override;

        void OnEvent(Event& event) override;
    private:
        void UI_DrawMenuBar();
        
        /// <summary>
        /// 创建新场景
        /// </summary>
        void NewScene();

        /// <summary>
        /// 打开场景
        /// </summary>
        void OpenScene();

        /// <summary>
        /// 打开场景
        /// </summary>
        /// <param name="filepath">文件路径</param>
        void OpenScene(const std::filesystem::path& filepath);

        /// <summary>
        /// 保存场景到当前场景
        /// </summary>
        void SaveScene();

        /// <summary>
        /// 场景另存为
        /// </summary>
        void SaveSceneAs();

        /// <summary>
        /// 序列化场景
        /// </summary>
        /// <param name="scene">场景</param>
        /// <param name="path">路径</param>
        void SerializeScene(Ref<Scene> scene, const std::filesystem::path& filepath);
    private:
        EditorDockSpace m_EditorDockSpace;  // 停靠空间

        Scope<PanelManager> m_PanelManager; // 编辑器面板管理器
        
        Ref<Scene> m_Scene;
        std::filesystem::path m_SceneFilePath;
    };
}
