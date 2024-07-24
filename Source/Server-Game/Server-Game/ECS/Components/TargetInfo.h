#pragma once
#include <Base/Types.h>

#include <entt/fwd.hpp>

namespace ECS::Components
{
	struct TargetInfo
	{
	public:
		entt::entity target;
	};
}