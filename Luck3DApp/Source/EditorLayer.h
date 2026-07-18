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
        /// 确保默认场景就绪（编辑器启动、且命令行未指定场景时调用）
        /// - 若 Assets/Scenes/New Scene.luck3d 已存在 → 加载
        /// - 若不存在 → 构造包含 Cube + DirectionalLight 的默认场景并落盘
        /// </summary>
        void EnsureDefaultScene();

        /// <summary>
        /// 保存场景到当前场景
        /// </summary>
        void SaveScene();

        /// <summary>
        /// 序列化场景
        /// </summary>
        /// <param name="scene">场景</param>
        /// <param name="filepath">路径</param>
        void SerializeScene(Ref<Scene> scene, const std::filesystem::path& filepath);
        
        void ImportModel();
        
        /// <summary>
        /// 导入模型
        /// </summary>
        /// <param name="filepath">文件路径</param>
        void ImportModel(const std::filesystem::path& filepath);
    private:
        EditorDockSpace m_EditorDockSpace;  // 停靠空间

        Scope<PanelManager> m_PanelManager; // 编辑器面板管理器
    };
}
