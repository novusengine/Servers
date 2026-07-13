#include "CreatureHandler.h"
#include "Server-Game/Application/EnttRegistries.h"
#include "Server-Game/ECS/Singletons/CreatureAIState.h"
#include "Server-Game/ECS/Singletons/WorldState.h"
#include "Server-Game/Util/ServiceLocator.h"

#include <Base/CVarSystem/CVarSystem.h>
#include <Base/Util/StringUtils.h>

#include <MetaGen/EnumTraits.h>
#include <MetaGen/Server/Lua/Lua.h>

#include <Scripting/LuaManager.h>

namespace Scripting
{
    void CreatureHandler::Register(Zenith* zenith)
    {
        // Register Functions
        {
            LuaMethodTable::Set(zenith, CreatureHandlersGlobalMethods);
        }
    }

    void CreatureHandler::Clear(Zenith* zenith)
    {
        if (zenith->key.IsGlobal())
            return;

        entt::registry* registry = ServiceLocator::GetEnttRegistries()->gameRegistry;
        auto& worldState = registry->ctx().get<ECS::Singletons::WorldState>();

        u16 mapID = zenith->key.GetMapID();
        ECS::World& world = worldState.GetWorld(mapID);

        auto& creatureAIState = world.GetSingleton<ECS::Singletons::CreatureAIState>();
        creatureAIState.loadedScriptNameHashes.clear();
    }

    void CreatureHandler::PostLoad(Zenith* zenith)
    {
        if (zenith->key.IsGlobal())
            return;

        entt::registry* registry = ServiceLocator::GetEnttRegistries()->gameRegistry;
        auto& worldState = registry->ctx().get<ECS::Singletons::WorldState>();

        u16 mapID = zenith->key.GetMapID();
        ECS::World& world = worldState.GetWorld(mapID);

        auto& creatureAIState = world.GetSingleton<ECS::Singletons::CreatureAIState>();

        // iterator 
        for (auto itr = creatureAIState.creatureGUIDToScriptNameHash.begin(); itr != creatureAIState.creatureGUIDToScriptNameHash.end();)
        {
            const auto& [guid, scriptNameHash] = *itr;

            entt::entity entity = world.GetEntity(guid);
            if (entity == entt::null)
                continue;

            if (!zenith->CallEvent(MetaGen::Server::Lua::CreatureAIEvent::OnInit, MetaGen::Server::Lua::CreatureAIEventDataOnInit{
                .creatureEntity = entt::to_integral(entity),
                .scriptNameHash = scriptNameHash
                }))
            {
                itr = creatureAIState.creatureGUIDToScriptNameHash.erase(itr);
            }

            itr++;
        }
    }

    i32 CreatureHandler::RegisterCreatureAIScript(Zenith* zenith)
    {
        const char* scriptName = zenith->CheckVal<const char*>(1);
        u32 scriptNameLength = static_cast<u32>(strlen(scriptName));

        if (!scriptName || scriptNameLength == 0)
            return 0;

        entt::registry* registry = ServiceLocator::GetEnttRegistries()->gameRegistry;
        auto& worldState = registry->ctx().get<ECS::Singletons::WorldState>();

        u16 mapID = zenith->key.GetMapID();
        ECS::World& world = worldState.GetWorld(mapID);
        auto& creatureAIState = world.GetSingleton<ECS::Singletons::CreatureAIState>();

        u32 scriptNameHash = StringUtils::fnv1a_32(scriptName, scriptNameLength);
        if (creatureAIState.loadedScriptNameHashes.contains(scriptNameHash))
            return 0;

        creatureAIState.loadedScriptNameHashes.insert(scriptNameHash);
        zenith->Push(scriptNameHash);
        return 1;
    }
}
