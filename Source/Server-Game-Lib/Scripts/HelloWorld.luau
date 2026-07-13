if (IsStateGlobal()) then return end

@native
local function OnServerEventLoaded(eventID : ServerEvent, data : any)
    local mapID, instanceID, variantID = GetStateIDs()
    print(`Loaded State : (MapID: {mapID}, InstanceID: {instanceID}, VariantID: {variantID})`)
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
RegisterEvent(PacketListEnum.ClientConnectPacket, OnClientConnect)

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