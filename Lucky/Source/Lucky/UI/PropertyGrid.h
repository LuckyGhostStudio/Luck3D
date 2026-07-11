#pragma once

#include "Lucky/Renderer/Texture.h"
#include "Lucky/Asset/Asset.h"
#include "Lucky/Asset/AssetManager.h"
#include "Lucky/Editor/EditorIconManager.h"
#include "Lucky/Editor/DragDropPayloads.h"
#include "Lucky/Editor/DragDropContext.h"

#include "Controls.h"
#include "UICore.h"
#include "Theme.h"

#include <glm/glm.hpp>
#include <type_traits>

namespace Lucky::UI
{
    /// <summary>
    /// 开始 PropertyGrid 两列布局（Label 列 + Value 列）
    /// 必须与 EndPropertyGrid() 配对使用
    /// 在 BeginPropertyGrid 和 EndPropertyGrid 之间调用 Property 系列函数
    /// </summary>
    void BeginPropertyGrid();

    /// <summary>
    /// 结束 PropertyGrid 两列布局
    /// </summary>
    void EndPropertyGrid();

    // ---- 内部辅助函数（供模板使用，外部不应直接调用） ----

    /// <summary>
    /// 绘制 Property 行的 Label 列
    /// </summary>
    void PropertyLabel(const char* label);

    /// <summary>
    /// 开始绘制 Property 行的 Value 列
    /// </summary>
    void PropertyValueBegin();

    /// <summary>
    /// 结束绘制 Property 行的 Value 列
    /// </summary>
    void PropertyValueEnd();

    // ========================================================================
    // Property 语义化控件
    // 内置 Label+控件两列布局，参数精简，面板代码直接调用
    // 所有 Property 函数必须在 BeginPropertyGrid / EndPropertyGrid 之间调用
    // 返回值统一为 bool modified（值是否被用户修改）
    // ========================================================================

    // ---- Float 系列 ----

    /// <summary>
    /// 浮点数属性（单个 float）
    /// </summary>
    /// <param name="label">属性名（显示在左侧 Label 列）</param>
    /// <param name="value">浮点数值引用</param>
    /// <param name="delta">拖拽速度（默认 0.1）</param>
    /// <param name="min">最小值（默认 0.0，min == max 时无限制）</param>
    /// <param name="max">最大值（默认 0.0）</param>
    /// <returns>值是否被修改</returns>
    bool PropertyFloat(const char* label, float& value, float delta = 0.1f, float min = 0.0f, float max = 0.0f);

    /// <summary>
    /// 二维浮点数属性（glm::vec2）
    /// </summary>
    bool PropertyFloat2(const char* label, glm::vec2& value, float delta = 0.1f, float min = 0.0f, float max = 0.0f);

    /// <summary>
    /// 三维浮点数属性（glm::vec3）
    /// </summary>
    bool PropertyFloat3(const char* label, glm::vec3& value, float delta = 0.1f, float min = 0.0f, float max = 0.0f);

    /// <summary>
    /// 四维浮点数属性（glm::vec4）
    /// </summary>
    bool PropertyFloat4(const char* label, glm::vec4& value, float delta = 0.1f, float min = 0.0f, float max = 0.0f);

    // ---- Int 系列 ----

    /// <summary>
    /// 整数拖拽属性
    /// </summary>
    /// <param name="label">属性名</param>
    /// <param name="value">整数值引用</param>
    /// <param name="delta">拖拽速度（默认 1.0）</param>
    /// <param name="min">最小值</param>
    /// <param name="max">最大值</param>
    /// <returns>值是否被修改</returns>
    bool PropertyInt(const char* label, int& value, float delta = 1.0f, int min = 0, int max = 0);

    // ---- Color 系列 ----

    /// <summary>
    /// RGB 颜色属性（glm::vec3）
    /// </summary>
    bool PropertyColor(const char* label, glm::vec3& value);
    
    /// <summary>
    /// RGB 颜色属性（glm::vec4）
    /// </summary>
    bool PropertyColor(const char* label, glm::vec4& value);

    // ---- String 系列 ----

    /// <summary>
    /// 文本输入属性
    /// </summary>
    /// <param name="label">属性名</param>
    /// <param name="value">字符串缓冲区</param>
    /// <param name="bufSize">缓冲区大小</param>
    /// <returns>值是否被修改</returns>
    bool PropertyString(const char* label, char* value, size_t bufSize);

    /// <summary>
    /// 只读文本属性
    /// </summary>
    /// <param name="label">属性名</param>
    /// <param name="value">显示的文本</param>
    void PropertyReadOnlyString(const char* label, const char* value);

    // ---- Combo 下拉框 ----

    /// <summary>
    /// 下拉选择框属性
    /// </summary>
    /// <param name="label">属性名</param>
    /// <param name="selected">当前选中索引引用</param>
    /// <param name="options">选项字符串数组</param>
    /// <param name="count">选项数量</param>
    /// <returns>选中项是否被修改</returns>
    bool PropertyCombo(const char* label, int& selected, const char* const* options, int count);

    // ---- Checkbox ----

    /// <summary>
    /// 复选框属性
    /// </summary>
    /// <param name="label">属性名</param>
    /// <param name="value">布尔值引用</param>
    /// <returns>值是否被修改</returns>
    bool PropertyCheckbox(const char* label, bool& value);

    // ---- Texture ----

    /// <summary>
    /// 纹理属性（带预览图和选择按钮）
    /// 内部处理：默认纹理、ID 管理、引擎类型转换、文件对话框
    /// </summary>
    /// <param name="label">属性名</param>
    /// <param name="texture">纹理引用</param>
    /// <returns>纹理是否被修改</returns>
    bool PropertyTexture(const char* label, const Ref<Texture2D>& texture);

    // ---- Asset 引用 ----

    /// <summary>
    /// 资产引用属性（AssetField）
    /// 显示资产类型图标 + 资产名称，支持拖拽赋值（后续实现）
    /// </summary>
    /// <typeparam name="T">资产类型（Material / Mesh / Texture2D 等，必须继承自 Asset 且提供 StaticAssetType()）</typeparam>
    /// <param name="label">属性名（显示在左侧 Label 列）</param>
    /// <param name="assetRef">资产引用（可被拖拽赋值修改）</param>
    /// <returns>资产引用是否被修改</returns>
    template<typename T>
        requires std::is_base_of_v<Asset, T>
    bool PropertyAsset(const char* label, Ref<T>& assetRef)
    {
        AssetType assetType = T::StaticAssetType();

        BeginPropertyGrid();
        
        PropertyLabel(label);
        PropertyValueBegin();

        // 获取资产类型图标
        const Ref<Texture2D>& icon = EditorIconManager::GetAssetTypeIcon(assetType);
        
        // 构建显示名称：有资产时显示名称，无资产时显示 "None (Type)"
        std::string displayName = assetRef ? assetRef->GetName() : std::string("None (") + AssetTypeToString(assetType) + ")";
        
        bool clicked = AssetField(GenerateID(), icon, displayName.c_str());
        bool modified = false;

        // 拖拽接收：合并 peek 与 delivery
        // - AcceptBeforeDelivery：拖拽悬停期间 payload 就会返回，可用于源端图标反馈
        // - IsDelivery()：区分"悬停中"与"鼠标释放"，只有释放时才真正赋值
        if (ImGui::BeginDragDropTarget())
        {
            // AcceptBeforeDelivery：悬停期间即可拿到 payload（用于源端图标反馈 & 目标端高亮）
            // 不加 AcceptNoDrawDefaultRect，让 ImGui 自动绘制默认高亮框（ImGuiCol_DragDropTarget）
            const ImGuiPayload* payload = ImGui::AcceptDragDropPayload(
                DragDrop::AssetHandle,
                ImGuiDragDropFlags_AcceptBeforeDelivery);
            if (payload && payload->DataSize == sizeof(AssetHandle))
            {
                AssetHandle handle = *static_cast<AssetHandle*>(payload->Data);
                if (AssetManager::GetAssetType(handle) == assetType)
                {
                    // 类型匹配 -> 向源端上报"当前帧被接受"，源端 tooltip 显示允许图标
                    DragDropContext::NotifyTargetAccepts(DragDrop::AssetHandle);

                    // 鼠标释放时才真正写入引用
                    if (payload->IsDelivery())
                    {
                        assetRef = AssetManager::GetAsset<T>(handle);
                        modified = true;
                    }
                }
            }
            ImGui::EndDragDropTarget();
        }

        PropertyValueEnd();
        
        EndPropertyGrid();
        
        return clicked || modified;
    }
    
    // ---- Object TODO ----
    
    bool PropertyObject(const char* label, const char* valueName);
}