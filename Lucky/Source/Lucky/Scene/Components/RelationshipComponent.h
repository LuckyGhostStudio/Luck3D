#pragma once

#include "Lucky/Core/UUID.h"

namespace Lucky
{
    struct RelationshipComponent
    {
        UUID Parent = 0;
        std::vector<UUID> Children;

        RelationshipComponent() = default;
        RelationshipComponent(const RelationshipComponent& other) = default;
        RelationshipComponent(UUID parent)
            : Parent(parent) {}
    };
}
