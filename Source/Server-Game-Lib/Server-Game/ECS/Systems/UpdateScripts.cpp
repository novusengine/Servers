#include "UpdateScripts.h"

#include "Server-Game/ECS/Singletons/TimeState.h"
#include "Server-Game/Util/ServiceLocator.h"

#include <Scripting/LuaManager.h>

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
        entt::registry::context& ctx = registry.ctx();
        auto& timeState = ctx.get<Singletons::TimeState>();

        Scripting::LuaManager* luaManager = ServiceLocator::GetLuaManager();
        luaManager->Update(deltaTime, timeState.tickCount);
    }
}