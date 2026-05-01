#pragma once

#include "Lucky/Core/Base.h"
#include "Lucky/Renderer/Texture.h"

#include <glm/glm.hpp>
#include <glm/gtc/type_ptr.hpp>

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
    
    bool PropertyInt(const char* label, int& value, int min = 0, int max = 0);

    // ---- Color 系列 ----

    /// <summary>
    /// RGB 颜色属性（glm::vec3）
    /// </summary>
    bool PropertyColor(const char* label, glm::vec3& value);

    /// <summary>
    /// RGBA 颜色属性（glm::vec4）
    /// </summary>
    bool PropertyColor4(const char* label, glm::vec4& value);

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
}