local mod = Solution.Util.CreateModuleTable("Server-Common", { "base", "fileformat", "input", "network", "gameplay", "luau-compiler", "luau-vm", "luau-analysis", "enkits", "refl-cpp", "utfcpp", "base64", "libpqxx" })

Solution.Util.CreateStaticLib(mod.Name, Solution.Projects.Current.BinDir, mod.Dependencies, function()
    local defines = { "_CRT_SECURE_NO_WARNINGS", "_SILENCE_ALL_CXX17_DEPRECATION_WARNINGS", "_SILENCE_ALL_MS_EXT_DEPRECATION_WARNINGS" }

    Solution.Util.SetLanguage("C++")
    Solution.Util.SetCppDialect(20)

    local files = Solution.Util.GetFilesForCpp(mod.Path)
    Solution.Util.SetFiles(files)
    Solution.Util.SetIncludes(mod.Path)
    Solution.Util.SetDefines(defines)

    Solution.Util.SetFilter("platforms:Win64", function()
        Solution.Util.SetDefines({"WIN32_LEAN_AND_MEAN", "NOMINMAX"})
    end)
    
    dependson {"Luau-Compiler", "Luau-VM"}
end)

Solution.Util.CreateDep(mod.NameLow, mod.Dependencies, function()
    Solution.Util.SetIncludes(mod.Path)
    Solution.Util.SetLinks(mod.Name)
    
    Solution.Util.SetFilter("platforms:Win64", function()
        Solution.Util.SetDefines({"WIN32_LEAN_AND_MEAN", "NOMINMAX"})
    end)
end)