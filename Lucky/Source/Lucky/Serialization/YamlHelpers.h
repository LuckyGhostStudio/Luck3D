#pragma once

#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>
#include <yaml-cpp/yaml.h>

#include "Lucky/Core/UUID.h"

namespace YAML
{
    /// <summary>
    /// vec2 转换
    /// </summary>
    template<>
    struct convert<glm::vec2>
    {
        /// <summary>
        /// 将 vec2 转换为 YAML 的节点
        /// </summary>
        /// <param name="rhs">vec2 类型</param>
        /// <returns>结点</returns>
        static Node encode(const glm::vec2& rhs)
        {
            Node node;

            node.push_back(rhs.x);
            node.push_back(rhs.y);

            node.SetStyle(EmitterStyle::Flow);

            return node;
        }

        /// <summary>
        /// 将 YAML 结点类型转换为 vec2
        /// </summary>
        /// <param name="node">结点</param>
        /// <param name="rhs">vec2</param>
        /// <returns>是否转换成功</returns>
        static bool decode(const Node& node, glm::vec2& rhs)
        {
            if (!node.IsSequence() || node.size() != 2)
            {
                return false;
            }

            rhs.x = node[0].as<float>();
            rhs.y = node[1].as<float>();

            return true;
        }
    };

    /// <summary>
    /// vec3 转换
    /// </summary>
    template<>
    struct convert<glm::vec3>
    {
        /// <summary>
        /// 将 vec3 转换为 YAML 的节点
        /// </summary>
        /// <param name="rhs">vec3 类型</param>
        /// <returns>结点</returns>
        static Node encode(const glm::vec3& rhs)
        {
            Node node;

            node.push_back(rhs.x);
            node.push_back(rhs.y);
            node.push_back(rhs.z);

            node.SetStyle(EmitterStyle::Flow);

            return node;
        }

        /// <summary>
        /// 将 YAML 结点类型转换为 vec3
        /// </summary>
        /// <param name="node">结点</param>
        /// <param name="rhs">vec3</param>
        /// <returns>是否转换成功</returns>
        static bool decode(const Node& node, glm::vec3& rhs)
        {
            if (!node.IsSequence() || node.size() != 3)
            {
                return false;
            }

            rhs.x = node[0].as<float>();
            rhs.y = node[1].as<float>();
            rhs.z = node[2].as<float>();

            return true;
        }
    };

    /// <summary>
    /// vec4 转换
    /// </summary>
    template<>
    struct convert<glm::vec4>
    {
        static Node encode(const glm::vec4& rhs)
        {
            Node node;

            node.push_back(rhs.x);
            node.push_back(rhs.y);
            node.push_back(rhs.z);
            node.push_back(rhs.w);

            node.SetStyle(EmitterStyle::Flow);

            return node;
        }

        static bool decode(const Node& node, glm::vec4& rhs)
        {
            if (!node.IsSequence() || node.size() != 4)
            {
                return false;
            }

            rhs.x = node[0].as<float>();
            rhs.y = node[1].as<float>();
            rhs.z = node[2].as<float>();
            rhs.w = node[3].as<float>();

            return true;
        }
    };
    
    /// <summary>
    /// quat 转换
    /// </summary>
    template<>
    struct convert<glm::quat>
    {
        static Node encode(const glm::quat& rhs)
        {
            Node node;
            
            node.push_back(rhs.x);
            node.push_back(rhs.y);
            node.push_back(rhs.z);
            node.push_back(rhs.w);
            
            node.SetStyle(EmitterStyle::Flow);
            return node;
        }
        
        static bool decode(const Node& node, glm::quat& rhs)
        {
            if (!node.IsSequence() || node.size() != 4)
            {
                return false;
            }
            
            rhs.x = node[0].as<float>();
            rhs.y = node[1].as<float>();
            rhs.z = node[2].as<float>();
            rhs.w = node[3].as<float>();
            
            return true;
        }
    };

    /// <summary>
    /// UUID 转换
    /// </summary>
    template<>
    struct convert<Lucky::UUID>
    {
        static Node encode(const Lucky::UUID& uuid)
        {
            Node node;
            node.push_back(static_cast<uint64_t>(uuid));

            return node;
        }

        static bool decode(const Node& node, Lucky::UUID& uuid)
        {
            uuid = node.as<uint64_t>();

            return true;
        }
    };
}

namespace Lucky
{
    inline YAML::Emitter& operator<<(YAML::Emitter& out, const glm::vec2& v)
    {
        out << YAML::Flow;    // 流 [x,y]
        out << YAML::BeginSeq << v.x << v.y << YAML::EndSeq;

        return out;
    }

    inline YAML::Emitter& operator<<(YAML::Emitter& out, const glm::vec3& v)
    {
        out << YAML::Flow;
        out << YAML::BeginSeq << v.x << v.y << v.z << YAML::EndSeq;

        return out;
    }

    inline YAML::Emitter& operator<<(YAML::Emitter& out, const glm::vec4& v)
    {
        out << YAML::Flow;
        out << YAML::BeginSeq << v.x << v.y << v.z << v.w << YAML::EndSeq;

        return out;
    }
    
    inline YAML::Emitter& operator<<(YAML::Emitter& out, const glm::quat& q)
    {
        out << YAML::Flow;
        out << YAML::BeginSeq << q.x << q.y << q.z << q.w << YAML::EndSeq;

        return out;
    };
}