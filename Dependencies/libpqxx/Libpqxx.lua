local osEnvName = "POSTGRES_ROOT"
local postgresRoot = os.getenv(osEnvName)

if not postgresRoot then
    Solution.Util.PrintError("Failed to find System Environment Variable '" .. osEnvName .. ". Please ensure Postgres is installed and that " .. osEnvName .. " have been properly configured")
end

local dep = Solution.Util.CreateDepTable("Libpqxx", {})

Solution.Util.CreateStaticLib(dep.Name, Solution.Projects.Current.BinDir, dep.Dependencies, function()
    local defines = { "_SILENCE_ALL_CXX17_DEPRECATION_WARNINGS", "_SILENCE_ALL_MS_EXT_DEPRECATION_WARNINGS" }

    Solution.Util.SetLanguage("C++")
    Solution.Util.SetCppDialect(20)

    local baseDir = dep.Path .. "/Libpqxx"
    local sourceDir = baseDir .. "/src"
    local includeDirs = { baseDir .. "/include", postgresRoot .. "/include" }
    local files =
    {
        sourceDir .. "/**.cxx",
    }

    Solution.Util.SetFiles(files)
    Solution.Util.SetIncludes(includeDirs)
    Solution.Util.SetDefines(defines)
    Solution.Util.SetLinks({postgresRoot .. "/lib/libpq.lib"})
end)

Solution.Util.CreateDep(dep.NameLow, dep.Dependencies, function()
    local baseDir = dep.Path .. "/Libpqxx"

    Solution.Util.SetIncludes({baseDir .. "/include"})
    Solution.Util.SetLinks(dep.Name)
end)