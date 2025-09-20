--!native

type CreatureAIOnInitCallback = ((unit : Unit) -> ())
type CreatureAIOnDeinitCallback = ((unit : Unit) -> ())
type CreatureAIOnEnterCombatCallback = ((unit : Unit) -> ())
type CreatureAIOnLeaveCombatCallback = ((unit : Unit) -> ())
type CreatureAIOnUpdateCallback = ((unit : Unit, deltaTime : number) -> ())
type CreatureAIOnResurrectCallback = ((unit : Unit, resurrector : Unit?) -> ())
type CreatureAIOnDiedCallback = ((unit : Unit, killer : Unit?) -> ())

type CreatureAIScriptInfo =
{
    scriptNameHash : number,
    onInitCallback : CreatureAIOnInitCallback?,
    onDeinitCallback : CreatureAIOnDeinitCallback?,
    onEnterCombatCallback : CreatureAIOnEnterCombatCallback?,
    onLeaveCombatCallback : CreatureAIOnLeaveCombatCallback?,
    onUpdateCallback : CreatureAIOnUpdateCallback?,
    onResurrectCallback : CreatureAIOnResurrectCallback?,
    onDiedCallback : CreatureAIOnDiedCallback?
}

type CreatureAIHandlerAPI = {
    idToScriptNameHash : { [number] : number },
    scriptNameHashToInfo : { [number] : CreatureAIScriptInfo },

    GetScriptNameHash : (self : CreatureAIHandlerAPI, creatureID : number) -> number,
    GetScriptInfo : (self : CreatureAIHandlerAPI, scriptNameHash : number) -> (CreatureAIScriptInfo),

    RegisterScriptInfo : (self : CreatureAIHandlerAPI, scriptName : string) -> number,
    RegisterScriptInitCallback : (self : CreatureAIHandlerAPI, scriptNameHash : number, callback : CreatureAIOnInitCallback?) -> (),
    RegisterScriptDeinitCallback : (self : CreatureAIHandlerAPI, scriptNameHash : number, callback : CreatureAIOnDeinitCallback?) -> (),
    RegisterScriptEnterCombatCallback : (self : CreatureAIHandlerAPI, scriptNameHash : number, callback : CreatureAIOnEnterCombatCallback?) -> (),
    RegisterScriptLeaveCombatCallback : (self : CreatureAIHandlerAPI, scriptNameHash : number, callback : CreatureAIOnLeaveCombatCallback?) -> (),
    RegisterScriptUpdateCallback : (self : CreatureAIHandlerAPI, scriptNameHash : number, callback : CreatureAIOnUpdateCallback?) -> (),
    RegisterScriptResurrectCallback : (self : CreatureAIHandlerAPI, scriptNameHash : number, callback : CreatureAIOnResurrectCallback?) -> (),
    RegisterScriptDiedCallback : (self : CreatureAIHandlerAPI, scriptNameHash : number, callback : CreatureAIOnDiedCallback?) -> ()
}

local creatureAIHandler : CreatureAIHandlerAPI =
{
    idToScriptNameHash = {},
    scriptNameHashToInfo = {}
}
creatureAIHandler.__index = creatureAIHandler

function creatureAIHandler:GetScriptInfo(scriptNameHash : number) : CreatureAIScriptInfo
    return self.scriptNameHashToInfo[scriptNameHash]
end

function creatureAIHandler:RegisterScriptInfo(scriptName : string) : ()
    local scriptNameHash = RegisterCreatureAIScript(scriptName)
    if scriptNameHash == nil then
        return nil
    end

    if self.scriptNameHashToInfo[scriptNameHash] then
        error(`Creature AI script already registered: {scriptName} (hash: {scriptNameHash})`)
        return nil
    end

    self.scriptNameHashToInfo[scriptNameHash] = {
        scriptNameHash = scriptNameHash,
        onInitCallback = nil,
        onDeinitCallback = nil,
        onEnterCombatCallback = nil,
        onLeaveCombatCallback = nil,
        onUpdateCallback = nil,
        onResurrectCallback = nil,
        onDiedCallback = nil
    }

    return scriptNameHash
end

function creatureAIHandler:RegisterScriptInitCallback(scriptNameHash : number, callback : CreatureAIOnInitCallback?) : ()
    local scriptInfo = self:GetScriptInfo(scriptNameHash)
    scriptInfo.onInitCallback = callback
end

function creatureAIHandler:RegisterScriptDeinitCallback(scriptNameHash : number, callback : CreatureAIOnDeinitCallback?) : ()
    local scriptInfo = self:GetScriptInfo(scriptNameHash)
    scriptInfo.onDeinitCallback = callback
end

function creatureAIHandler:RegisterScriptEnterCombatCallback(scriptNameHash : number, callback : CreatureAIOnEnterCombatCallback?) : ()
    local scriptInfo = self:GetScriptInfo(scriptNameHash)
    scriptInfo.onEnterCombatCallback = callback
end

function creatureAIHandler:RegisterScriptLeaveCombatCallback(scriptNameHash : number, callback : CreatureAIOnLeaveCombatCallback?) : ()
    local scriptInfo = self:GetScriptInfo(scriptNameHash)
    scriptInfo.onLeaveCombatCallback = callback
end

function creatureAIHandler:RegisterScriptUpdateCallback(scriptNameHash : number, callback : CreatureAIOnUpdateCallback?) : ()
    local scriptInfo = self:GetScriptInfo(scriptNameHash)
    scriptInfo.onUpdateCallback = callback
end

function creatureAIHandler:RegisterScriptResurrectCallback(scriptNameHash : number, callback : CreatureAIOnResurrectCallback?) : ()
    local scriptInfo = self:GetScriptInfo(scriptNameHash)
    scriptInfo.onResurrectCallback = callback
end

function creatureAIHandler:RegisterScriptDiedCallback(scriptNameHash : number, callback : CreatureAIOnDiedCallback?) : ()
    local scriptInfo = self:GetScriptInfo(scriptNameHash)
    scriptInfo.onDiedCallback = callback
end

local function OnCreatureAIInit(eventID : CreatureAIEvent, data : CreatureAIEventDataOnInit) : ()
    local creatureID = data.unit:GetID()

    local scriptNameHash = data.scriptNameHash
    if scriptNameHash == nil then
        return false
    end

    local scriptInfo = creatureAIHandler:GetScriptInfo(scriptNameHash)
    if scriptInfo.onInitCallback then
        scriptInfo.onInitCallback(data.unit)
    end

    creatureAIHandler.idToScriptNameHash[creatureID] = scriptNameHash
    return true
end
local function OnCreatureAIDeinit(eventID : CreatureAIEvent, data : CreatureAIEventDataOnDeinit) : ()
    local creatureID = data.unit:GetID()

    local scriptNameHash = creatureAIHandler.idToScriptNameHash[creatureID]
    if scriptNameHash == nil then
        return
    end
    
    local scriptInfo = creatureAIHandler:GetScriptInfo(scriptNameHash)
    if scriptInfo.onDeinitCallback then
        scriptInfo.onDeinitCallback(data.unit)
    end

    creatureAIHandler.idToScriptNameHash[creatureID] = nil
end
local function OnCreatureAIEnterCombat(eventID : CreatureAIEvent, data : CreatureAIEventDataOnEnterCombat) : ()
    local creatureID = data.unit:GetID()

    local scriptNameHash = creatureAIHandler.idToScriptNameHash[creatureID]
    if scriptNameHash == nil then
        return
    end
    
    local scriptInfo = creatureAIHandler:GetScriptInfo(scriptNameHash)
    if scriptInfo.onEnterCombatCallback then
        scriptInfo.onEnterCombatCallback(data.unit)
    end
end
local function OnCreatureAILeaveCombat(eventID : CreatureAIEvent, data : CreatureAIEventDataOnLeaveCombat) : ()
    local creatureID = data.unit:GetID()

    local scriptNameHash = creatureAIHandler.idToScriptNameHash[creatureID]
    if scriptNameHash == nil then
        return
    end
    
    local scriptInfo = creatureAIHandler:GetScriptInfo(scriptNameHash)
    if scriptInfo.onLeaveCombatCallback then
        scriptInfo.onLeaveCombatCallback(data.unit)
    end
end
local function OnCreatureAIUpdate(eventID : CreatureAIEvent, data : CreatureAIEventDataOnUpdate) : ()
    local creatureID = data.unit:GetID()

    local scriptNameHash = creatureAIHandler.idToScriptNameHash[creatureID]
    if scriptNameHash == nil then
        return
    end
    
    local scriptInfo = creatureAIHandler:GetScriptInfo(scriptNameHash)
    if scriptInfo.onUpdateCallback then
        scriptInfo.onUpdateCallback(data.unit, data.deltaTime)
    end
end
local function OnCreatureAIResurrect(eventID : CreatureAIEvent, data : CreatureAIEventDataOnResurrect) : ()
    local creatureID = data.unit:GetID()

    local scriptNameHash = creatureAIHandler.idToScriptNameHash[creatureID]
    if scriptNameHash == nil then
        return
    end
    
    local scriptInfo = creatureAIHandler:GetScriptInfo(scriptNameHash)
    if scriptInfo.onResurrectCallback then
        scriptInfo.onResurrectCallback(data.unit, data.resurrector)
    end
end
local function OnCreatureAIDied(eventID : CreatureAIEvent, data : CreatureAIEventDataOnDied) : ()
    local creatureID = data.unit:GetID()

    local scriptNameHash = creatureAIHandler.idToScriptNameHash[creatureID]
    if scriptNameHash == nil then
        return
    end
    
    local scriptInfo = creatureAIHandler:GetScriptInfo(scriptNameHash)
    if scriptInfo.onDiedCallback then
        scriptInfo.onDiedCallback(data.unit, data.killer)
    end
end
RegisterEvent(CreatureAIEvent.OnInit, OnCreatureAIInit)
RegisterEvent(CreatureAIEvent.OnDeinit, OnCreatureAIDeinit)
RegisterEvent(CreatureAIEvent.OnEnterCombat, OnCreatureAIEnterCombat)
RegisterEvent(CreatureAIEvent.OnLeaveCombat, OnCreatureAILeaveCombat)
RegisterEvent(CreatureAIEvent.OnUpdate, OnCreatureAIUpdate)
RegisterEvent(CreatureAIEvent.OnResurrect, OnCreatureAIResurrect)
RegisterEvent(CreatureAIEvent.OnDied, OnCreatureAIDied)

__global_creatureAIHandler = creatureAIHandler
return __global_creatureAIHandler