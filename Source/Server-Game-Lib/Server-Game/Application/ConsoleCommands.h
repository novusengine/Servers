#pragma once
#include <vector>
#include <string>

class Application;
class ConsoleCommands
{
public:
    static void CommandPrint(Application& app, std::vector<std::string>& subCommands);
    static void CommandPing(Application& app, std::vector<std::string>& subCommands);
    static void CommandExit(Application& app, std::vector<std::string>& subCommands);
    static void CommandDoString(Application& app, std::vector<std::string>& subCommands);
    static void CommandReloadScripts(Application& app, std::vector<std::string>& subCommands);
    static void CommandAddAccount(Application& app, std::vector<std::string>& subCommands);
    static void CommandPermission(Application& app, std::vector<std::string>& subCommands);
    static void CommandExportFactions(Application& app, std::vector<std::string>& subCommands);
    static void ExecutePermission(std::vector<std::string>& subCommands);
    static void ExecuteExportFactions(const std::string& stagingDirectory);
};
