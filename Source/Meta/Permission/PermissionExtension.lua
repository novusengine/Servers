local PermissionBackend = require("PermissionBackend")

return {
    definitionKinds = { "permissionCatalog" },
    backends = {
        { name = "permission", backend = PermissionBackend }
    }
}
