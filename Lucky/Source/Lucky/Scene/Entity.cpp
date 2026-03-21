#include "lcpch.h"
#include "Entity.h"

namespace Lucky
{
	Entity::Entity(entt::entity entityID, Scene* scene)
		: m_EntityID(entityID), m_Scene(scene)
	{

	}
}