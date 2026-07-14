local D = require("Definition")
local P = require("PermissionDefinitions")

local PermissionID = P.IDs("permission",
{
    SeeDespawnedCreatures = 1,
    Damage = 2,
    Heal = 3,
    Kill = 4,
    Resurrect = 5,
    UnitMorph = 6,
    UnitDemorph = 7,
    Teleport = 8,
    CharacterAdd = 9,
    CharacterRemove = 10,
    UnitSetRace = 11,
    UnitSetGender = 12,
    UnitSetClass = 13,
    UnitSetLevel = 14,
    ItemSetTemplate = 15,
    ItemSetStatTemplate = 16,
    ItemSetArmorTemplate = 17,
    ItemSetWeaponTemplate = 18,
    ItemSetShieldTemplate = 19,
    ItemAdd = 20,
    ItemRemove = 21,
    CreatureAdd = 22,
    CreatureRemove = 23,
    CreatureInfo = 24,
    MapAdd = 25,
    GotoAdd = 26,
    GotoAddHere = 27,
    GotoRemove = 28,
    GotoMap = 29,
    GotoLocation = 30,
    GotoXYZ = 31,
    TriggerAdd = 32,
    TriggerRemove = 33,
    SpellSet = 34,
    SpellEffectSet = 35,
    SpellProcDataSet = 36,
    SpellProcLinkSet = 37,
    CreatureAddScript = 38,
    CreatureRemoveScript = 39,
    CreatureMove = 40,
    CreatureFollow = 41,
    CreatureWander = 42,
    CreatureStop = 43
})

local GroupID = P.IDs("group",
{
    Player = 1,
    GameMaster = 2
})

local Visibility = P.Category("visibility",
{
    CreatureSeeDespawned = P.Boolean(PermissionID.SeeDespawnedCreatures, "creature.see_despawned",
    {
        description = "Allows seeing creatures while they are in the despawned lifecycle state.",
        default = false
    })
})

local Command = P.Category("command",
{
    Damage = P.Boolean(PermissionID.Damage, "damage", { description = "Allows dealing administrative damage." }),
    Heal = P.Boolean(PermissionID.Heal, "heal", { description = "Allows performing administrative healing." }),
    Kill = P.Boolean(PermissionID.Kill, "kill", { description = "Allows administratively killing units." }),
    Resurrect = P.Boolean(PermissionID.Resurrect, "resurrect", { description = "Allows administratively resurrecting units." }),
    UnitMorph = P.Boolean(PermissionID.UnitMorph, "unit.morph", { description = "Allows changing a unit's display model." }),
    UnitDemorph = P.Boolean(PermissionID.UnitDemorph, "unit.demorph", { description = "Allows restoring a unit's display model." }),
    Teleport = P.Boolean(PermissionID.Teleport, "teleport", { description = "Allows administrative teleportation." }),
    CharacterAdd = P.Boolean(PermissionID.CharacterAdd, "character.add", { description = "Allows creating characters administratively." }),
    CharacterRemove = P.Boolean(PermissionID.CharacterRemove, "character.remove", { description = "Allows deleting characters administratively." }),
    UnitSetRace = P.Boolean(PermissionID.UnitSetRace, "unit.set_race", { description = "Allows changing a unit's race." }),
    UnitSetGender = P.Boolean(PermissionID.UnitSetGender, "unit.set_gender", { description = "Allows changing a unit's gender." }),
    UnitSetClass = P.Boolean(PermissionID.UnitSetClass, "unit.set_class", { description = "Allows changing a unit's class." }),
    UnitSetLevel = P.Boolean(PermissionID.UnitSetLevel, "unit.set_level", { description = "Allows changing a unit's level." }),
    ItemSetTemplate = P.Boolean(PermissionID.ItemSetTemplate, "item.set_template", { description = "Allows changing an item's template." }),
    ItemSetStatTemplate = P.Boolean(PermissionID.ItemSetStatTemplate, "item.set_stat_template", { description = "Allows changing an item's stat template." }),
    ItemSetArmorTemplate = P.Boolean(PermissionID.ItemSetArmorTemplate, "item.set_armor_template", { description = "Allows changing an item's armor template." }),
    ItemSetWeaponTemplate = P.Boolean(PermissionID.ItemSetWeaponTemplate, "item.set_weapon_template", { description = "Allows changing an item's weapon template." }),
    ItemSetShieldTemplate = P.Boolean(PermissionID.ItemSetShieldTemplate, "item.set_shield_template", { description = "Allows changing an item's shield template." }),
    ItemAdd = P.Boolean(PermissionID.ItemAdd, "item.add", { description = "Allows creating items administratively." }),
    ItemRemove = P.Boolean(PermissionID.ItemRemove, "item.remove", { description = "Allows deleting items administratively." }),
    CreatureAdd = P.Boolean(PermissionID.CreatureAdd, "creature.add", { description = "Allows creating creature spawns." }),
    CreatureRemove = P.Boolean(PermissionID.CreatureRemove, "creature.remove", { description = "Allows deleting creature spawns." }),
    CreatureInfo = P.Boolean(PermissionID.CreatureInfo, "creature.info", { description = "Allows inspecting creature information." }),
    MapAdd = P.Boolean(PermissionID.MapAdd, "map.add", { description = "Allows adding maps administratively." }),
    GotoAdd = P.Boolean(PermissionID.GotoAdd, "goto.add", { description = "Allows adding named teleport locations." }),
    GotoAddHere = P.Boolean(PermissionID.GotoAddHere, "goto.add_here", { description = "Allows adding a named teleport location at the current position." }),
    GotoRemove = P.Boolean(PermissionID.GotoRemove, "goto.remove", { description = "Allows removing named teleport locations." }),
    GotoMap = P.Boolean(PermissionID.GotoMap, "goto.map", { description = "Allows teleporting to a map." }),
    GotoLocation = P.Boolean(PermissionID.GotoLocation, "goto.location", { description = "Allows teleporting to named locations." }),
    GotoXYZ = P.Boolean(PermissionID.GotoXYZ, "goto.xyz", { description = "Allows teleporting to explicit coordinates." }),
    TriggerAdd = P.Boolean(PermissionID.TriggerAdd, "trigger.add", { description = "Allows creating proximity triggers." }),
    TriggerRemove = P.Boolean(PermissionID.TriggerRemove, "trigger.remove", { description = "Allows deleting proximity triggers." }),
    SpellSet = P.Boolean(PermissionID.SpellSet, "spell.set", { description = "Allows modifying spell definitions." }),
    SpellEffectSet = P.Boolean(PermissionID.SpellEffectSet, "spell_effect.set", { description = "Allows modifying spell-effect definitions." }),
    SpellProcDataSet = P.Boolean(PermissionID.SpellProcDataSet, "spell_proc_data.set", { description = "Allows modifying spell-proc data." }),
    SpellProcLinkSet = P.Boolean(PermissionID.SpellProcLinkSet, "spell_proc_link.set", { description = "Allows modifying spell-proc links." }),
    CreatureAddScript = P.Boolean(PermissionID.CreatureAddScript, "creature.add_script", { description = "Allows attaching scripts to creatures." }),
    CreatureRemoveScript = P.Boolean(PermissionID.CreatureRemoveScript, "creature.remove_script", { description = "Allows detaching scripts from creatures." }),
    CreatureMove = P.Boolean(PermissionID.CreatureMove, "creature.move", { description = "Allows moving creature spawns." }),
    CreatureFollow = P.Boolean(PermissionID.CreatureFollow, "creature.follow", { description = "Allows forcing creatures to follow a target." }),
    CreatureWander = P.Boolean(PermissionID.CreatureWander, "creature.wander", { description = "Allows forcing creatures to wander." }),
    CreatureStop = P.Boolean(PermissionID.CreatureStop, "creature.stop", { description = "Allows stopping creature movement." })
})

local Player = P.Group(GroupID.Player, "player",
{
    description = "Baseline permissions for ordinary players."
})

local GameMaster = P.Group(GroupID.GameMaster, "game_master",
{
    description = "Baseline permissions for game masters.",
    inherits =
    {
        Player
    },
    permissions =
    {
        P.Allow(Visibility.CreatureSeeDespawned),
        P.Allow(Command.Damage),
        P.Allow(Command.Heal),
        P.Allow(Command.Kill),
        P.Allow(Command.Resurrect),
        P.Allow(Command.UnitMorph),
        P.Allow(Command.UnitDemorph),
        P.Allow(Command.Teleport),
        P.Allow(Command.CharacterAdd),
        P.Allow(Command.CharacterRemove),
        P.Allow(Command.UnitSetRace),
        P.Allow(Command.UnitSetGender),
        P.Allow(Command.UnitSetClass),
        P.Allow(Command.UnitSetLevel),
        P.Allow(Command.ItemSetTemplate),
        P.Allow(Command.ItemSetStatTemplate),
        P.Allow(Command.ItemSetArmorTemplate),
        P.Allow(Command.ItemSetWeaponTemplate),
        P.Allow(Command.ItemSetShieldTemplate),
        P.Allow(Command.ItemAdd),
        P.Allow(Command.ItemRemove),
        P.Allow(Command.CreatureAdd),
        P.Allow(Command.CreatureRemove),
        P.Allow(Command.CreatureInfo),
        P.Allow(Command.MapAdd),
        P.Allow(Command.GotoAdd),
        P.Allow(Command.GotoAddHere),
        P.Allow(Command.GotoRemove),
        P.Allow(Command.GotoMap),
        P.Allow(Command.GotoLocation),
        P.Allow(Command.GotoXYZ),
        P.Allow(Command.TriggerAdd),
        P.Allow(Command.TriggerRemove),
        P.Allow(Command.SpellSet),
        P.Allow(Command.SpellEffectSet),
        P.Allow(Command.SpellProcDataSet),
        P.Allow(Command.SpellProcLinkSet),
        P.Allow(Command.CreatureAddScript),
        P.Allow(Command.CreatureRemoveScript),
        P.Allow(Command.CreatureMove),
        P.Allow(Command.CreatureFollow),
        P.Allow(Command.CreatureWander),
        P.Allow(Command.CreatureStop)
    }
})

return D.Definitions
{
    P.Catalog("Permissions",
    {
        permissions =
        {
            Visibility,
            Command
        },
        groups =
        {
            Player,
            GameMaster
        },
        defaultAccountGroup = Player
    })
}
