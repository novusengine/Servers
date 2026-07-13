local mod = Solution.Util.CreateModuleTable("Server-Game-App", { "server-game-lib" })

Solution.Util.CreateConsoleApp(mod.Name, Solution.Projects.Current.BinDir, mod.Dependencies, function()
    local defines = { "_SILENCE_ALL_CXX17_DEPRECATION_WARNINGS", "_SILENCE_ALL_MS_EXT_DEPRECATION_WARNINGS" }

    Solution.Util.SetLanguage("C++")
    Solution.Util.SetCppDialect(20)

    local projFile = mod.Path .. "/" .. mod.Name .. ".lua"
    local files = Solution.Util.GetFilesForCpp(mod.Path)
    table.insert(files, projFile)

    Solution.Util.SetFiles(files)
    Solution.Util.SetIncludes(mod.Path)
    Solution.Util.SetDefines(defines)

    Solution.Util.SetFilter("platforms:Win64", function()
        Solution.Util.SetDefines({"WIN32_LEAN_AND_MEAN", "NOMINMAX"})
    end)

    Solution.Util.SetFilter("system:Windows", function()
        Solution.Util.SetFiles({ "appicon.rc", "**.ico" })
        vpaths {
            ["Resources/*"] = { "*.rc", "**.ico" },
            ["/*"] = { "*.lua", "Server-Game-App/**" }
        }
    end)
end)
