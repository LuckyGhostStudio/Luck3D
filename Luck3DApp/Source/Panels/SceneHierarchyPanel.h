#pragma once

#include "Lucky/Editor/EditorPanel.h"
#include "Lucky/Scene/Scene.h"

#include <unordered_map>
#include <unordered_set>

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

        /// <summary>
        /// 拖拽悬停自动展开（对齐 Unity Hierarchy 的 "Spring-loaded folders" 行为）
        /// 
        /// 语义：
        /// - 拖拽 payload 悬停在"非叶且当前折叠"的节点上时，累计悬停秒数
        /// - 累计达到 s_HoverExpandDelay 阈值 → 将该节点加入 m_PendingExpand，下一帧自动展开
        /// - 悬停目标切换到其他节点 / 命中节点变叶子 / 命中节点已展开 → 累计清零
        /// - 本帧无任何命中（拖拽结束或移出所有节点） → 帧末统一清零
        /// </summary>
        struct HoverExpandState
        {
            UUID  HoveredEntityUUID = 0;    // 当前正在累计悬停的节点
            float HoverAccumTime    = 0.0f; // 累计悬停秒数
            bool  HitThisFrame      = false;// 本帧是否有任意节点命中悬停逻辑，帧末据此判断是否清零
        };
        HoverExpandState m_HoverExpand;

        /// <summary>
        /// 待强制展开的节点集合（跨帧一次性生效）
        /// 
        /// 写入时机（3 种）：
        /// 1. 拖拽悬停超过 s_HoverExpandDelay 阈值（"悬停自动展开"）
        /// 2. Drop 到某节点 Inside 模式落地成功后（"未到阈值先释放，节点立即展开"）
        /// 3. 在某节点上右键 Create... 成功创建子节点后（"右键创建子节点后自动展开"）
        /// 
        /// 生效方式：DrawEntityNode 在调用 BeginTreeNode 前，若集合中包含该 id，
        /// 则调用 ImGui::SetNextItemOpen(true, ImGuiCond_Always) 强制打开，随后 erase 掉该 id（一次性）
        /// </summary>
        std::unordered_set<UUID> m_PendingExpand;

        /// <summary>
        /// 拖拽悬停自动展开的阈值秒数（对齐 Unity 约 0.7s）
        /// </summary>
        static constexpr float s_HoverExpandDelay = 0.7f;

        Ref<Scene> m_Scene;
    };
}
