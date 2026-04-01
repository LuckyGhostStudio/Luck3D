#pragma once

#define GLM_ENABLE_EXPERIMENTAL
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtx/norm.hpp>
#include <glm/gtx/quaternion.hpp>

#include "Lucky/Math/Math.h"

namespace Lucky
{
    struct TransformComponent
    {
        glm::vec3 Translation = { 0.0f, 0.0f, 0.0f };
        glm::vec3 Scale = { 1.0f, 1.0f, 1.0f };
    private:
        // 这些属性设为私有是为了强制通过 SetRotation() 或 SetRotationEuler() 方法进行设置
        // 避免出现只设置了其中一个而忘记设置另一个的情况
        //
        // 为什么需要同时用四元数和欧拉角表示旋转
        // 因为欧拉角存在万向节死锁问题 -> 旋转应该用四元数存储
        //
        // 但：四元数难以理解，人类更习惯使用欧拉角
        // 不能仅存储四元数并在欧拉角之间转换，因为欧拉角->四元数->欧拉角的转换不具备不变性
        //
        // 有时还需要支持超过360度的旋转存储
        // 而四元数无法实现这一点
        //
        // 因此，我们将欧拉角用于人类操作的"编辑器"相关功能
        // 其他情况则使用四元数. 通过 SetRotation() 方法保持两者同步
        glm::vec3 RotationEuler = { 0.0f, 0.0f, 0.0f };
        glm::quat Rotation = { 1.0f, 0.0f, 0.0f, 0.0f };

    public:
        TransformComponent() = default;
        TransformComponent(const TransformComponent& other) = default;
        TransformComponent(const glm::vec3& translation)
            : Translation(translation)
        {
        }

        glm::mat4 GetTransform() const
        {
            return glm::translate(glm::mat4(1.0f), Translation)
                * glm::toMat4(Rotation)
                * glm::scale(glm::mat4(1.0f), Scale);
        }

        void SetTransform(const glm::mat4& transform)
        {
            Math::DecomposeTransform(transform, Translation, Rotation, Scale);
            RotationEuler = glm::eulerAngles(Rotation);
        }

        glm::vec3 GetRotationEuler() const
        {
            return RotationEuler;
        }

        void SetRotationEuler(const glm::vec3& euler)
        {
            RotationEuler = euler;
            Rotation = glm::quat(RotationEuler);
        }

        glm::quat GetRotation() const
        {
            return Rotation;
        }

        void SetRotation(const glm::quat& quat)
        {
            // 将给定的欧拉角范围限制在 [-pi, pi]
            auto wrapToPi = [](glm::vec3 v)
            {
                return glm::mod(v + glm::pi<float>(), 2.0f * glm::pi<float>()) - glm::pi<float>();
            };

            auto originalEuler = RotationEuler;
            Rotation = quat;
            RotationEuler = glm::eulerAngles(Rotation);

            // 一个四元数可以对应多种欧拉角（理论上无限多种），而 glm::eulerAngles() 只能给出其中一种，这可能不是我们想要的那个.
            // 这里我们查看一些可能的替代方案，并选择最接近原始欧拉角的那个.
            // 这是为了避免在调用 SetRotation(quat) 时，欧拉角突然出现180度的翻转.

            glm::vec3 alternate1 = { RotationEuler.x - glm::pi<float>(), glm::pi<float>() - RotationEuler.y, RotationEuler.z - glm::pi<float>() };
            glm::vec3 alternate2 = { RotationEuler.x + glm::pi<float>(), glm::pi<float>() - RotationEuler.y, RotationEuler.z - glm::pi<float>() };
            glm::vec3 alternate3 = { RotationEuler.x + glm::pi<float>(), glm::pi<float>() - RotationEuler.y, RotationEuler.z + glm::pi<float>() };
            glm::vec3 alternate4 = { RotationEuler.x - glm::pi<float>(), glm::pi<float>() - RotationEuler.y, RotationEuler.z + glm::pi<float>() };

            // 选择最接近原始值的替代方案.
            float distance0 = glm::length2(wrapToPi(RotationEuler - originalEuler));
            float distance1 = glm::length2(wrapToPi(alternate1 - originalEuler));
            float distance2 = glm::length2(wrapToPi(alternate2 - originalEuler));
            float distance3 = glm::length2(wrapToPi(alternate3 - originalEuler));
            float distance4 = glm::length2(wrapToPi(alternate4 - originalEuler));

            float best = distance0;
            if (distance1 < best)
            {
                best = distance1;
                RotationEuler = alternate1;
            }
            if (distance2 < best)
            {
                best = distance2;
                RotationEuler = alternate2;
            }
            if (distance3 < best)
            {
                best = distance3;
                RotationEuler = alternate3;
            }
            if (distance4 < best)
            {
                best = distance4;
                RotationEuler = alternate4;
            }

            RotationEuler = wrapToPi(RotationEuler);
        }
        
        /// <summary>
        /// 获取前方向量（局部 +Z 轴经旋转后的方向）
        /// </summary>
        glm::vec3 GetForward() const
        {
            return Rotation * glm::vec3(0.0f, 0.0f, 1.0f);
        }

        /// <summary>
        /// 获取上方向量（局部 +Y 轴经旋转后的方向）
        /// </summary>
        glm::vec3 GetUp() const
        {
            return Rotation * glm::vec3(0.0f, 1.0f, 0.0f);
        }

        /// <summary>
        /// 获取右方向量（局部 +X 轴经旋转后的方向）
        /// </summary>
        glm::vec3 GetRight() const
        {
            return Rotation * glm::vec3(1.0f, 0.0f, 0.0f);
        }
    };
}
