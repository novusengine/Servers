local Type = require("Type")
local D = require("Definition")

return D.Definitions
{
    D.LuaEnum("LuaHandlerTypeEnum", Type.U16,
    {
        D.Field("Global"),
        D.Field("Event"),
        D.Field("Message"),
        D.Field("Unit"),
        D.Field("Character"),
        D.Field("Spell"),
        D.Field("Item"),
        D.Field("Creature"),
        D.Field("Count")
    }),

    D.LuaEnum("ServerEvent", Type.U8,
    {
        D.Field("Invalid"),
        D.Field("Loaded"),
        D.Field("Updated"),
        D.Field("Count")
    }),

    D.LuaEnum("CharacterEvent", Type.U8,
    {
        D.Field("Invalid"),
        D.Field("OnLogin"),
        D.Field("OnLogout"),
        D.Field("OnWorldChanged"),
        D.Field("Count")
    }),

    D.LuaEnum("TriggerEvent", Type.U8,
    {
        D.Field("Invalid"),
        D.Field("OnEnter"),
        D.Field("OnExit"),
        D.Field("OnStay"),
        D.Field("Count")
    }),

    D.LuaEnum("SpellEvent", Type.U8,
    {
        D.Field("Invalid"),
        D.Field("OnPrepare"),
        D.Field("OnHandleEffect"),
        D.Field("OnFinish"),
        D.Field("Count")
    }),

    D.LuaEnum("AuraEvent", Type.U8,
    {
        D.Field("Invalid"),
        D.Field("OnApply"),
        D.Field("OnRemove"),
        D.Field("OnHandleEffect"),
        D.Field("Count")
    }),

    D.LuaEnum("CreatureAIEvent", Type.U8,
    {
        D.Field("Invalid"),
        D.Field("OnInit"),
        D.Field("OnDeinit"),
        D.Field("OnEnterCombat"),
        D.Field("OnLeaveCombat"),
        D.Field("OnUpdate"),
        D.Field("OnResurrect"),
        D.Field("OnDied"),
        D.Field("Count")
    })
}
