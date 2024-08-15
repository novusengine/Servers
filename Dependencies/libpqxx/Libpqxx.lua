local function getPostgresInfo()
    local includeDirs = {}
    local libDirs = {}
    local libs = {}
    
    if os.target() == "windows" then
        local osEnvName = "POSTGRES_ROOT"
        local postgresRoot = os.getenv(osEnvName)

        if not postgresRoot then
            Solution.Util.PrintError("Failed to find System Environment Variable '" .. osEnvName .. "'. Please ensure Postgres is installed and that " .. osEnvName .. " has been properly configured.")
        else
            print("Found PostgreSQL at: " .. postgresRoot)
            table.insert(includeDirs, postgresRoot .. "/include")
            table.insert(libs, postgresRoot .. "/lib/libpq.lib")
        end
    else
        -- Fetch PostgreSQL include, lib paths and version using pg_config on Linux
        local pgInclude = io.popen("pg_config --includedir"):read("*a"):gsub("\n", "")
        local pgIncludeServer = io.popen("pg_config --includedir-server"):read("*a"):gsub("\n", "")
        local pgLibDir = io.popen("pg_config --libdir"):read("*a"):gsub("\n", "")
        local postgresVersion = io.popen("pg_config --version"):read("*a"):gsub("\n", "")

        if pgInclude == "" or pgLibDir == "" then
            Solution.Util.PrintError("Failed to find PostgreSQL installation using 'pg_config'. Please ensure PostgreSQL is installed.")
        else
            print("PostgreSQL version: " .. postgresVersion)
            print("PostgreSQL include dir: " .. pgInclude)
            table.insert(includeDirs, pgInclude)
            print("PostgreSQL include-server dir: " .. pgIncludeServer)
            table.insert(includeDirs, pgIncludeServer)
            print("PostgreSQL lib dir: " .. pgLibDir)
            table.insert(libDirs, pgLibDir)
            table.insert(libs, "pq")
        end
    end
    
    return includeDirs, libDirs, libs
end

local dep = Solution.Util.CreateDepTable("Libpqxx", {})

Solution.Util.CreateStaticLib(dep.Name, Solution.Projects.Current.BinDir, dep.Dependencies, function()
    local defines = { "_SILENCE_ALL_CXX17_DEPRECATION_WARNINGS", "_SILENCE_ALL_MS_EXT_DEPRECATION_WARNINGS" }

    Solution.Util.SetLanguage("C++")
    Solution.Util.SetCppDialect(20)

    local baseDir = dep.Path .. "/Libpqxx"
    local sourceDir = baseDir .. "/src"

    -- Include directories setup
    local files =
    {
        sourceDir .. "/**.cxx",
    }
    Solution.Util.SetFiles(files)
    
    -- Get libraries and includes from system
    local foundIncludeDirs, foundLibDirs, foundLibs = getPostgresInfo()
    local includeDirs = { baseDir .. "/include", foundIncludeDirs }
    Solution.Util.SetIncludes(includeDirs)
    Solution.Util.SetDefines(defines)

    -- Library link setup
    Solution.Util.SetLibDirs(foundLibDirs)
    Solution.Util.SetLinks(foundLibs)
end)

Solution.Util.CreateDep(dep.NameLow, dep.Dependencies, function()
    local baseDir = dep.Path .. "/Libpqxx"
    local _, _, foundLibs = getPostgresInfo()
    Solution.Util.SetIncludes({baseDir .. "/include"})
    Solution.Util.SetLinks({ dep.Name, foundLibs })
end)
