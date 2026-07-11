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
        /// 绘制实体右侧的组件图标列表
        /// 在树节点行的右侧右对齐显示该实体拥有的非默认组件图标
        /// </summary>
        /// <param name="entity">实体</param>
        void DrawEntityComponentIcons(Entity entity);
        
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

        /// <summary>
        /// 检查 potentialChild 是否是 potentialParent 的祖先节点
        /// 用于防止拖拽时形成循环引用
        /// </summary>
        /// <param name="potentialParent">潜在的新父节点（可以是无效 Entity，表示拖到根节点的兄弟位置，一定不会形成循环）</param>
        /// <param name="potentialChild">被拖拽的节点（潜在的新子节点）</param>
        /// <returns>如果会形成循环，返回 true</returns>
        bool WouldCreateCycle(Entity potentialParent, Entity potentialChild);

        Ref<Scene> m_Scene;
    };
}
