-- Modules
Solution.Util.Print("-- Creating Modules --")
Solution.Util.ClearFilter()

if Solution.Projects.Current.IsRoot then
    Solution.Util.SetGroupRaw(Solution.BuildSystemGroup)
    include("Generate/Generate.lua")
end

Solution.Util.SetGroup(Solution.ModuleGroup)
local modules =
{
    "Server-Common/Server-Common.lua",
    "Server-Game/Server-Game.lua"
}

for _, v in pairs(modules) do
    include(v)
    Solution.Util.ClearFilter()
end

Solution.Util.SetGroup(Solution.TestGroup)
local tests =
{
    --"Game-Tests/Game-Tests.lua"
}

for _, v in pairs(tests) do
    include(v)
    Solution.Util.ClearFilter()
end

Solution.Util.SetGroup("")
Solution.Util.Print("-- Finished with Modules --\n")