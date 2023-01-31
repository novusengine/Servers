#pragma once
#include <Base/Types.h>

namespace ECS::Components
{
	struct Transform
	{
	public:
		vec3 position = vec3(0.0f, 0.0f, 0.0f);
		quat rotation = quat(0.0f, 0.0f, 0.0f, 1.0f);
		vec3 scale = vec3(1.0f, 1.0f, 1.0f);
	};
}