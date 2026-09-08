#pragma once

#include "Lucky/Editor/EditorPanel.h"
#include "Lucky/Editor/EditorIconManager.h"
#include "Lucky/Scene/Scene.h"
#include "Lucky/Scene/SceneManager.h"
#include "Lucky/Scene/Entity.h"
#include "Lucky/Scene/Components/Components.h"
#include "Lucky/Scene/ComponentDescriptor.h"

#include "Lucky/UI/UICore.h"
#include "Lucky/UI/Theme.h"
#include "Lucky/UI/DrawUtils.h"
#include "Lucky/UI/ScopedGuards.h"
#include "Lucky/UI/Widgets.h"

#include "Lucky/Editor/EditorPreferences.h"

#include "imgui/imgui.h"

namespace Lucky
{
    class InspectorPanel : public EditorPanel
    {
    public:
        InspectorPanel() = default;
        InspectorPanel(const Ref<Scene>& scene);
        ~InspectorPanel() override;
        
        void SetScene(const Ref<Scene>& scene);

        void OnUpdate(DeltaTime dt) override;
        
        void OnGUI() override;
        
        void DrawComponents(Entity entity);
        
        void OnEvent(Event& event) override;
    private:
        /// <summary>
        /// 绘制组件通用外壳（HorizontalLine + TreeNode + 图标 + Name + Settings 按钮 + Remove 弹窗）
        /// 打开时调用 desc.Draw(entity) 填充组件内容
        /// </summary>
        /// <param name="entity">实体</param>
        /// <param name="desc">组件描述符</param>
        void DrawComponentHeader(Entity entity, const ComponentDescriptor& desc);

        /// <summary>
        /// 绘制底部 Add Component 按钮及其下拉弹出框
        /// </summary>
        /// <param name="entity">当前选中实体</param>
        void DrawAddComponentButton(Entity entity);

        /// <summary>
        /// 绘制 MeshRenderer / Sprite 的材质编辑器附加块
        /// 这是"跨组件的附加块"（组件面板之后、AddComponent 按钮之前独立绘制），
        /// 不属于任何单个组件的 Draw 范围，因此不进 Registry
        /// </summary>
        /// <param name="entity">当前选中实体</param>
        void DrawMaterialEditors(Entity entity);
    private:
        Ref<Scene> m_Scene;

        // SceneManager 订阅句柄：ctor 中 Subscribe，dtor 中 Unsubscribe
        SceneManager::SubscriptionHandle m_SceneChangedSub = 0;
    };
}
