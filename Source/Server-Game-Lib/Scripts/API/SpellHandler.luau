--!native

type SpellOnPrepareCallback = ((eventID : SpellEvent, data : SpellEventDataOnPrepare) -> ())
type SpellOnHandleEffectCallback = ((eventID : SpellEvent, data : SpellEventDataOnHandleEffect) -> boolean)
type SpellOnFinishCallback = ((eventID : SpellEvent, data : SpellEventDataOnFinish) -> ())

type SpellEventInfo =
{
    id : number,
    onPrepareCallback : SpellOnPrepareCallback?,
    onFinishCallback : SpellOnFinishCallback?,
    effectIndexToCallback : { [number] : SpellOnHandleEffectCallback }
}

type SpellHandlerAPI = {
    idToEventInfo : { [number] : SpellEventInfo },
    effectTypeToCallback : { [number] : SpellOnHandleEffectCallback },
    procDataIDToCallback : { [number] : SpellOnHandleEffectCallback },

    GetEventInfo : (self : SpellHandlerAPI, spellID : number) -> (SpellEventInfo),

    RegisterGenericEffectTypeCallback : (self : SpellHandlerAPI, effectType : number, callback : SpellOnHandleEffectCallback?) -> (),
    RegisterOnProcCallback : (self : SpellHandlerAPI, procDataID : number, callback : SpellOnHandleEffectCallback?) -> (),

    RegisterOnPrepareCallback : (self : SpellHandlerAPI, spellID : number, callback : SpellOnPrepareCallback?) -> (),
    RegisterOnHandleEffectCallback : (self : SpellHandlerAPI, spellID : number, effectIndex : number, callback : SpellOnHandleEffectCallback?) -> (),
    RegisterOnFinishCallback : (self : SpellHandlerAPI, spellID : number, callback : SpellOnFinishCallback?) -> ()
}

local effectHandler : SpellHandlerAPI =
{
    idToEventInfo = {},
    effectTypeToCallback = {},
    procDataIDToCallback = {}
}
effectHandler.__index = effectHandler

function effectHandler:GetEventInfo(spellID : number) : SpellEventInfo
    local eventInfo = self.idToEventInfo[spellID]
    if not eventInfo then
        eventInfo =
        {
            id = spellID,
            onPrepareCallback = nil,
            effectIndexToCallback = {}
        }

        self.idToEventInfo[spellID] = eventInfo
    end

    return eventInfo
end

function effectHandler:RegisterGenericEffectTypeCallback(effectType : number, callback : SpellOnHandleEffectCallback?)
    self.effectTypeToCallback[effectType] = callback
end

function effectHandler:RegisterOnProcCallback(procDataID : number, callback : SpellOnHandleEffectCallback?)
    self.procDataIDToCallback[procDataID] = callback
end

function effectHandler:RegisterOnPrepareCallback(spellID : number, callback : SpellOnPrepareCallback?)
    local eventInfo = self:GetEventInfo(spellID)
    eventInfo.onPrepareCallback = callback
end

function effectHandler:RegisterOnHandleEffectCallback(spellID : number, effectIndex : number, callback : SpellOnHandleEffectCallback?)
    local eventInfo = self:GetEventInfo(spellID)
    eventInfo.effectIndexToCallback[effectIndex] = callback
end

function effectHandler:RegisterOnFinishCallback(spellID : number, callback : SpellOnFinishCallback?)
    local eventInfo = self:GetEventInfo(spellID)
    eventInfo.onFinishCallback = callback
end

local function OnSpellPrepare(eventID : SpellEvent, data : SpellEventDataOnPrepare)
    local eventInfo = effectHandler:GetEventInfo(data.spell:GetID())
    if eventInfo.onPrepareCallback then
        local result : boolean = eventInfo.onPrepareCallback(eventID, data) or false
        return result
    end

    return true
end

local function OnSpellHandleEffect(eventID : SpellEvent, data : SpellEventDataOnHandleEffect)
    local spellID = data.spell:GetID()
    local eventInfo = effectHandler:GetEventInfo(spellID)
    local effectIndex = data.effect:GetIndex()
    
    local procCallback = if data.procID then effectHandler.procDataIDToCallback[data.procID] else nil
    if procCallback and not procCallback(eventID, data) then
        return
    end

    local effectCallback = eventInfo.effectIndexToCallback[effectIndex]
    if effectCallback and not effectCallback(eventID, data) then
        return
    end

    local effectType = data.effect:GetType()
    local genericTypeCallback = effectHandler.effectTypeToCallback[effectType]
    if not genericTypeCallback then
        return
    end

    genericTypeCallback(eventID, data)
end

local function OnSpellFinish(eventID : SpellEvent, data : SpellEventDataOnFinish)
    local eventInfo = effectHandler:GetEventInfo(data.spell:GetID())
    if eventInfo.onFinishCallback then
        eventInfo.onFinishCallback(eventID, data)
    end
end

RegisterEvent(SpellEvent.OnPrepare, OnSpellPrepare)
RegisterEvent(SpellEvent.OnHandleEffect, OnSpellHandleEffect)
RegisterEvent(SpellEvent.OnFinish, OnSpellFinish)

__global_effectHandler = effectHandler
return __global_effectHandler