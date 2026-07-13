local Type = require("Type")
local D = require("Definition")

return D.Definitions
{
    D.LuaEvent("ServerEventDataLoaded",
    {
        D.Field("motd", Type.STRING)
    }),

    D.LuaEvent("ServerEventDataUpdated",
    {
        D.Field("deltaTime", Type.F32)
    }),

    D.LuaEvent("CharacterEventDataOnLogin",
    {
        D.Field("characterEntity", Type.U32)
    }),

    D.LuaEvent("CharacterEventDataOnLogout",
    {
        D.Field("characterEntity", Type.U32)
    }),

    D.LuaEvent("CharacterEventDataOnWorldChanged",
    {
        D.Field("characterEntity", Type.U32)
    }),

    D.LuaEvent("TriggerEventDataOnEnter",
    {
        D.Field("triggerID", Type.U32),
        D.Field("playerID", Type.U32)
    }),

    D.LuaEvent("TriggerEventDataOnExit",
    {
        D.Field("triggerID", Type.U32),
        D.Field("playerID", Type.U32)
    }),

    D.LuaEvent("TriggerEventDataOnStay",
    {
        D.Field("triggerID", Type.U32),
        D.Field("playerID", Type.U32)
    }),

    D.LuaEvent("SpellEventDataOnPrepare",
    {
        D.Field("casterID", Type.U32),
        D.Field("spellEntity", Type.U32),
        D.Field("spellID", Type.U32)
    }),

    D.LuaEvent("SpellEventDataOnHandleEffect",
    {
        D.Field("casterID", Type.U32),
        D.Field("spellEntity", Type.U32),
        D.Field("spellID", Type.U32),
        D.Field("procID", Type.U32),
        D.Field("effectIndex", Type.U8),
        D.Field("effectType", Type.U8)
    }),

    D.LuaEvent("SpellEventDataOnFinish",
    {
        D.Field("casterID", Type.U32),
        D.Field("spellEntity", Type.U32),
        D.Field("spellID", Type.U32)
    }),

    D.LuaEvent("AuraEventDataOnApply",
    {
        D.Field("casterID", Type.U32),
        D.Field("targetID", Type.U32),
        D.Field("auraEntity", Type.U32),
        D.Field("spellID", Type.U32)
    }),

    D.LuaEvent("AuraEventDataOnRemove",
    {
        D.Field("casterID", Type.U32),
        D.Field("targetID", Type.U32),
        D.Field("auraEntity", Type.U32),
        D.Field("spellID", Type.U32)
    }),

    D.LuaEvent("AuraEventDataOnHandleEffect",
    {
        D.Field("casterID", Type.U32),
        D.Field("targetID", Type.U32),
        D.Field("auraEntity", Type.U32),
        D.Field("spellID", Type.U32),
        D.Field("procID", Type.U32),
        D.Field("effectIndex", Type.U8),
        D.Field("effectType", Type.U8)
    }),

    D.LuaEvent("CreatureAIEventDataOnInit",
    {
        D.Field("creatureEntity", Type.U32),
        D.Field("scriptNameHash", Type.U32)
    }),

    D.LuaEvent("CreatureAIEventDataOnDeinit",
    {
        D.Field("creatureEntity", Type.U32)
    }),

    D.LuaEvent("CreatureAIEventDataOnEnterCombat",
    {
        D.Field("creatureEntity", Type.U32)
    }),

    D.LuaEvent("CreatureAIEventDataOnLeaveCombat",
    {
        D.Field("creatureEntity", Type.U32)
    }),

    D.LuaEvent("CreatureAIEventDataOnUpdate",
    {
        D.Field("creatureEntity", Type.U32),
        D.Field("deltaTime", Type.F32)
    }),

    D.LuaEvent("CreatureAIEventDataOnResurrect",
    {
        D.Field("creatureEntity", Type.U32),
        D.Field("resurrectorEntity", Type.U32)
    }),

    D.LuaEvent("CreatureAIEventDataOnDied",
    {
        D.Field("creatureEntity", Type.U32),
        D.Field("killerEntity", Type.U32)
    })
}
