if (IsStateGlobal()) then return end

@native
local function OnServerEventLoaded(eventID : ServerEvent, data : any)
    print(`Message of the day: {data.motd}`)
end
RegisterEvent(ServerEvent.Loaded, OnServerEventLoaded)

local function OnClientConnect(eventID : PacketList, data : any)
    data.packet:Prepare()

    local name = data.packet:GetString()
    if not name then
        print("Failed to read string")
        return false
    end
    
    print(`Client Connecting with Character : {name}`)

    return true
end
RegisterEvent(PacketList.Connect, OnClientConnect)

local function OnCharacterLogin(eventID : CharacterEvent, data : CharacterEventDataOnLogin)
    print(`Character Logged In: {data.character:GetID()}`)
end
RegisterEvent(CharacterEvent.OnLogin, OnCharacterLogin)

local function OnCharacterLogout(eventID : CharacterEvent, data : CharacterEventDataOnLogout)
    print(`Character Logged Out: {data.character:GetID()}`)
end
RegisterEvent(CharacterEvent.OnLogout, OnCharacterLogout)

local function OnCharacterWorldChanged(eventID : CharacterEvent, data : CharacterEventDataOnWorldChanged)
    print(`Character Changed World: {data.character:GetID()}`)
end
RegisterEvent(CharacterEvent.OnWorldChanged, OnCharacterWorldChanged)