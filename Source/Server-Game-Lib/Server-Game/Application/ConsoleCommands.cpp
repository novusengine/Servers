#include "ConsoleCommands.h"
#include "Application.h"
#include "Message.h"

#include <Server-Game/ECS/Components/PermissionInfo.h>
#include <Server-Game/ECS/Singletons/GameCache.h>
#include <Server-Game/ECS/Singletons/TimeState.h>
#include <Server-Game/ECS/Util/PermissionUtil.h>
#include <Server-Game/Gameplay/Faction/FactionClientDBExport.h>
#include <Server-Game/Util/ServiceLocator.h>

#include <Server-Common/Database/DatabaseService.h>

#include <Base/Util/DebugHandler.h>

#include <entt/entt.hpp>
#include <libsodium/sodium.h>
#include <spake2-ee/crypto_spake.h>

#include <algorithm>
#include <charconv>

namespace
{
    void LogPermissionUsage()
    {
        NC_LOG_ERROR("Usage: permission <account|character> <name> <show|set|remove|group>");
        NC_LOG_ERROR("  permission account <name> set <permission-key> <value>");
        NC_LOG_ERROR("  permission account <name> remove <permission-key>");
        NC_LOG_ERROR("  permission account <name> group <add|remove> <group-key>");
        NC_LOG_ERROR("  The same operations are available with 'character'.");
    }

    bool ParsePermissionValue(const std::string& text, i64& value)
    {
        const char* begin = text.data();
        const char* end = begin + text.size();
        const auto result = std::from_chars(begin, end, value);

        return result.ec == std::errc{} && result.ptr == end;
    }

    bool ResolvePermissionID(const Database::PermissionTables& tables, const std::string& name, u32& permissionID)
    {
        auto itr = tables.permissionNameToID.find(name);
        if (itr == tables.permissionNameToID.end())
        {
            NC_LOG_ERROR("Unknown permission '{0}'", name);
            return false;
        }

        permissionID = itr->second;
        return true;
    }

    bool ResolvePermissionGroupID(const Database::PermissionTables& tables, const std::string& name, u32& permissionGroupID)
    {
        auto itr = tables.permissionGroupNameToID.find(name);
        if (itr == tables.permissionGroupNameToID.end())
        {
            NC_LOG_ERROR("Unknown permission group '{0}'", name);
            return false;
        }

        permissionGroupID = itr->second;
        return true;
    }

    bool ValidatePermissionGroupAncestry(u32 permissionGroupID, const Database::PermissionAssignmentSnapshot& assignments, const Database::PermissionTables& tables)
    {
        if (assignments.defaultGroupApplied)
            return true;

        using PermissionGroup = MetaGen::Server::Permission::PermissionGroup;
        const PermissionGroup candidate = static_cast<PermissionGroup>(permissionGroupID);
        for (u32 existingGroupID : assignments.permissionGroupIDs)
        {
            if (existingGroupID == permissionGroupID)
                continue;

            const PermissionGroup existing = static_cast<PermissionGroup>(existingGroupID);
            if (!MetaGen::Server::Permission::GroupInherits(candidate, existing) &&
                !MetaGen::Server::Permission::GroupInherits(existing, candidate))
            {
                continue;
            }

            auto candidateNameItr = tables.permissionGroupIDToName.find(permissionGroupID);
            auto existingNameItr = tables.permissionGroupIDToName.find(existingGroupID);
            const std::string candidateName = candidateNameItr == tables.permissionGroupIDToName.end() ? std::to_string(permissionGroupID) : candidateNameItr->second;
            const std::string existingName = existingNameItr == tables.permissionGroupIDToName.end() ? std::to_string(existingGroupID) : existingNameItr->second;

            NC_LOG_ERROR("Cannot combine permission groups '{0}' and '{1}' because one inherits the other", candidateName, existingName);
            return false;
        }

        return true;
    }

    template <typename T>
    bool ReportPermissionDatabaseResult(const Database::OperationResult<T>& result, std::string_view operation)
    {
        if (result)
            return true;
        NC_LOG_ERROR("Permission operation '{0}' failed ({1})", operation,
            Database::OperationFailureName(result.Failure()));
        return false;
    }

    bool ReportPermissionDeleteResult(const Database::OperationResult<Database::DeleteOutcome>& result, std::string_view operation)
    {
        if (!ReportPermissionDatabaseResult(result, operation))
            return false;

        if (result.Value() == Database::DeleteOutcome::AlreadyAbsent)
        {
            NC_LOG_INFO("Permission operation '{0}' made no change because no explicit assignment existed", operation);
            return false;
        }

        return true;
    }

    void LogPermissionAssignments(std::string_view source, const Database::PermissionAssignmentSnapshot& snapshot, const Database::PermissionTables& tables)
    {
        if (snapshot.permissionGroupIDs.empty() && snapshot.permissions.empty())
        {
            NC_LOG_INFO("  {0}: no assignments", source);
            return;
        }

        for (u32 groupID : snapshot.permissionGroupIDs)
        {
            auto itr = tables.permissionGroupIDToName.find(groupID);
            const std::string groupName = itr == tables.permissionGroupIDToName.end() ? "unknown:" + std::to_string(groupID) : itr->second;
            NC_LOG_INFO("  {0} group: {1}{2}", source, groupName, snapshot.defaultGroupApplied ? " (implicit default)" : "");
        }
        for (const Database::PermissionAssignment& assignment : snapshot.permissions)
        {
            auto itr = tables.permissionIDToName.find(assignment.permissionID);
            const std::string permissionName = itr == tables.permissionIDToName.end() ? "unknown:" + std::to_string(assignment.permissionID) : itr->second;
            NC_LOG_INFO("  {0} direct: {1} = {2}", source, permissionName, assignment.value);
        }
    }

    void LogEffectivePermissions(const ECS::Components::PermissionSet& permissions, const Database::PermissionTables& tables)
    {
        std::vector<std::pair<u32, i64>> sortedPermissions;
        sortedPermissions.reserve(permissions.values.size());
        for (const auto& [permissionID, value] : permissions.values)
        {
            sortedPermissions.emplace_back(permissionID, value);
        }

        std::sort(sortedPermissions.begin(), sortedPermissions.end(), [](const auto& left, const auto& right)
        {
            return left.first < right.first;
        });

        if (sortedPermissions.empty())
        {
            NC_LOG_INFO("  Effective: no non-default values");
            return;
        }

        for (const auto& [permissionID, value] : sortedPermissions)
        {
            auto itr = tables.permissionIDToName.find(permissionID);
            const std::string permissionName = itr == tables.permissionIDToName.end() ? "unknown:" + std::to_string(permissionID) : itr->second;
            NC_LOG_INFO("  Effective: {0} = {1}", permissionName, value);
        }
    }
}

void ConsoleCommands::CommandPrint(Application& app, std::vector<std::string>& subCommands)
{
    if (subCommands.size() == 0)
        return;

    MessageInbound message(MessageInbound::Type::Print);

    for (u32 i = 0; i < subCommands.size(); i++)
    {
        if (i > 0)
        {
            message.data += " ";
        }

        message.data += subCommands[i];
    }

    app.PassMessage(message);
}

void ConsoleCommands::CommandPing(Application& app, std::vector<std::string>& subCommands)
{
    MessageInbound message(MessageInbound::Type::Ping);
    app.PassMessage(message);
}

void ConsoleCommands::CommandExit(Application& app, std::vector<std::string>& subCommands)
{
    MessageInbound message(MessageInbound::Type::Exit);
    app.PassMessage(message);
}

void ConsoleCommands::CommandDoString(Application& app, std::vector<std::string>& subCommands)
{
    if (subCommands.size() == 0)
        return;

    MessageInbound message(MessageInbound::Type::DoString);

    for (u32 i = 0; i < subCommands.size(); i++)
    {
        if (i > 0)
        {
            message.data += " ";
        }

        message.data += subCommands[i];
    }

    app.PassMessage(message);
}

void ConsoleCommands::CommandReloadScripts(Application& app, std::vector<std::string>& subCommands)
{
    MessageInbound message(MessageInbound::Type::ReloadScripts);
    app.PassMessage(message);
}

void ConsoleCommands::CommandAddAccount(Application& app, std::vector<std::string>& subCommands)
{
    std::string username = "";
    std::string password = "";

    u32 numSubCommands = static_cast<u32>(subCommands.size());
    if (numSubCommands < 2 || numSubCommands > 3)
    {
        NC_LOG_ERROR("Usage: addaccount <username> <password> <email?>");
        return;
    }

    username = subCommands[0];
    password = subCommands[1];

    if (username.length() < 2 || password.length() == 2)
    {
        NC_LOG_ERROR("Failed to add account : Username and/or password must be at least 2 characters in length");
        return;
    }

    if (username.length() > 32 || password.length() > 128)
    {
        NC_LOG_ERROR("Failed to add account : Username and/or password exceed maximum length of 32/128 characters respectively");
        return;
    }

    std::string email = numSubCommands == 3 ? subCommands[2] : "";
    if (email.length() > 128)
    {
        NC_LOG_ERROR("Failed to add account : Email exceeds maximum length of 128 characters");
        return;
    }

    unsigned char stored_data[crypto_spake_STOREDBYTES];
    i32 result = crypto_spake_server_store(stored_data, password.c_str(), password.length(), crypto_pwhash_OPSLIMIT_INTERACTIVE, crypto_pwhash_MEMLIMIT_INTERACTIVE);

    // Clear password from memory
    sodium_memzero(password.data(), password.length());
    password.clear();

    if (result != 0)
    {
        NC_LOG_ERROR("Failed to add account : Spake2 Data could not be generated.");
        return;
    }

    entt::registry* registry = ServiceLocator::GetEnttRegistries()->gameRegistry;
    auto& gameCache = registry->ctx().get<ECS::Singletons::GameCache>();
    auto& timeState = registry->ctx().get<ECS::Singletons::TimeState>();
    u64 registrationTimestamp = timeState.epochAtFrameStart;

    auto createResult = gameCache.database->CreateAccount(username, email, registrationTimestamp, stored_data, crypto_spake_STOREDBYTES);
    if (!createResult)
        return;

    NC_LOG_INFO("Successfully added account '{0}'", username);
}

void ConsoleCommands::CommandPermission(Application& app, std::vector<std::string>& subCommands)
{
    MessageInbound message(MessageInbound::Type::PermissionCommand);

    for (u32 index = 0; index < subCommands.size(); ++index)
    {
        if (index > 0)
            message.data += " ";

        message.data += subCommands[index];
    }

    app.PassMessage(message);
}

void ConsoleCommands::CommandExportFactions(Application& app, std::vector<std::string>& subCommands)
{
    if (subCommands.size() != 1)
    {
        NC_LOG_ERROR("Usage: exportfactions <patch-staging-directory>");
        return;
    }

    MessageInbound message(MessageInbound::Type::FactionClientDBExport, subCommands.front());
    app.PassMessage(message);
}

void ConsoleCommands::ExecuteExportFactions(const std::string& stagingDirectory)
{
    entt::registry* registry = ServiceLocator::GetEnttRegistries()->gameRegistry;
    auto& gameCache = registry->ctx().get<ECS::Singletons::GameCache>();
    if (!gameCache.factionRuntimeData)
    {
        NC_LOG_ERROR("Faction ClientDB export is unavailable until World faction data has loaded");
        return;
    }

    Gameplay::Faction::ClientDBExportResult result;
    if (!Gameplay::Faction::ExportClientDB(gameCache.factionTables, *gameCache.factionRuntimeData, stagingDirectory, result))
    {
        NC_LOG_ERROR("Faction ClientDB export failed");
        return;
    }

    NC_LOG_INFO("Exported faction ClientDBs to '{}' (factions {}, relations {}, standings {})", stagingDirectory, result.factionCount, result.relationCount, result.standingCount);
}

void ConsoleCommands::ExecutePermission(std::vector<std::string>& subCommands)
{
    if (subCommands.size() < 3)
    {
        LogPermissionUsage();
        return;
    }

    const std::string& scope = subCommands[0];
    const std::string& targetName = subCommands[1];
    const std::string& action = subCommands[2];
    if (scope != "account" && scope != "character")
    {
        LogPermissionUsage();
        return;
    }

    entt::registry* registry = ServiceLocator::GetEnttRegistries()->gameRegistry;
    auto& gameCache = registry->ctx().get<ECS::Singletons::GameCache>();
    Database::DatabaseService& database = *gameCache.database;
    const Database::PermissionTables& tables = gameCache.permissionTables;

    u64 targetID = 0;
    u64 accountID = 0;
    if (scope == "account")
    {
        auto accountResult = database.GetAccountByName(targetName);
        if (!ReportPermissionDatabaseResult(accountResult, "find account"))
            return;

        if (!accountResult.Value())
        {
            NC_LOG_ERROR("Account '{0}' was not found", targetName);
            return;
        }

        targetID = accountResult.Value()->id;
        accountID = targetID;
    }
    else
    {
        auto characterResult = database.GetCharacterByName(targetName);
        if (!ReportPermissionDatabaseResult(characterResult, "find character"))
            return;

        if (!characterResult.Value())
        {
            NC_LOG_ERROR("Character '{0}' was not found", targetName);
            return;
        }

        targetID = characterResult.Value()->id;
        accountID = characterResult.Value()->accountId;
    }

    if (action == "show")
    {
        if (subCommands.size() != 3)
        {
            LogPermissionUsage();
            return;
        }

        auto accountPermissionsResult = database.GetAccountPermissions(accountID);
        if (!ReportPermissionDatabaseResult(accountPermissionsResult, "load account permissions"))
            return;

        ECS::Components::PermissionSet effectivePermissions;
        ECS::Util::Permission::Merge(effectivePermissions, tables, accountPermissionsResult.Value());
        NC_LOG_INFO("Permission assignments for {0} '{1}':", scope, targetName);
        LogPermissionAssignments("Account", accountPermissionsResult.Value(), tables);

        if (scope == "character")
        {
            auto characterPermissionsResult = database.GetCharacterPermissions(targetID);
            if (!ReportPermissionDatabaseResult(characterPermissionsResult, "load character permissions"))
                return;

            LogPermissionAssignments("Character", characterPermissionsResult.Value(), tables);
            ECS::Util::Permission::Merge(effectivePermissions, tables, characterPermissionsResult.Value());
        }

        LogEffectivePermissions(effectivePermissions, tables);
    }

    bool succeeded = false;
    if (action == "set" && subCommands.size() == 5)
    {
        u32 permissionID = 0;
        i64 value = 0;
        if (!ResolvePermissionID(tables, subCommands[3], permissionID) ||
            !ParsePermissionValue(subCommands[4], value))
        {
            if (!ParsePermissionValue(subCommands[4], value))
                NC_LOG_ERROR("Permission value '{0}' is not a valid integer", subCommands[4]);

            return;
        }

        auto valueKindItr = tables.permissionIDToValueKind.find(permissionID);
        if (valueKindItr != tables.permissionIDToValueKind.end() && valueKindItr->second == 0 && value != 0 && value != 1)
        {
            NC_LOG_ERROR("Boolean permission '{0}' only accepts values 0 and 1", subCommands[3]);
            return;
        }

        if (scope == "account")
        {
            auto result = database.SetAccountPermission(targetID, permissionID, value);
            succeeded = ReportPermissionDatabaseResult(result, "set account permission");
        }
        else
        {
            auto result = database.SetCharacterPermission(targetID, permissionID, value);
            succeeded = ReportPermissionDatabaseResult(result, "set character permission");
        }
    }
    else if (action == "remove" && subCommands.size() == 4)
    {
        u32 permissionID = 0;
        if (!ResolvePermissionID(tables, subCommands[3], permissionID))
            return;

        if (scope == "account")
        {
            auto result = database.DeleteAccountPermission(targetID, permissionID);
            succeeded = ReportPermissionDeleteResult(result, "remove account permission");
        }
        else
        {
            auto result = database.DeleteCharacterPermission(targetID, permissionID);
            succeeded = ReportPermissionDeleteResult(result, "remove character permission");
        }
    }
    else if (action == "group" && subCommands.size() == 5)
    {
        const std::string& groupAction = subCommands[3];
        u32 permissionGroupID = 0;
        if ((groupAction != "add" && groupAction != "remove") || !ResolvePermissionGroupID(tables, subCommands[4], permissionGroupID))
        {
            LogPermissionUsage();
            return;
        }

        if (groupAction == "add")
        {
            auto accountGroupsResult = database.GetAccountPermissions(accountID);
            if (!ReportPermissionDatabaseResult(accountGroupsResult, "validate account permission groups") ||
                !ValidatePermissionGroupAncestry(permissionGroupID, accountGroupsResult.Value(), tables))
            {
                return;
            }
            if (scope == "character")
            {
                auto characterGroupsResult = database.GetCharacterPermissions(targetID);
                if (!ReportPermissionDatabaseResult(characterGroupsResult, "validate character permission groups") ||
                    !ValidatePermissionGroupAncestry(permissionGroupID, characterGroupsResult.Value(), tables))
                {
                    return;
                }
            }
            else
            {
                auto charactersResult = database.GetCharactersByAccount(accountID);
                if (!ReportPermissionDatabaseResult(charactersResult, "find account characters"))
                    return;

                for (const auto& character : charactersResult.Value())
                {
                    auto characterGroupsResult = database.GetCharacterPermissions(character.id);
                    if (!ReportPermissionDatabaseResult(characterGroupsResult, "validate character permission groups") ||
                        !ValidatePermissionGroupAncestry(permissionGroupID, characterGroupsResult.Value(), tables))
                    {
                        return;
                    }
                }
            }
        }

        if (scope == "account" && groupAction == "add")
        {
            auto result = database.AddAccountPermissionGroup(targetID, permissionGroupID);
            succeeded = ReportPermissionDatabaseResult(result, "add account permission group");
        }
        else if (scope == "account")
        {
            auto result = database.DeleteAccountPermissionGroup(targetID, permissionGroupID);
            succeeded = ReportPermissionDeleteResult(result, "remove account permission group");
        }
        else if (groupAction == "add")
        {
            auto result = database.AddCharacterPermissionGroup(targetID, permissionGroupID);
            succeeded = ReportPermissionDatabaseResult(result, "add character permission group");
        }
        else
        {
            auto result = database.DeleteCharacterPermissionGroup(targetID, permissionGroupID);
            succeeded = ReportPermissionDeleteResult(result, "remove character permission group");
        }
    }
    else
    {
        LogPermissionUsage();
        return;
    }

    if (succeeded)
        NC_LOG_INFO("Permission assignment updated for {0} '{1}'; reconnect to refresh effective permissions", scope, targetName);
}
