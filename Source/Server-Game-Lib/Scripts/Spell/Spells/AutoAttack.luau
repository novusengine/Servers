if (IsStateGlobal()) then return end

local spellHandler = require("@src/API/SpellHandler")

local spellInfo =
{
    primaryAttackID = 1,
    secondaryAttackID = 2,
}

local function OnPrepare(eventID : SpellEvent, data : SpellEventDataOnPrepare)
    local effect1 = data.spell:GetEffect(1)
    if not effect1 then
        return false
    end

    local _, weaponSlot, _ = effect1:GetValues()
    local cooldown = 1.0

    local character = data.caster:ToCharacter()
    if character then
        local weaponEquipmentSlot = if weaponSlot == 1 then ItemEquipSlotEnum.MainHand else ItemEquipSlotEnum.OffHand
        local weapon = character:GetEquippedItem(weaponEquipmentSlot)
        local itemTemplate = weapon and weapon:GetTemplate()
        local itemWeaponTemplate = itemTemplate and itemTemplate:GetWeaponTemplate()

        if itemWeaponTemplate then
            cooldown = itemWeaponTemplate:GetSpeed()
        end
    end

    data.caster:SetSpellCooldown(data.spell:GetID(), cooldown)
    return true
end

spellHandler:RegisterOnPrepareCallback(spellInfo.primaryAttackID, OnPrepare)
spellHandler:RegisterOnPrepareCallback(spellInfo.secondaryAttackID, OnPrepare)