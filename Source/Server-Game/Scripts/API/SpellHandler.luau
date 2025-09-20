--!native

type SpellOnPrepareCallback = ((eventID : SpellEvent, data : SpellEventDataOnPrepare) -> ())
type SpellOnHandleEffectCallback = ((eventID : SpellEvent, data : SpellEventDataOnHandleEffect) -> boolean)

type SpellEventInfo =
{
    id : number,
    onPrepareCallback : SpellOnPrepareCallback?,
    effectIndexToCallback : { [number] : SpellOnHandleEffectCallback }
}

type SpellHandlerAPI = {
    idToEventInfo : { [number] : SpellEventInfo },
    effectTypeToCallback : { [number] : SpellOnHandleEffectCallback },

    GetEventInfo : (self : SpellHandlerAPI, spellID : number) -> (SpellEventInfo),

    RegisterGenericEffectTypeCallback : (self : SpellHandlerAPI, effectType : number, callback : SpellOnHandleEffectCallback?) -> (),
    RegisterOnPrepareCallback : (self : SpellHandlerAPI, spellID : number, callback : SpellOnPrepareCallback?) -> (),
    RegisterOnHandleEffectCallback : (self : SpellHandlerAPI, spellID : number, effectIndex : number, callback : SpellOnHandleEffectCallback?) -> ()
}

local effectHandler : SpellHandlerAPI =
{
    idToEventInfo = {},
    effectTypeToCallback = {}
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

function effectHandler:RegisterOnPrepareCallback(spellID : number, callback : SpellOnPrepareCallback?)
    local eventInfo = self:GetEventInfo(spellID)
    eventInfo.onPrepareCallback = callback
end

function effectHandler:RegisterOnHandleEffectCallback(spellID : number, effectIndex : number, callback : SpellOnHandleEffectCallback?)
    local eventInfo = self:GetEventInfo(spellID)
    eventInfo.effectIndexToCallback[effectIndex] = callback
end

local function OnSpellPrepare(eventID : SpellEvent, data : SpellEventDataOnPrepare)
    local eventInfo = effectHandler:GetEventInfo(data.spell:GetID())
    if eventInfo.onPrepareCallback then
        local result : boolean = eventInfo.onPrepareCallback(eventID, data) or false
        return result
    end

    return true
end
RegisterEvent(SpellEvent.OnPrepare, OnSpellPrepare)

local function OnSpellHandleEffect(eventID : SpellEvent, data : SpellEventDataOnHandleEffect)
    local spellID = data.spell:GetID()
    local eventInfo = effectHandler:GetEventInfo(spellID)
    local effectIndex = data.effect:GetIndex()

    local callback = eventInfo.effectIndexToCallback[effectIndex]
    if callback then
        if not callback(eventID, data) then
            return
        end
    end

    local effectType = data.effect:GetType()
    local typeCallback = effectHandler.effectTypeToCallback[effectType]
    if not typeCallback then
        return
    end

    typeCallback(eventID, data)
end
RegisterEvent(SpellEvent.OnHandleEffect, OnSpellHandleEffect)

__global_effectHandler = effectHandler
return __global_effectHandler