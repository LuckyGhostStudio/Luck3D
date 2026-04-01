#pragma once

#include "Lucky/Editor/EditorPanel.h"
#include "Lucky/Scene/Scene.h"

namespace Lucky
{
    class SceneHierarchyPanel : public EditorPanel
    {
    public:
        SceneHierarchyPanel() = default;
        SceneHierarchyPanel(const Ref<Scene>& scene);
        ~SceneHierarchyPanel() override = default;
        
        void SetScene(const Ref<Scene>& scene);

        void OnUpdate(DeltaTime dt) override;
        
        void OnGUI() override;
        void DrawEntityNode(Entity entity);
        
        /// <summary>
        /// 绘制实体创建菜单
        /// </summary>
        /// <param name="parent">父节点</param>
        void DrawEntityCreateMenu(Entity parent);
        
        void OnEvent(Event& event) override;
    private:
        /// <summary>
        /// 生成在兄弟节点中不重复的名称（仅用于 Hierarchy 创建时的视觉区分）
        /// </summary>
        /// <param name="baseName">基础名称</param>
        /// <param name="parent">父实体（无效则检查根层级）</param>
        /// <returns>不重复的名称</returns>
        std::string GenerateUniqueName(const std::string& baseName, Entity parent);

        Ref<Scene> m_Scene;
    };
}
