local postgresRoot = ""
-- TODO: should probably not hardcode postgres dir for Linux but oh well
if os.target() == "windows" then
    local osEnvName = "POSTGRES_ROOT"
    postgresRoot = os.getenv(osEnvName)

    if not postgresRoot then
        Solution.Util.PrintError("Failed to find System Environment Variable '" .. osEnvName .. ". Please ensure Postgres is installed and that " .. osEnvName .. " have been properly configured")
    end
end

local dep = Solution.Util.CreateDepTable("Libpqxx", {})

Solution.Util.CreateStaticLib(dep.Name, Solution.Projects.Current.BinDir, dep.Dependencies, function()
    local defines = { "_SILENCE_ALL_CXX17_DEPRECATION_WARNINGS", "_SILENCE_ALL_MS_EXT_DEPRECATION_WARNINGS" }

    Solution.Util.SetLanguage("C++")
    Solution.Util.SetCppDialect(20)

    local baseDir = dep.Path .. "/Libpqxx"
    local sourceDir = baseDir .. "/src"
    local pqinclude = iif(os.istarget("windows"), postgresRoot .. "/include", { "/usr/include/postgresql", "/usr/include/postgresql/14/server" })
    
    if os.target() == "linux" then
        Solution.Util.SetLibDirs({ "/usr/lib/x86_64-linux-gnu", "/usr/lib/postgresql/14/lib" })
    end
    
    local includeDirs = { baseDir .. "/include", pqinclude }
    local files =
    {
        sourceDir .. "/**.cxx",
    }

    Solution.Util.SetFiles(files)
    Solution.Util.SetIncludes(includeDirs)
    Solution.Util.SetDefines(defines)
    
    local libPath = iif(os.istarget("windows"), postgresRoot .. "/lib/libpq.lib", "pq")
    Solution.Util.SetLinks(libPath)
end)

Solution.Util.CreateDep(dep.NameLow, dep.Dependencies, function()
    local baseDir = dep.Path .. "/Libpqxx"

    Solution.Util.SetIncludes({baseDir .. "/include"})
    local libPath = iif(os.istarget("windows"), postgresRoot .. "/lib/libpq.lib", "pq")
    Solution.Util.SetLinks({ dep.Name, libPath })
end)