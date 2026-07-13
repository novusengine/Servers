local info = debug.getinfo(1, "S")
local root = assert(info.source:sub(2):gsub("\\", "/"):match("(.*/)"))
local engineMetaRoot = path.getabsolute("../../Submodules/Engine/Source/Meta", root):gsub("\\", "/") .. "/"
package.path = engineMetaRoot .. "?.lua;" .. package.path

require("OrderedTable")
local Model = require("Model")
local PostgresBackend = require("PostgresBackend")
local PostgresModel = require("PostgresModel")
local TestSuite = require("TestSuite")

local expectedCounts = { auth = 1, character = 6, world = 18 }
local expectedNames =
{
    auth = "accounts",
    character = "character_currency|character_items|character_permission_groups|character_permissions|characters|item_instances",
    world = "creature_templates|creatures|currency|item_armor_templates|item_shield_templates|item_stat_templates|" ..
        "item_templates|item_weapon_templates|map_locations|maps|permission_group_data|permission_groups|permissions|" ..
        "proximity_triggers|spell_effects|spell_proc_data|spell_proc_link|spells"
}

local function Run()
    local profile = MetaGen.ResolveProject()
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

    assert(#normalized.tables == 25)
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

    local context = { postgresHistoryByBundle = assert(profile.postgres and profile.postgres.historyByBundle) }
    PostgresBackend.Validate(model, definitions, context)
    assert(#context.postgresMigrations.auth == 1)
    assert(#context.postgresMigrations.character == 1)
    assert(#context.postgresMigrations.world == 1)
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
