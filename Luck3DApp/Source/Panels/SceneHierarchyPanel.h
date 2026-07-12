#pragma once

#include "Lucky/Editor/EditorPanel.h"
#include "Lucky/Scene/Scene.h"

#include <unordered_map>

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

        /// <summary>
        /// 递归绘制单个实体节点
        /// </summary>
        /// <param name="entity">要绘制的实体</param>
        /// <param name="depth">该实体在 Hierarchy 中的可视缩进层级（根节点=0，其子=1，以此类推）
        /// 用于拖拽 Insert 高亮线的层级锚点 X 计算</param>
        void DrawEntityNode(Entity entity, int depth = 0);

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

        /// <summary>
        /// 帧末统一绘制的拖拽视觉反馈参数（延后到整个 Hierarchy 遍历结束后统一绘制，
        /// 避免高亮被后续兄弟节点选中态 header 蓝色填充遮挡）
        /// 
        /// 三种模式互斥（同一帧至多一种命中）：
        /// - Insert 线（Before / After）：Valid = true，Kind = Line，X/Y 有效
        /// - Inside 高亮框：Valid = true，Kind = Rect，MinX/MinY/MaxX/MaxY 有效
        /// </summary>
        struct PendingInsertLine
        {
            enum class KindType { Line, Rect };

            bool     Valid = false;
            KindType Kind  = KindType::Line;

            // Line 模式（Before / After）
            float X = 0.0f;
            float Y = 0.0f;

            // Rect 模式（Inside）
            float MinX = 0.0f;
            float MinY = 0.0f;
            float MaxX = 0.0f;
            float MaxY = 0.0f;

            // Line 模式下的可选"孤立小圆圈"（After 模式发生 X 层级回退时使用）：
            // 在"被跨越的祖先节点"行底部额外绘制一个圆圈，
            // 直观提示用户当前插入位置发生了从深层到浅层的层级回退
            // - HasExtraDot = false：不绘制
            // - ExtraDotX：圆圈中心 X（通常与主线左端圆圈同 X）
            // - ExtraDotY：圆圈中心 Y（= 被跨越祖先节点的 ItemRectMax.y）
            bool  HasExtraDot = false;
            float ExtraDotX   = 0.0f;
            float ExtraDotY   = 0.0f;
        };
        PendingInsertLine m_PendingInsertLine;

        /// <summary>
        /// 帧内缓存：每个已绘制 entity 的行底部屏幕 Y 坐标（= ItemRectMax.y）
        /// 用于 After 模式发生 X 层级回退时，定位"孤立小圆圈"的 Y 坐标
        /// （被跨越的祖先节点在遍历到当前 target 时已绘制完毕，其 ItemRect 已无法通过 ImGui 查询，
        /// 需要在遍历过程中主动缓存）
        /// 帧首清空，DrawEntityNode 中每绘制一个节点就写入
        /// </summary>
        std::unordered_map<UUID, float> m_EntityBottomY;

        Ref<Scene> m_Scene;
    };
}
