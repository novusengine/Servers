if (IsStateGlobal()) then return end

local spellHandler = require("@src/API/SpellHandler")

local function HandleEffect(eventID : SpellEvent, data : SpellEventDataOnHandleEffect)
    local spellID, stacks = data.effect:GetValues()
    if spellID == -1 then
        spellID = data.spell:GetID()
    end

    local target = data.spell:GetTarget() or data.caster
    data.caster:AddAura(target, spellID, stacks)
end
spellHandler:RegisterGenericEffectTypeCallback(SpellEffectTypeEnum.AuraApply, HandleEffect)