#include "GlobalHandler.h"

#include <Base/CVarSystem/CVarSystem.h>

#include <Meta/Generated/Server/LuaEvent.h>

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

        zenith->CallEvent(Generated::LuaServerEventEnum::Loaded, Generated::LuaServerEventDataLoaded{
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
}
