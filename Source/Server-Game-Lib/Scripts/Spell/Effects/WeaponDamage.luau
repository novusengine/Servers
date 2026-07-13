if (IsStateGlobal()) then return end

local spellHandler = require("@src/API/SpellHandler")

local function HandleEffect(eventID : SpellEvent, data : SpellEventDataOnHandleEffect)
    local _, weaponSlot, _ = data.effect:GetValues()
    local minDamage, maxDamage, _ = data.effect:GetMiscValues()

    local character = data.caster:ToCharacter()
    if character then
        local weaponEquipmentSlot = if weaponSlot == 1 then ItemEquipSlotEnum.MainHand else ItemEquipSlotEnum.OffHand
        local weapon = character:GetEquippedItem(weaponEquipmentSlot)
        local itemTemplate = weapon and weapon:GetTemplate()
        local itemWeaponTemplate = itemTemplate and itemTemplate:GetWeaponTemplate()

        if itemWeaponTemplate then
            local weaponMinDamage, weaponMaxDamage = itemWeaponTemplate:GetDamageRange()
            minDamage += weaponMinDamage
            maxDamage += weaponMaxDamage
        end
    end

    local attackPower = data.caster:GetStat(StatTypeEnum.AttackPower)
    minDamage += attackPower
    maxDamage += attackPower

    local damage = math.random(minDamage, maxDamage)
    data.spell:Damage(damage)
end
spellHandler:RegisterGenericEffectTypeCallback(SpellEffectTypeEnum.WeaponDamage, HandleEffect)