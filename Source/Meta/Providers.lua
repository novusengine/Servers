MetaGen.RegisterProvider {
    name = "Servers.Runtime",
    definitionRoot = path.getabsolute("Definitions/Server", _SCRIPT_DIR),
    namespace = "MetaGen.Server",
    dependencies = { "Engine.Protocol" }
}

MetaGen.RegisterProvider {
    name = "Servers.Permission",
    definitionRoot = path.getabsolute("Definitions/Permission", _SCRIPT_DIR),
    namespace = "MetaGen.Server.Permission",
    moduleRoots = { path.getabsolute("Permission", _SCRIPT_DIR) },
    extensions = { path.getabsolute("Permission/PermissionExtension.lua", _SCRIPT_DIR) }
}

MetaGen.RegisterProvider {
    name = "Servers.Database",
    definitionRoot = path.getabsolute("Definitions/Database", _SCRIPT_DIR),
    namespace = "MetaGen.Database",
    inputs = {
        path.getabsolute("Definitions/Database/harden_inventory_and_map_ids.sql", _SCRIPT_DIR),
        path.getabsolute("Definitions/Database/Baseline/Character.sql", _SCRIPT_DIR),
        path.getabsolute("Definitions/Database/Baseline/World.sql", _SCRIPT_DIR)
    },
    dependencies = { "Engine.ClientDB", "Engine.Gameplay" }
}
