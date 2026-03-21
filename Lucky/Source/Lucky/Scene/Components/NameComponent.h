#pragma once

#include <string>

namespace Lucky
{
    struct NameComponent
    {
        std::string Name;
        
        NameComponent() = default;
        NameComponent(const NameComponent& other) = default;
        NameComponent(const std::string& name)
            : Name(name) {}

        operator std::string& () { return Name; }
        operator const std::string& () const { return Name; }
    };
}
