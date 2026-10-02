#pragma once

#include <type_traits>
#include <xhash>

namespace Lucky
{
    /// <summary>
    /// 虚拟唯一标识
    /// </summary>
    class UUID
    {
    private:
        uint64_t m_UUID;
    public:
        UUID();
        UUID(uint64_t uuid);
        UUID(const UUID&) = default;

        operator uint64_t() const { return m_UUID; }
    };

    // mono icall 按值传参依赖 trivially copyable，退化后 MSVC 会改传指针
    static_assert(std::is_trivially_copyable_v<UUID>,
        "UUID must stay trivially copyable: mono icalls pass it by value in a register");
}

namespace std
{
    /// <summary>
    /// UUID 类型哈希
    /// </summary>
    template<>
    struct hash<Lucky::UUID>
    {
        std::size_t operator()(const Lucky::UUID& uuid) const
        {
            return hash<uint64_t>()(uuid);
        }
    };
}