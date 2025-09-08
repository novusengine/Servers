if (IsStateGlobal()) then return end

@native
local function OnServerEventLoaded(eventID : u32, data : any)
    print(`Message of the day: {data.motd}`)
end
RegisterEvent(ServerEvent.Loaded, OnServerEventLoaded)

local function OnClientConnect(eventID : u32, data : any)
    data.packet:Prepare()

    local name = data.packet:GetString()
    if not name then
        print("Failed to read string")
        return false
    end
    
    print(`Client Connected with Character : {name}`)
    return true
end
RegisterEvent(PacketList.Connect, OnClientConnect)