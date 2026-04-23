#pragma once

#include <glm/glm.hpp>
#include <string>

namespace Lucky
{
    /// <summary>
    /// 颜色设置：编辑器中所有可配置的颜色
    /// </summary>
    struct ColorSettings
    {
        // ---- Gizmo 颜色 ----
        glm::vec4 GridAxisXColor  = { 1.0f, 0.2f, 0.322f, 1.0f };       // 网格 X 轴（红色）
        glm::vec4 GridAxisZColor  = { 0.157f, 0.566f, 1.0f, 1.0f };     // 网格 Z 轴（蓝色）
        glm::vec4 GridLineColor   = { 0.329f, 0.329f, 0.329f, 0.502f }; // 网格线（灰色半透明）
        
        // ---- 视口颜色 ----
        glm::vec4 ViewportClearColor = { 0.1f, 0.1f, 0.1f, 1.0f };     // 视口背景色
        
        // ---- ImGui UI 颜色 ----
        glm::vec4 WindowBackground       = { 0.1f, 0.105f, 0.11f, 1.0f };       // 普通窗口背景
        glm::vec4 ChildBackground        = { 0.14f, 0.14f, 0.14f, 1.0f };       // 子窗口背景
        glm::vec4 PopupBackground        = { 0.08f, 0.08f, 0.08f, 0.94f };      // 弹出菜单/工具提示背景
        
        glm::vec4 BorderColor            = { 0.43f, 0.43f, 0.50f, 0.50f };      // 边框颜色
        glm::vec4 BorderShadowColor      = { 0.0f, 0.0f, 0.0f, 0.0f };          // 边框阴影（通常透明）
        
        glm::vec4 FrameBackground        = { 0.2f, 0.205f, 0.21f, 1.0f };       // 控件背景（输入框、复选框等）
        glm::vec4 FrameBackgroundHovered = { 0.3f, 0.305f, 0.31f, 1.0f };       // 控件背景（悬停）
        glm::vec4 FrameBackgroundActive  = { 0.15f, 0.1505f, 0.151f, 1.0f };    // 控件背景（激活）
        
        glm::vec4 ButtonColor            = { 0.2f, 0.205f, 0.21f, 1.0f };       // 按钮
        glm::vec4 ButtonHovered          = { 0.3f, 0.305f, 0.31f, 1.0f };       // 按钮（悬停）
        glm::vec4 ButtonActive           = { 0.15f, 0.1505f, 0.151f, 1.0f };    // 按钮（激活）
        
        glm::vec4 HeaderColor            = { 0.2f, 0.205f, 0.21f, 1.0f };       // 标题头（折叠头、树节点、可选项等）
        glm::vec4 HeaderHovered          = { 0.3f, 0.305f, 0.31f, 1.0f };       // 标题头（悬停）
        glm::vec4 HeaderActive           = { 0.15f, 0.1505f, 0.151f, 1.0f };    // 标题头（激活）
        
        glm::vec4 TitleBarBackground     = { 0.15f, 0.1505f, 0.151f, 1.0f };    // 标题栏背景
        glm::vec4 MenuBarBackground      = { 0.14f, 0.14f, 0.14f, 1.0f };       // 菜单栏背景
        
        glm::vec4 TabColor               = { 0.15f, 0.1505f, 0.151f, 1.0f };    // Tab 标签
        glm::vec4 TabHovered             = { 0.38f, 0.3805f, 0.381f, 1.0f };    // Tab 标签（悬停）
        glm::vec4 TabActive              = { 0.28f, 0.2805f, 0.281f, 1.0f };    // Tab 标签（激活）
        
        glm::vec4 TextColor              = { 1.0f, 1.0f, 1.0f, 1.0f };          // 文字颜色
        glm::vec4 TextDisabledColor      = { 0.5f, 0.5f, 0.5f, 1.0f };          // 禁用文字颜色
        glm::vec4 TextSelectedBackground = { 0.26f, 0.59f, 0.98f, 0.35f };      // 文本选中背景
        
        glm::vec4 CheckMarkColor         = { 0.26f, 0.59f, 0.98f, 1.0f };       // 复选框勾选标记
        glm::vec4 SliderGrab             = { 0.24f, 0.52f, 0.88f, 1.0f };       // 滑块抓取手柄
        glm::vec4 SliderGrabActive       = { 0.26f, 0.59f, 0.98f, 1.0f };       // 滑块抓取手柄（激活）
        
        glm::vec4 SeparatorColor         = { 0.14f, 0.14f, 0.14f, 1.0f };       // 分隔线颜色
        
        glm::vec4 ResizeGripColor        = { 0.26f, 0.59f, 0.98f, 0.20f };      // 窗口缩放手柄
        glm::vec4 ResizeGripHovered      = { 0.26f, 0.59f, 0.98f, 0.67f };      // 窗口缩放手柄（悬停）
        glm::vec4 ResizeGripActive       = { 0.26f, 0.59f, 0.98f, 0.95f };      // 窗口缩放手柄（激活）
        
        glm::vec4 ScrollbarBackground    = { 0.02f, 0.02f, 0.02f, 0.53f };      // 滚动条背景
        glm::vec4 ScrollbarGrab          = { 0.31f, 0.31f, 0.31f, 1.0f };       // 滚动条滑块
        glm::vec4 ScrollbarGrabHovered   = { 0.41f, 0.41f, 0.41f, 1.0f };       // 滚动条滑块（悬停）
        glm::vec4 ScrollbarGrabActive    = { 0.51f, 0.51f, 0.51f, 1.0f };       // 滚动条滑块（激活）
        
        glm::vec4 TableHeaderBg          = { 0.19f, 0.19f, 0.20f, 1.0f };       // 表格表头背景
        glm::vec4 TableBorderStrong      = { 0.31f, 0.31f, 0.35f, 1.0f };       // 表格外边框/表头边框
        glm::vec4 TableBorderLight       = { 0.23f, 0.23f, 0.25f, 1.0f };       // 表格内边框
        glm::vec4 TableRowBg             = { 0.0f, 0.0f, 0.0f, 0.0f };          // 表格偶数行背景
        glm::vec4 TableRowBgAlt          = { 1.0f, 1.0f, 1.0f, 0.06f };         // 表格奇数行背景（交替色）
        
        glm::vec4 DragDropTarget         = { 1.0f, 1.0f, 0.0f, 0.90f };         // 拖放目标高亮矩形
        glm::vec4 ModalWindowDimBackground = { 0.80f, 0.80f, 0.80f, 0.35f };    // 模态窗口背景遮罩
        glm::vec4 DockingPreview         = { 0.26f, 0.59f, 0.98f, 0.70f };      // 停靠预览覆盖色
        glm::vec4 DockingEmptyBackground = { 0.20f, 0.20f, 0.20f, 1.0f };       // 空停靠节点背景
    };
    
    /// <summary>
    /// 编辑器偏好设置：全局单例，管理所有编辑器配置
    /// </summary>
    class EditorPreferences
    {
    public:
        /// <summary>
        /// 获取单例实例
        /// </summary>
        static EditorPreferences& Get();
        
        /// <summary>
        /// 获取颜色设置（可修改）
        /// </summary>
        ColorSettings& GetColors() { return m_Colors; }
        
        /// <summary>
        /// 获取颜色设置（只读）
        /// </summary>
        const ColorSettings& GetColors() const { return m_Colors; }
        
        /// <summary>
        /// 将颜色设置应用到 ImGui 主题
        /// </summary>
        void ApplyImGuiColors();
        
        /// <summary>
        /// 重置颜色为默认值
        /// </summary>
        void ResetColorsToDefault();
        
        /// <summary>
        /// 保存偏好设置到文件
        /// </summary>
        /// <param name="filepath">文件路径（默认 "preferences.yaml"）</param>
        void Save(const std::string& filepath = "preferences.yaml");
        
        /// <summary>
        /// 从文件加载偏好设置
        /// </summary>
        /// <param name="filepath">文件路径（默认 "preferences.yaml"）</param>
        /// <returns>是否加载成功</returns>
        bool Load(const std::string& filepath = "preferences.yaml");
        
    private:
        EditorPreferences() = default;
        ~EditorPreferences() = default;
        
        // 禁止拷贝和移动
        EditorPreferences(const EditorPreferences&) = delete;
        EditorPreferences& operator=(const EditorPreferences&) = delete;
        
    private:
        ColorSettings m_Colors;     // 颜色设置
    };
}