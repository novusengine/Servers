#include "ConsoleCommands.h"
#include "Application.h"
#include "Message.h"

#include <Server-Game/ECS/Singletons/GameCache.h>
#include <Server-Game/ECS/Singletons/TimeState.h>
#include <Server-Game/Util/ServiceLocator.h>

#include <Server-Common/Database/DatabaseService.h>

#include <Base/Util/DebugHandler.h>

#include <entt/entt.hpp>
#include <libsodium/sodium.h>
#include <spake2-ee/crypto_spake.h>

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

    auto createResult = gameCache.database->CreateAccount(username, email, registrationTimestamp,
        stored_data, crypto_spake_STOREDBYTES);
    if (!createResult)
        return;

    NC_LOG_INFO("Successfully added account '{0}'", username);
}
