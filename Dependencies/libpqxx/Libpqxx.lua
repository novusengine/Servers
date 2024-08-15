if os.target() == "windows" then
    local osEnvName = "POSTGRES_ROOT"
    local postgresRoot = os.getenv(osEnvName)

    if not postgresRoot then
        Solution.Util.PrintError("Failed to find System Environment Variable '" .. osEnvName .. "'. Please ensure PostgreSQL is installed and that " .. osEnvName .. " has been properly configured.")
    else
        print("Found PostgreSQL at: " .. postgresRoot)
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
        print("PostgreSQL include-server dir: " .. pgIncludeServer)
        print("PostgreSQL lib dir: " .. pgLibDir)
    end
end

local dep = Solution.Util.CreateDepTable("Libpqxx", {})

Solution.Util.CreateStaticLib(dep.Name, Solution.Projects.Current.BinDir, dep.Dependencies, function()
    local defines = { "_SILENCE_ALL_CXX17_DEPRECATION_WARNINGS", "_SILENCE_ALL_MS_EXT_DEPRECATION_WARNINGS" }

    Solution.Util.SetLanguage("C++")
    Solution.Util.SetCppDialect(20)

    local baseDir = dep.Path .. "/Libpqxx"
    local sourceDir = baseDir .. "/src"

    -- Include directories setup
    local pqInclude = {}
    if os.target() == "windows" then
        table.insert(pqInclude, postgresRoot .. "/include")
    else
        table.insert(pqInclude, pgInclude)
        table.insert(pqInclude, pgIncludeServer)
    end

    local includeDirs = { baseDir .. "/include", table.unpack(pqInclude) }
    local files =
    {
        sourceDir .. "/**.cxx",
    }

    Solution.Util.SetFiles(files)
    Solution.Util.SetIncludes(includeDirs)
    Solution.Util.SetDefines(defines)

    -- Library link setup
    local libPath = ""
    if os.target() == "windows" then
        libPath = postgresRoot .. "/lib/libpq.lib"
    else
        libPath = "pq"  -- on Linux the system linker will handle finding libpq based on pg_config
        Solution.Util.SetLibDirs({pgLibDir})
    end
    Solution.Util.SetLinks(libPath)
end)

Solution.Util.CreateDep(dep.NameLow, dep.Dependencies, function()
    local baseDir = dep.Path .. "/Libpqxx"

    Solution.Util.SetIncludes({baseDir .. "/include"})

    if os.target() == "windows" then
        libPath = postgresRoot .. "/lib/libpq.lib"
    else
        libPath = "pq"
    end
    Solution.Util.SetLinks({ dep.Name, libPath })
end)
