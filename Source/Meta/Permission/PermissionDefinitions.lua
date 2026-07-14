local Type = require("Type")
local D = require("Definition")

local M = {}

local PermissionCatalogArchetype =
{
    kind = "permissionCatalog",
    base = Type.STRUCT,
    targets = { permission = true },
    components = {}
}

function M.Retired(id, description)
    return {
        _kind = "retiredStableID",
        id = id,
        description = description
    }
end

function M.IDs(namespace, entries)
    local registry = {
        _kind = "stableIDRegistry",
        _namespace = namespace,
        _entries = {}
    }

    for name, value in pairs(entries) do
        local retired = type(value) == "table" and value._kind == "retiredStableID"
        local token = {
            _kind = "stableID",
            _namespace = namespace,
            _registry = registry,
            name = name,
            id = retired and value.id or value,
            retired = retired,
            description = retired and value.description or nil
        }
        registry[name] = token
        table.insert(registry._entries, token)
    end

    return registry
end

local function Permission(valueKind, stableID, key, options)
    options = options or {}
    local defaultValue = options.default
    if defaultValue == nil then
        if valueKind == "boolean" then
            defaultValue = false
        else
            defaultValue = 0
        end
    end

    return {
        _kind = "permission",
        stableID = stableID,
        key = key,
        valueKind = valueKind,
        description = options.description,
        defaultValue = defaultValue
    }
end

function M.Boolean(stableID, key, options)
    return Permission("boolean", stableID, key, options)
end

function M.Level(stableID, key, options)
    return Permission("level", stableID, key, options)
end

function M.Limit(stableID, key, options)
    return Permission("limit", stableID, key, options)
end

function M.Category(key, permissions)
    local category = {
        _kind = "permissionCategory",
        _key = key,
        _permissions = {}
    }

    for memberName, permission in pairs(permissions) do
        if type(memberName) ~= "string" then
            error("Permission category '" .. key .. "' entries must use explicit member names", 0)
        end
        if category[memberName] ~= nil then
            error("Permission category '" .. key .. "' has duplicate member '" .. memberName .. "'", 0)
        end
        category[memberName] = permission
        table.insert(category._permissions, { name = memberName, permission = permission })
    end
    return category
end

function M.Allow(permission)
    return { _kind = "permissionAssignment", action = "allow", permission = permission }
end

function M.Deny(permission)
    return { _kind = "permissionAssignment", action = "deny", permission = permission }
end

function M.DenyForGroup(permission)
    return { _kind = "permissionAssignment", action = "denyForGroup", permission = permission }
end

function M.Set(permission, value)
    return { _kind = "permissionAssignment", action = "set", permission = permission, value = value }
end

function M.Group(stableID, key, options)
    options = options or {}
    return {
        _kind = "permissionGroup",
        stableID = stableID,
        key = key,
        description = options.description,
        inherits = options.inherits or {},
        permissions = options.permissions or {}
    }
end

function M.Catalog(name, options)
    return D.Struct(name, PermissionCatalogArchetype, {}, options)
end

return M
