#pragma once
#include <Base/Types.h>

#include <Gameplay/GameDefine.h>

#include <robinhood/robinhood.h>

namespace Database
{
    struct CharacterDefinition
    {
    public:
        u64 id = std::numeric_limits<u64>().max();
        u32 accountid = 0;
        std::string name = "";
        u32 totalTime = 0;
        u32 levelTime = 0;
        u32 logoutTime = 0;
        u32 flags = 0;
        u16 raceGenderClass = 0;
        u16 level = 0;
        u64 experiencePoints = 0;
        u16 mapID = 0;
        vec3 position = vec3(0.0f);
        f32 orientation = 0.0f;

    public:
        GameDefine::UnitRace GetRace() const { return static_cast<GameDefine::UnitRace>(raceGenderClass & 0x7F); }
        void SetRace(GameDefine::UnitRace race) { raceGenderClass = (raceGenderClass & 0xFF80) | static_cast<u16>(race); }

        GameDefine::Gender GetGender() const { return static_cast<GameDefine::Gender>((raceGenderClass >> 7) & 0x3); }
        void SetGender(GameDefine::Gender gender) { raceGenderClass = (raceGenderClass & 0xFE7F) | (static_cast<u16>(gender) << 7); }

        GameDefine::UnitClass GetGameClass() const { return static_cast<GameDefine::UnitClass>((raceGenderClass >> 9) & 0x7f); }
        void SetGameClass(GameDefine::UnitClass gameClass) { raceGenderClass = (raceGenderClass & 0x1FF) | (static_cast<u16>(gameClass) << 9); }
    };
    struct CharacterCurrency
    {
    public:
        u16 currencyID = 0;
        u64 value = 0;
    };

    struct Permission
    {
    public:
        u16 id = 0;
        std::string name;
    };
    struct PermissionGroup
    {
    public:
        u16 id = 0;
        std::string name;
    };
    struct PermissionGroupData
    {
    public:
        u16 groupID = 0;
        u16 permissionID = 0;
    };

    struct Currency
    {
    public:
        u16 id = 0;
        std::string name;
    };

    struct PermissionTables
    {
    public:
        robin_hood::unordered_map<u16, Database::Permission> idToDefinition;
        robin_hood::unordered_map<u16, Database::PermissionGroup> groupIDToDefinition;
        robin_hood::unordered_map<u16, std::vector<Database::PermissionGroupData>> groupIDToData;
    };

    struct CurrencyTables
    {
    public:
        robin_hood::unordered_map<u16, Database::Currency> idToDefinition;
    };

    struct CharacterTables
    {
    public:
        robin_hood::unordered_map<u64, Database::CharacterDefinition> idToDefinition;
        robin_hood::unordered_map<u32, u64> nameHashToID;
        robin_hood::unordered_map<u64, std::vector<u16>> idToPermissions;
        robin_hood::unordered_map<u64, std::vector<u16>> idToPermissionGroups;
        robin_hood::unordered_map<u64, std::vector<CharacterCurrency>> idToCurrency;
    };
}