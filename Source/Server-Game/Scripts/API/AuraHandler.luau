--!native

type AuraOnApplyCallback = ((eventID : AuraEvent, data : AuraEventDataOnApply) -> ())
type AuraOnRemoveCallback = ((eventID : AuraEvent, data : AuraEventDataOnRemove) -> ())
type AuraOnHandleEffectCallback = ((eventID : AuraEvent, data : AuraEventDataOnHandleEffect) -> boolean)

type AuraEventInfo =
{
    id : number,
    onApplyCallback : AuraOnApplyCallback?,
    onRemoveCallback : AuraOnRemoveCallback?,
    effectIndexToCallback : { [number] : AuraOnHandleEffectCallback }
}

type AuraHandlerAPI = {
    idToEventInfo : { [number] : AuraEventInfo },
    effectTypeToCallback : { [number] : AuraOnHandleEffectCallback },

    GetEventInfo : (self : AuraHandlerAPI, spellID : number) -> (AuraEventInfo),

    RegisterGenericEffectTypeCallback : (self : AuraHandlerAPI, effectType : number, callback : AuraOnHandleEffectCallback?) -> (),
    RegisterOnApplyCallback : (self : AuraHandlerAPI, spellID : number, callback : AuraOnApplyCallback?) -> (),
    RegisterOnRemoveCallback : (self : AuraHandlerAPI, spellID : number, callback : AuraOnRemoveCallback?) -> (),
    RegisterOnHandleEffectCallback : (self : AuraHandlerAPI, spellID : number, effectIndex : number, callback : AuraOnHandleEffectCallback?) -> ()
}

local effectHandler : AuraHandlerAPI =
{
    idToEventInfo = {},
    effectTypeToCallback = {}
}
effectHandler.__index = effectHandler

function effectHandler:GetEventInfo(spellID : number) : AuraEventInfo
    local eventInfo = self.idToEventInfo[spellID]
    if not eventInfo then
        eventInfo =
        {
            id = spellID,
            onApplyCallback = nil,
            onRemoveCallback = nil,
            effectIndexToCallback = {}
        }

        self.idToEventInfo[spellID] = eventInfo
    end

    return eventInfo
end

function effectHandler:RegisterGenericEffectTypeCallback(effectType : number, callback : AuraOnHandleEffectCallback?)
    self.effectTypeToCallback[effectType] = callback
end

function effectHandler:RegisterOnApplyCallback(spellID : number, callback : AuraOnApplyCallback?)
    local eventInfo = self:GetEventInfo(spellID)
    eventInfo.onApplyCallback = callback
end

function effectHandler:RegisterOnRemoveCallback(spellID : number, callback : AuraOnRemoveCallback?)
    local eventInfo = self:GetEventInfo(spellID)
    eventInfo.onRemoveCallback = callback
end

function effectHandler:RegisterOnHandleEffectCallback(spellID : number, effectIndex : number, callback : AuraOnHandleEffectCallback?)
    local eventInfo = self:GetEventInfo(spellID)
    eventInfo.effectIndexToCallback[effectIndex] = callback
end

local function OnAuraApply(eventID : AuraEvent, data : AuraEventDataOnApply)
    local eventInfo = effectHandler:GetEventInfo(data.aura:GetID())
    if eventInfo.onApplyCallback then
        local result : boolean = eventInfo.onApplyCallback(eventID, data) or false
        return result
    end

    return true
end

local function OnAuraRemove(eventID : AuraEvent, data : AuraEventDataOnRemove)
    local eventInfo = effectHandler:GetEventInfo(data.aura:GetID())
    if eventInfo.onRemoveCallback then
        eventInfo.onRemoveCallback(eventID, data)
    end
end

local function OnAuraHandleEffect(eventID : AuraEvent, data : AuraEventDataOnHandleEffect)
    local spellID = data.aura:GetID()
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

RegisterEvent(AuraEvent.OnApply, OnAuraApply)
RegisterEvent(AuraEvent.OnRemove, OnAuraRemove)
RegisterEvent(AuraEvent.OnHandleEffect, OnAuraHandleEffect)

__global_effectHandler = effectHandler
return __global_effectHandler