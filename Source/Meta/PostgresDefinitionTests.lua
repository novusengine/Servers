local info = debug.getinfo(1, "S")
local root = assert(info.source:sub(2):gsub("\\", "/"):match("(.*/)"))
local engineMetaRoot = path.getabsolute("../../Submodules/Engine/Source/Meta", root):gsub("\\", "/") .. "/"
package.path = engineMetaRoot .. "?.lua;" .. package.path

require("OrderedTable")
local Model = require("Model")
local PostgresBackend = require("PostgresBackend")
local PostgresModel = require("PostgresModel")
local TestSuite = require("TestSuite")

local expectedCounts = { auth = 6, character = 7, world = 21 }
local expectedNames =
{
    auth = "account_permission_groups|account_permissions|accounts|permission_group_data|permission_groups|permissions",
    character = "character_currency|character_items|character_permission_groups|character_permissions|character_reputations|characters|item_instances",
    world = "creature_class_level_stats|creature_templates|creatures|currency|faction_relations|faction_standings|faction_starting_reputations|factions|" ..
        "item_armor_templates|item_shield_templates|item_stat_templates|item_templates|item_weapon_templates|map_locations|maps|" ..
        "proximity_triggers|spell_effects|spell_proc_data|spell_proc_link|spells|unit_race_factions"
}

local function Run()
    local profile = MetaGen.ResolveProject()
    for _, provider in ipairs(profile.providers) do
        for _, moduleRoot in ipairs(provider.moduleRoots) do
            assert(os.isdir(moduleRoot), "MetaGen provider module root does not exist: " .. moduleRoot)
            package.path = moduleRoot .. "/?.lua;" .. moduleRoot .. "/?/Init.lua;" .. package.path
        end
    end

    local sourceSets = {}
    for _, provider in ipairs(profile.providers) do
        for _, source in ipairs(provider.sources) do
            local files = os.matchfiles(source.root .. "/**.lua")
            table.sort(files)
            table.insert(sourceSets, { files = files, root = source.root, namespace = source.namespace })
        end
    end
    local model = Model.LoadSourceSets(sourceSets)
    local definitions = model.definitionsByTarget.postgres or {}
    local normalized = PostgresModel.Build(model, definitions)

    assert(#normalized.tables == 34)
    assert(#normalized.bundles == 3)
    for _, bundle in ipairs(normalized.bundles) do
        assert(#bundle.tables == expectedCounts[bundle.name])
        local names = {}
        for _, tableModel in ipairs(bundle.tables) do names[#names + 1] = tableModel.tableName end
        table.sort(names)
        assert(table.concat(names, "|") == expectedNames[bundle.name])
    end
    assert(normalized.tablesByPersistentId["postgres.spell_effects"].orderBy[1].name == "spellId")
    assert(normalized.tablesByPersistentId["postgres.spell_effects"].orderBy[2].direction == "desc")
    assert(normalized.tablesByPersistentId["postgres.spell_proc_link"].orderBy[2].name == "procDataId")
    assert(#normalized.tablesByPersistentId["postgres.characters"].queries == 2)
    assert(normalized.tablesByPersistentId["postgres.accounts"].queries[1].cardinality == "zeroOrOne")
    assert(normalized.tablesByPersistentId["postgres.characters"].queries[1].cardinality == "zeroOrOne")
    assert(#normalized.tablesByPersistentId["postgres.item_instances"].operations == 6)
    assert(#normalized.tablesByPersistentId["postgres.character_items"].operations == 1)
    assert(#normalized.tablesByPersistentId["postgres.character_items"].constraints == 4)
    assert(#normalized.tablesByPersistentId["postgres.item_instances"].constraints == 2)
    assert(#normalized.tablesByPersistentId["postgres.characters"].indexes == 1)
    assert(#normalized.tablesByPersistentId["postgres.creatures"].indexes == 1)
    assert(#normalized.tablesByPersistentId["postgres.proximity_triggers"].indexes == 1)
    assert(#normalized.tablesByPersistentId["postgres.character_reputations"].operations == 3)
    assert(normalized.tablesByPersistentId["postgres.character_reputations"].queries[1].orderBy[1].name == "factionId")
    assert(normalized.tablesByPersistentId["postgres.unit_race_factions"].primaryKey == nil)
    assert(normalized.tablesByPersistentId["postgres.faction_relations"].orderBy[2].name == "targetFactionId")

    local context = { postgresHistoryByBundle = assert(profile.postgres and profile.postgres.historyByBundle) }
    PostgresBackend.Validate(model, definitions, context)
    assert(#context.postgresMigrations.auth == 5)
    assert(#context.postgresMigrations.character == 5)
    assert(#context.postgresMigrations.world == 6)
    Model.AssertUnchanged(model, "production PostgreSQL definition validation")
    print("MetaGen PostgreSQL production definitions passed")
end

TestSuite.Register("postgres-definitions", Run)

newaction
{
    trigger = "metagen-postgres-definition-tests",
    description = "Validate production PostgreSQL definitions and committed history",
    execute = Run
}
