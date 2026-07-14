local Output = require("Output")

local M = {}

local valueKinds = { boolean = "Boolean", level = "Level", limit = "Limit" }
local FIRST_CUSTOM_ID = 1000000

local function Fail(message)
    error("Permission MetaGen validation failed: " .. message, 0)
end

local function IsInteger(value)
    return type(value) == "number" and value % 1 == 0
end

local function IsArray(value)
    if type(value) ~= "table" then return false end

    local length = #value
    local count = 0
    for key in pairs(value) do
        if not IsInteger(key) or key < 1 or key > length then return false end
        count = count + 1
    end
    return count == length
end

local function ValidateIdentifier(value, description)
    if type(value) ~= "string" or not value:match("^[A-Za-z_][A-Za-z0-9_]*$") then
        Fail(description .. " must be a valid C++ identifier")
    end
end

local function ValidateCategoryMember(value, description)
    if type(value) ~= "string" or not value:match("^[A-Z][A-Za-z0-9]*$") then
        Fail(description .. " must be a PascalCase identifier")
    end
end

local function ValidateKey(value, description)
    if type(value) ~= "string" or value == "" or not value:match("^[a-z0-9_.]+$") then
        Fail(description .. " must contain only lowercase letters, digits, underscores, and dots")
    end
end

local function ValidateKeySegment(value, description)
    if type(value) ~= "string" or value == "" or not value:match("^[a-z0-9_]+$") then
        Fail(description .. " must contain only lowercase letters, digits, and underscores")
    end
end

local function ToPascalCase(value)
    local result = ""
    for segment in value:gmatch("[a-z0-9]+") do
        result = result .. segment:sub(1, 1):upper() .. segment:sub(2)
    end
    return result
end

local function CopyValues(values)
    local result = {}
    for permissionName, value in pairs(values) do result[permissionName] = value end
    return result
end

local function MergeInherited(current, incoming)
    if current == nil then return incoming end
    if current <= 0 then return incoming <= 0 and math.min(current, incoming) or current end
    return incoming <= 0 and incoming or math.max(current, incoming)
end

local function Build(definitions)
    if not IsArray(definitions) or #definitions ~= 1 then
        Fail("exactly one permission catalogue must be defined")
    end
    local definition = definitions[1]
    local rawPermissions = definition.options.permissions
    local rawGroups = definition.options.groups
    local rawDefaultAccountGroup = definition.options.defaultAccountGroup
    if not IsArray(rawPermissions) or not IsArray(rawGroups) then
        Fail("catalogue permissions and groups must be arrays")
    end

    local result = { permissions = {}, groups = {} }
    local registries = {}
    local consumedTokens = {}

    local function ConsumeStableID(token, namespace, description)
        if type(token) ~= "table" or token._kind ~= "stableID" then
            Fail(description .. " must use an ID token from P.IDs")
        end
        if token._namespace ~= namespace then
            Fail(description .. " uses a " .. tostring(token._namespace) .. " ID instead of a " .. namespace .. " ID")
        end
        if token.retired then
            Fail(description .. " uses retired ID " .. tostring(token.id))
        end
        if consumedTokens[token] then
            Fail(description .. " reuses stable ID token '" .. token.name .. "'")
        end

        local existingRegistry = registries[namespace]
        if existingRegistry ~= nil and existingRegistry ~= token._registry then
            Fail("catalogue uses more than one " .. namespace .. " ID registry")
        end
        registries[namespace] = token._registry
        consumedTokens[token] = true
        return token.id
    end

    local permissionByObject, permissionByName, permissionByID, permissionIDs, permissionKeys = {}, {}, {}, {}, {}
    local function AddPermission(permission, category, categoryMember)
        if type(permission) ~= "table" or permission._kind ~= "permission" then
            Fail("permission catalogue entries must be permissions or categories")
        end
        if permissionByObject[permission] then
            Fail("permission object is included more than once")
        end
        ValidateKey(permission.key, "permission key")

        local fullKey = category == "" and permission.key or category .. "." .. permission.key
        local name
        if category == "" then
            name = ToPascalCase(fullKey)
        else
            ValidateCategoryMember(categoryMember, "permission category member")
            name = ToPascalCase(category) .. categoryMember
        end
        ValidateIdentifier(name, "derived permission name for key '" .. fullKey .. "'")
        local id = ConsumeStableID(permission.stableID, "permission", "permission '" .. fullKey .. "'")
        if not IsInteger(id) or id <= 0 or id >= FIRST_CUSTOM_ID then
            Fail("permission '" .. fullKey .. "' must use a reserved system ID below " .. FIRST_CUSTOM_ID)
        end
        if permissionIDs[id] then Fail("duplicate permission ID " .. id) end
        if permissionByName[name] then Fail("duplicate derived permission name '" .. name .. "'") end
        if permissionKeys[fullKey] then Fail("duplicate permission key '" .. fullKey .. "'") end
        if valueKinds[permission.valueKind] == nil then
            Fail("permission '" .. fullKey .. "' has an unknown value kind")
        end
        if type(permission.description) ~= "string" or permission.description == "" then
            Fail("permission '" .. fullKey .. "' requires a description")
        end

        local defaultValue = permission.defaultValue
        if permission.valueKind == "boolean" then
            if type(defaultValue) ~= "boolean" then
                Fail("boolean permission '" .. fullKey .. "' requires a boolean default")
            end
            defaultValue = defaultValue and 1 or 0
        elseif not IsInteger(defaultValue) then
            Fail("permission '" .. fullKey .. "' requires an integer default value")
        end

        local normalized = {
            object = permission, name = name, id = id, key = fullKey, category = category,
            valueKind = permission.valueKind, description = permission.description, defaultValue = defaultValue
        }
        permissionByObject[permission] = normalized
        permissionByName[name] = normalized
        permissionByID[id] = normalized
        permissionIDs[id] = true
        permissionKeys[fullKey] = true
        table.insert(result.permissions, normalized)
    end

    local categoryKeys = {}
    for _, entry in ipairs(rawPermissions) do
        if type(entry) == "table" and entry._kind == "permissionCategory" then
            ValidateKeySegment(entry._key, "permission category key")
            if categoryKeys[entry._key] then Fail("duplicate permission category '" .. entry._key .. "'") end
            if not IsArray(entry._permissions) then
                Fail("permission category '" .. entry._key .. "' must contain an array")
            end
            categoryKeys[entry._key] = true
            for _, categoryEntry in ipairs(entry._permissions) do
                AddPermission(categoryEntry.permission, entry._key, categoryEntry.name)
            end
        else
            AddPermission(entry, "", nil)
        end
    end

    local groupByObject, groupByName, groupByID, groupIDs, groupKeys = {}, {}, {}, {}, {}
    for _, group in ipairs(rawGroups) do
        if type(group) ~= "table" or group._kind ~= "permissionGroup" then
            Fail("catalogue group entries must be groups")
        end
        if groupByObject[group] then Fail("permission group object is included more than once") end
        ValidateKeySegment(group.key, "permission group key")
        local name = ToPascalCase(group.key)
        ValidateIdentifier(name, "derived permission group name for key '" .. group.key .. "'")
        local id = ConsumeStableID(group.stableID, "group", "permission group '" .. group.key .. "'")
        if not IsInteger(id) or id <= 0 or id >= FIRST_CUSTOM_ID then
            Fail("permission group '" .. group.key .. "' must use a reserved system ID below " .. FIRST_CUSTOM_ID)
        end
        if groupIDs[id] then Fail("duplicate permission group ID " .. id) end
        if groupByName[name] then Fail("duplicate derived permission group name '" .. name .. "'") end
        if groupKeys[group.key] then Fail("duplicate permission group key '" .. group.key .. "'") end
        if type(group.description) ~= "string" or group.description == "" then
            Fail("permission group '" .. group.key .. "' requires a description")
        end
        if not IsArray(group.inherits) or not IsArray(group.permissions) then
            Fail("permission group '" .. group.key .. "' inherits and permissions must be arrays")
        end

        local normalized = {
            object = group, name = name, id = id, key = group.key, description = group.description,
            inherits = group.inherits, directPermissions = group.permissions
        }
        groupByObject[group] = normalized
        groupByName[name] = normalized
        groupByID[id] = normalized
        groupIDs[id] = true
        groupKeys[group.key] = true
        table.insert(result.groups, normalized)
    end

    local function ValidateRegistry(namespace)
        local registry = registries[namespace]
        if registry == nil then Fail("catalogue requires a " .. namespace .. " ID registry") end
        if not IsArray(registry._entries) then Fail(namespace .. " ID registry entries must be an array") end
        local ids = {}
        for _, token in ipairs(registry._entries) do
            ValidateIdentifier(token.name, namespace .. " ID registry entry")
            if not IsInteger(token.id) or token.id <= 0 or token.id >= FIRST_CUSTOM_ID then
                Fail(namespace .. " ID registry entry '" .. token.name ..
                    "' must use a reserved system ID below " .. FIRST_CUSTOM_ID)
            end
            if ids[token.id] then Fail(namespace .. " ID registry contains duplicate ID " .. token.id) end
            ids[token.id] = true
            if token.retired then
                if type(token.description) ~= "string" or token.description == "" then
                    Fail("retired " .. namespace .. " ID '" .. token.name .. "' requires a description")
                end
            elseif not consumedTokens[token] then
                Fail("active " .. namespace .. " ID '" .. token.name .. "' is not used")
            end
        end
    end
    ValidateRegistry("permission")
    ValidateRegistry("group")

    local function StableIDsMatch(left, right, namespace)
        return type(left) == "table" and type(right) == "table" and
            left._kind == "stableID" and right._kind == "stableID" and
            left._namespace == namespace and right._namespace == namespace and
            left.name == right.name and left.id == right.id and
            not left.retired and not right.retired
    end

    local function ResolvePermission(permission)
        if type(permission) ~= "table" or permission._kind ~= "permission" or
            type(permission.stableID) ~= "table"
        then
            return nil
        end

        local candidate = permissionByID[permission.stableID.id]
        if candidate == nil or not StableIDsMatch(permission.stableID, candidate.object.stableID, "permission") or
            permission.key ~= candidate.object.key or permission.valueKind ~= candidate.object.valueKind or
            permission.description ~= candidate.object.description or permission.defaultValue ~= candidate.object.defaultValue
        then
            return nil
        end
        return candidate
    end

    local function ResolveGroup(group)
        if type(group) ~= "table" or group._kind ~= "permissionGroup" or type(group.stableID) ~= "table" then
            return nil
        end

        local candidate = groupByID[group.stableID.id]
        if candidate == nil or not StableIDsMatch(group.stableID, candidate.object.stableID, "group") or
            group.key ~= candidate.object.key or group.description ~= candidate.object.description
        then
            return nil
        end
        return candidate
    end

    local states = {}
    local function Flatten(group)
        local state = states[group]
        if state == "done" then return group.permissions, group.inheritablePermissions end
        if state == "visiting" then Fail("permission group inheritance cycle involving '" .. group.name .. "'") end
        states[group] = "visiting"

        local effective = {}
        local inheritable = {}
        local ancestors = {}
        for _, parentObject in ipairs(group.inherits) do
            local parent = groupByObject[parentObject] or ResolveGroup(parentObject)
            if parent == nil then
                Fail("permission group '" .. group.name .. "' inherits a group outside the catalogue")
            end
            local _, parentInheritable = Flatten(parent)
            for permissionName, value in pairs(parentInheritable) do
                effective[permissionName] = MergeInherited(effective[permissionName], value)
                inheritable[permissionName] = MergeInherited(inheritable[permissionName], value)
            end
            ancestors[parent.name] = true
            for ancestorName in pairs(parent.ancestors) do ancestors[ancestorName] = true end
        end

        local directSeen = {}
        for _, assignment in ipairs(group.directPermissions) do
            if type(assignment) ~= "table" or assignment._kind ~= "permissionAssignment" then
                Fail("permission group '" .. group.name .. "' contains an invalid permission assignment")
            end
            local permission = permissionByObject[assignment.permission] or ResolvePermission(assignment.permission)
            if permission == nil then
                Fail("permission group '" .. group.name .. "' references a permission outside the catalogue")
            end
            if directSeen[permission.name] then
                Fail("permission group '" .. group.name .. "' assigns permission '" .. permission.name .. "' more than once")
            end

            local value
            if assignment.action == "allow" or assignment.action == "deny" or assignment.action == "denyForGroup" then
                if permission.valueKind ~= "boolean" then
                    Fail("permission group '" .. group.name .. "' must use P.Set for non-boolean permission '" .. permission.name .. "'")
                end
                value = assignment.action == "allow" and 1 or 0
            elseif assignment.action == "set" then
                if permission.valueKind == "boolean" then
                    Fail("permission group '" .. group.name ..
                        "' must use P.Allow, P.Deny, or P.DenyForGroup for boolean permission '" .. permission.name .. "'")
                end
                if not IsInteger(assignment.value) then
                    Fail("permission group '" .. group.name .. "' assigns a non-integer value")
                end
                value = assignment.value
            else
                Fail("permission group '" .. group.name .. "' uses an unknown permission assignment action")
            end

            directSeen[permission.name] = true
            effective[permission.name] = value
            if assignment.action ~= "denyForGroup" then
                inheritable[permission.name] = value
            end
        end

        group.permissions = CopyValues(effective)
        group.inheritablePermissions = CopyValues(inheritable)
        group.ancestors = ancestors
        states[group] = "done"
        return group.permissions, group.inheritablePermissions
    end

    for _, group in ipairs(result.groups) do Flatten(group) end
    result.defaultAccountGroup = groupByObject[rawDefaultAccountGroup] or ResolveGroup(rawDefaultAccountGroup)
    if result.defaultAccountGroup == nil then
        Fail("defaultAccountGroup must reference a group in the catalogue")
    end
    if result.defaultAccountGroup.id ~= 1 then
        Fail("defaultAccountGroup must use reserved group ID 1")
    end

    table.sort(result.permissions, function(a, b) return a.id < b.id end)
    table.sort(result.groups, function(a, b) return a.id < b.id end)
    return result
end

function M.Validate(_, definitions)
    Build(definitions)
end

function M.PlanOutputs()
    return { "Server/Permission/Permission.h" }
end

function M.Emit(_, definitions, context)
    local catalog = Build(definitions)
    local path = context.stagingRootDir .. "/Server/Permission"
    local makeDirectory = context.makeDirectory or os.mkdir
    makeDirectory(context.stagingRootDir .. "/Server")
    makeDirectory(path)

    Output.Write(context, path .. "/Permission.h", function()
        local cpp = context.cpp
        cpp:Line("#pragma once")
        cpp:BlankLine()
        cpp:Line("#include <Base/Types.h>")
        cpp:BlankLine()
        cpp:Line("#include <array>")
        cpp:Line("#include <string_view>")
        cpp:BlankLine()
        cpp:Block("namespace MetaGen::Server::Permission", function()
            cpp:Line("enum class PermissionValueKind : u8")
            cpp:Line("{")
            context.writer:AddIndent()
            cpp:Line("Boolean,")
            cpp:Line("Level,")
            cpp:Line("Limit")
            context.writer:SubIndent()
            cpp:Line("};")
            cpp:BlankLine()

            cpp:Line("enum class Permission : u32")
            cpp:Line("{")
            context.writer:AddIndent()
            for index, permission in ipairs(catalog.permissions) do
                cpp:Line(permission.name .. " = " .. permission.id .. (index < #catalog.permissions and "," or ""))
            end
            context.writer:SubIndent()
            cpp:Line("};")
            cpp:BlankLine()

            cpp:Line("enum class PermissionGroup : u32")
            cpp:Line("{")
            context.writer:AddIndent()
            for index, group in ipairs(catalog.groups) do
                cpp:Line(group.name .. " = " .. group.id .. (index < #catalog.groups and "," or ""))
            end
            context.writer:SubIndent()
            cpp:Line("};")
            cpp:BlankLine()
            cpp:Line("inline constexpr PermissionGroup DEFAULT_ACCOUNT_GROUP = PermissionGroup::" .. catalog.defaultAccountGroup.name .. ";")
            cpp:BlankLine()
            cpp:Line("[[nodiscard]] constexpr bool GroupInherits(PermissionGroup group, PermissionGroup ancestor)")
            cpp:Line("{")
            context.writer:AddIndent()
            cpp:Line("switch (group)")
            cpp:Line("{")
            context.writer:AddIndent()
            for _, group in ipairs(catalog.groups) do
                local ancestors = {}
                for ancestorName in pairs(group.ancestors) do table.insert(ancestors, ancestorName) end
                table.sort(ancestors, function(a, b) return a < b end)
                if #ancestors > 0 then
                    local conditions = {}
                    for _, ancestorName in ipairs(ancestors) do
                        table.insert(conditions, "ancestor == PermissionGroup::" .. ancestorName)
                    end
                    cpp:Line("case PermissionGroup::" .. group.name .. ": return " .. table.concat(conditions, " || ") .. ";")
                end
            end
            cpp:Line("default: return false;")
            context.writer:SubIndent()
            cpp:Line("}")
            context.writer:SubIndent()
            cpp:Line("}")
            cpp:BlankLine()

            cpp:Struct("PermissionDescriptor", function()
                cpp:Variable("Permission", "id")
                cpp:Variable("std::string_view", "name")
                cpp:Variable("std::string_view", "category")
                cpp:Variable("PermissionValueKind", "valueKind")
                cpp:Variable("std::string_view", "description")
                cpp:Variable("i64", "defaultValue")
            end)
            cpp:BlankLine()
            cpp:Struct("PermissionGroupEntry", function()
                cpp:Variable("Permission", "permission")
                cpp:Variable("i64", "value")
            end)
            cpp:BlankLine()
            cpp:Struct("PermissionGroupDescriptor", function()
                cpp:Variable("PermissionGroup", "id")
                cpp:Variable("std::string_view", "name")
                cpp:Variable("std::string_view", "description")
                cpp:Variable("const PermissionGroupEntry", "permissions", nil, { pointer = true })
                cpp:Variable("u32", "numPermissions")
            end)
            cpp:BlankLine()

            cpp:Line("inline constexpr std::array<PermissionDescriptor, " .. #catalog.permissions .. "> PERMISSIONS =")
            cpp:Line("{{")
            context.writer:AddIndent()
            for index, permission in ipairs(catalog.permissions) do
                local suffix = index < #catalog.permissions and "," or ""
                cpp:Line("{ Permission::" .. permission.name .. ", " .. cpp:String(permission.key) .. ", " ..
                    cpp:String(permission.category) .. ", PermissionValueKind::" .. valueKinds[permission.valueKind] .. ", " ..
                    cpp:String(permission.description) .. ", " .. permission.defaultValue .. " }" .. suffix)
            end
            context.writer:SubIndent()
            cpp:Line("}};")
            cpp:BlankLine()

            local permissionIDByName = {}
            for _, permission in ipairs(catalog.permissions) do permissionIDByName[permission.name] = permission.id end
            for _, group in ipairs(catalog.groups) do
                local entries = {}
                for permissionName, value in pairs(group.permissions) do
                    table.insert(entries, { name = permissionName, value = value })
                end
                table.sort(entries, function(a, b)
                    return permissionIDByName[a.name] < permissionIDByName[b.name]
                end)
                cpp:Line("inline constexpr std::array<PermissionGroupEntry, " .. #entries .. "> " .. group.name .. "Permissions =")
                cpp:Line("{{")
                context.writer:AddIndent()
                for index, entry in ipairs(entries) do
                    cpp:Line("{ Permission::" .. entry.name .. ", " .. entry.value .. " }" .. (index < #entries and "," or ""))
                end
                context.writer:SubIndent()
                cpp:Line("}};")
                cpp:BlankLine()
            end

            cpp:Line("inline constexpr std::array<PermissionGroupDescriptor, " .. #catalog.groups .. "> PERMISSION_GROUPS =")
            cpp:Line("{{")
            context.writer:AddIndent()
            for index, group in ipairs(catalog.groups) do
                cpp:Line("{ PermissionGroup::" .. group.name .. ", " .. cpp:String(group.key) .. ", " .. cpp:String(group.description) ..
                    ", " .. group.name .. "Permissions.data(), static_cast<u32>(" .. group.name .. "Permissions.size()) }" ..
                    (index < #catalog.groups and "," or ""))
            end
            context.writer:SubIndent()
            cpp:Line("}};")
        end)
    end)
end

return M
