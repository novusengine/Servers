#pragma once
#include <Base/Types.h>
#include <entt/fwd.hpp>

namespace ECS::Systems
{
	class UpdatePower
	{
	public:
		static void Init(entt::registry& registry);
		static void Update(entt::registry& registry, f32 deltaTime);
	};
}