#include "UpdateScripts.h"

#include "Server-Game/Scripting/LuaManager.h"
#include "Server-Game/Util/ServiceLocator.h"

#include <entt/entt.hpp>
#include <tracy/Tracy.hpp>

namespace ECS::Systems
{
    void UpdateScripts::Init(entt::registry& registry)
    {
    }

    void UpdateScripts::Update(entt::registry& registry, f32 deltaTime)
    {
        ZoneScopedN("ECS::UpdateScripts");

        Scripting::LuaManager* luaManager = ServiceLocator::GetLuaManager();
        luaManager->Update(deltaTime);
    }
}