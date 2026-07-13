#include "GlobalHandler.h"
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
    void GlobalHandler::Register(Zenith* zenith)
    {
        // Register Functions
        {
            LuaMethodTable::Set(zenith, GlobalHandlerGlobalMethods);
        }

        zenith->CreateTable("Engine");
        zenith->AddTableField("Name", "NovusEngine");
        zenith->AddTableField("Version", vec3(0.0f, 0.0f, 1.0f));
        zenith->Pop();

        zenith->AddGlobalField("STATE_GLOBAL_ID", std::numeric_limits<u16>().max());
    }

    void GlobalHandler::PostLoad(Zenith* zenith)
    {
        const char* motd = CVarSystem::Get()->GetStringCVar(CVarCategory::Client, "scriptingMotd");

        zenith->CallEvent(MetaGen::Server::Lua::ServerEvent::Loaded, MetaGen::Server::Lua::ServerEventDataLoaded{
            .motd = motd
        });
    }

    i32 GlobalHandler::GetStateMapID(Zenith* zenith)
    {
        zenith->Push(zenith->key.GetMapID());
        return 1;
    }
    i32 GlobalHandler::GetStateInstanceID(Zenith* zenith)
    {
        zenith->Push(zenith->key.GetInstanceID());
        return 1;
    }
    i32 GlobalHandler::GetStateVariantID(Zenith* zenith)
    {
        zenith->Push(zenith->key.GetVariantID());
        return 1;
    }
    i32 GlobalHandler::GetStateIDs(Zenith* zenith)
    {
        zenith->Push(zenith->key.GetMapID());
        zenith->Push(zenith->key.GetInstanceID());
        zenith->Push(zenith->key.GetVariantID());
        return 3;
    }
    i32 GlobalHandler::IsStateGlobal(Zenith* zenith)
    {
        zenith->Push(zenith->key.IsGlobal());
        return 1;
    }

    i32 GlobalHandler::RegisterCreatureAIScript(Zenith* zenith)
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
