#pragma once

#include "Lucky/Editor/EditorPanel.h"
#include "Lucky/Scene/Scene.h"
#include "Lucky/Scene/Entity.h"

#include "imgui/imgui.h"

namespace Lucky
{
    class InspectorPanel : public EditorPanel
    {
    public:
        InspectorPanel() = default;
        InspectorPanel(const Ref<Scene>& scene);
        ~InspectorPanel() override = default;

        void OnUpdate(DeltaTime dt) override;
        
        void OnGUI() override;
        
        void DrawComponents(Entity entity);
        
        void OnEvent(Event& event) override;
    private:
        /// <summary>
        /// 绘制组件
        /// </summary>
        /// <typeparam name="TComponent">组件类型</typeparam>
        /// <typeparam name="UIFunction">组件功能函数类型</typeparam>
        /// <param name="name">组件名</param>
        /// <param name="entity">实体</param>
        /// <param name="OnOpened">组件打开时调用</param>
        template<typename TComponent, typename UIFunction>
        void DrawComponent(const std::string& name, Entity entity, UIFunction OnOpened);
    private:
        Ref<Scene> m_Scene;
    };
    
    template<typename TComponent, typename UIFunction>
    void InspectorPanel::DrawComponent(const std::string& name, Entity entity, UIFunction OnOpened)
    {
        // TComponent 组件存在
        if (entity.HasComponent<TComponent>())
        {
            // 树节点标志：打开|框架|延伸到右边|允许重叠|框架边框
            const ImGuiTreeNodeFlags flags = ImGuiTreeNodeFlags_DefaultOpen | ImGuiTreeNodeFlags_AllowItemOverlap | ImGuiTreeNodeFlags_Framed | ImGuiTreeNodeFlags_SpanAvailWidth;
            
            auto& component = entity.GetComponent<TComponent>();

            ImVec2 contentRegionAvail = ImGui::GetContentRegionAvail(); // 可用区域大小
            
            // 组件结点：组件类的哈希值作为结点 id
            bool opened = ImGui::TreeNodeEx((void*)typeid(TComponent).hash_code(), flags, name.c_str());
            
            ImGui::SameLine(contentRegionAvail.x - 18);
            
            // 组件设置按钮
            if (ImGui::Button("+", { 30, 30 }))
            {
                ImGui::OpenPopup("ComponentSettings");  // 打开弹出框
            }

            // 移除组件
            bool componentRemoved = false;
            // 渲染弹出框
            if (ImGui::BeginPopup("ComponentSettings"))
            {
                // 移除组件菜单项
                if (ImGui::MenuItem("Remove Component"))
                {
                    componentRemoved = true;    // 组件标记为移除
                }

                ImGui::EndPopup();
            }
            
            if (opened)
            {
                OnOpened(component);    // 调用组件功能函数：绘制该组件不同的部分

                ImGui::TreePop();       // 展开结点
            }

            if (componentRemoved)
            {
                entity.RemoveComponent<TComponent>();    // 移除 TComponent 组件
            }
        }
    }
}
