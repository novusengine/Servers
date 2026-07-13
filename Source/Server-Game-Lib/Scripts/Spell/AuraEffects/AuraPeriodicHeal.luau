if (IsStateGlobal()) then return end

local auraHandler = require("@src/API/AuraHandler")

local function HandleEffect(eventID : AuraEvent, data : AuraEventDataOnHandleEffect)
    local stacks = data.aura:GetStacks()
    local _, minHeal, maxHeal = data.effect:GetValues()
    --local damageSchool, _, _ = data.effect:GetMiscValues()

    local heal = math.random(minHeal, maxHeal) * stacks
    data.aura:Heal(heal)
end
auraHandler:RegisterGenericEffectTypeCallback(SpellEffectTypeEnum.AuraPeriodicHeal, HandleEffect)