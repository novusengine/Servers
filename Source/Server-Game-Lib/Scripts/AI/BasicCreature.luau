if (IsStateGlobal()) then return end

local creatureAIHandler = require("@src/API/CreatureAIHandler")

local scriptNameHash = creatureAIHandler:RegisterScriptInfo("basic_creature")
if not scriptNameHash then return end

local function OnInit(unit : Unit)
end

local function OnDeinit(unit : Unit) : ()
end

local function OnEnterCombat(unit : Unit) : ()
end

local function OnLeaveCombat(unit : Unit) : ()
end

local function OnUpdate(unit : Unit, deltaTime : number) : ()
end

local function OnResurrect(unit : Unit, resurrector : Unit?) : ()
end

local function OnDied(unit : Unit, killer : Unit?) : ()
end

creatureAIHandler:RegisterScriptInitCallback(scriptNameHash, OnInit)
creatureAIHandler:RegisterScriptDeinitCallback(scriptNameHash, OnDeinit)
creatureAIHandler:RegisterScriptEnterCombatCallback(scriptNameHash, OnEnterCombat)
creatureAIHandler:RegisterScriptLeaveCombatCallback(scriptNameHash, OnLeaveCombat)
creatureAIHandler:RegisterScriptUpdateCallback(scriptNameHash, OnUpdate)
creatureAIHandler:RegisterScriptResurrectCallback(scriptNameHash, OnResurrect)
creatureAIHandler:RegisterScriptDiedCallback(scriptNameHash, OnDied)