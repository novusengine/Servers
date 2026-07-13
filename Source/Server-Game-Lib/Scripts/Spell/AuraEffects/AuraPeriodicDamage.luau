if (IsStateGlobal()) then return end

local auraHandler = require("@src/API/AuraHandler")

local function HandleEffect(eventID : AuraEvent, data : AuraEventDataOnHandleEffect)
    local stacks = data.aura:GetStacks()
    local _, minDamage, maxDamage = data.effect:GetValues()
    --local damageSchool, _, _ = data.effect:GetMiscValues()

    local damage = math.random(minDamage, maxDamage) * stacks
    data.aura:Damage(damage)
end

auraHandler:RegisterGenericEffectTypeCallback(SpellEffectTypeEnum.AuraPeriodicDamage, HandleEffect)