#pragma once

#include <cstdint>
#include <cstring>

namespace Lucky
{
    /// <summary>
    /// 脚本字段类型：本引擎支持在 Inspector 中编辑的全部字段类型
    /// None 表示不支持的类型，枚举时会被跳过
    /// 新增类型时只改本文件：枚举加值 + 表里加一行
    /// </summary>
    enum class ScriptFieldType : uint8_t
    {
        None = 0,

        Bool,
        SByte, Byte, Short, UShort, Int, UInt, Long, ULong,
        Float, Double,
        String,
        Vector2, Vector3, Vector4, Quaternion, Color,
        Entity,
        Material, Mesh, Texture2D, Script
    };

    /// <summary>
    /// 字段在 Inspector 里用哪一类控件
    /// 刻意不引用 ImGuiDataType：本头必须零依赖，UI 层再把这个标识映射到具体控件
    /// </summary>
    enum class ScriptFieldWidgetKind : uint8_t
    {
        None = 0,
        Checkbox,
        Int,
        Float,
        Float2,
        Float3,
        Float4,
        Color,
        Text,
        EntityRef,
        AssetRef
    };

    /// <summary>
    /// 字段类型的完整描述：类型集合的唯一来源
    /// </summary>
    struct ScriptFieldTypeInfo
    {
        ScriptFieldType        Type;
        const char*            Name;         // 序列化用的可读名字，如 "Float"
        const char*            ManagedName;  // 托管侧完整类型名，反射匹配用，如 "System.Single"
        ScriptFieldWidgetKind  Widget;       // 控件种类
    };

    /// <summary>
    /// 全部受支持字段类型；新增类型时在此追加一行即可，各处消费点自动生效
    /// </summary>
    inline constexpr ScriptFieldTypeInfo s_ScriptFieldTypeInfos[] =
    {
        { ScriptFieldType::Bool,       "Bool",       "System.Boolean",   ScriptFieldWidgetKind::Checkbox  },
        { ScriptFieldType::SByte,      "SByte",      "System.SByte",     ScriptFieldWidgetKind::Int       },
        { ScriptFieldType::Byte,       "Byte",       "System.Byte",      ScriptFieldWidgetKind::Int       },
        { ScriptFieldType::Short,      "Short",      "System.Int16",     ScriptFieldWidgetKind::Int       },
        { ScriptFieldType::UShort,     "UShort",     "System.UInt16",    ScriptFieldWidgetKind::Int       },
        { ScriptFieldType::Int,        "Int",        "System.Int32",     ScriptFieldWidgetKind::Int       },
        { ScriptFieldType::UInt,       "UInt",       "System.UInt32",    ScriptFieldWidgetKind::Int       },
        { ScriptFieldType::Long,       "Long",       "System.Int64",     ScriptFieldWidgetKind::Int       },
        { ScriptFieldType::ULong,      "ULong",      "System.UInt64",    ScriptFieldWidgetKind::Int       },
        { ScriptFieldType::Float,      "Float",      "System.Single",    ScriptFieldWidgetKind::Float     },
        { ScriptFieldType::Double,     "Double",     "System.Double",    ScriptFieldWidgetKind::Float     },
        { ScriptFieldType::String,     "String",     "System.String",    ScriptFieldWidgetKind::Text      },
        { ScriptFieldType::Vector2,    "Vector2",    "Lucky.Vector2",    ScriptFieldWidgetKind::Float2    },
        { ScriptFieldType::Vector3,    "Vector3",    "Lucky.Vector3",    ScriptFieldWidgetKind::Float3    },
        { ScriptFieldType::Vector4,    "Vector4",    "Lucky.Vector4",    ScriptFieldWidgetKind::Float4    },
        { ScriptFieldType::Quaternion, "Quaternion", "Lucky.Quaternion", ScriptFieldWidgetKind::Float4    },
        { ScriptFieldType::Color,      "Color",      "Lucky.Color",      ScriptFieldWidgetKind::Color     },
        { ScriptFieldType::Entity,     "Entity",     "Lucky.Entity",     ScriptFieldWidgetKind::EntityRef },
        { ScriptFieldType::Material,   "Material",   "Lucky.Material",   ScriptFieldWidgetKind::AssetRef  },
        { ScriptFieldType::Mesh,       "Mesh",       "Lucky.Mesh",       ScriptFieldWidgetKind::AssetRef  },
        { ScriptFieldType::Texture2D,  "Texture2D",  "Lucky.Texture2D",  ScriptFieldWidgetKind::AssetRef  },
        { ScriptFieldType::Script,     "Script",     "Lucky.Script",     ScriptFieldWidgetKind::AssetRef  },
    };

    /// <summary>
    /// 按类型取描述信息；未登记的类型返回一个 Name 为 "None" 的静态条目
    /// </summary>
    inline const ScriptFieldTypeInfo& GetScriptFieldTypeInfo(ScriptFieldType type)
    {
        for (const ScriptFieldTypeInfo& info : s_ScriptFieldTypeInfos)
        {
            if (info.Type == type)
            {
                return info;
            }
        }

        static const ScriptFieldTypeInfo s_UnknownInfo = { ScriptFieldType::None, "None", "", ScriptFieldWidgetKind::None };
        return s_UnknownInfo;
    }

    /// <summary>
    /// 按托管侧完整类型名查类型；查不到返回 false
    /// </summary>
    /// <param name="managedName">如 "System.Single" / "Lucky.Vector3"</param>
    /// <param name="outType">输出：匹配到的字段类型</param>
    inline bool TryGetScriptFieldTypeByManagedName(const char* managedName, ScriptFieldType& outType)
    {
        if (!managedName)
        {
            return false;
        }

        for (const ScriptFieldTypeInfo& info : s_ScriptFieldTypeInfos)
        {
            if (std::strcmp(info.ManagedName, managedName) == 0)
            {
                outType = info.Type;
                return true;
            }
        }

        return false;
    }
}
