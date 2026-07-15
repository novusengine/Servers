#pragma once

#include "Server-Game/Gameplay/Faction/FactionRuntimeData.h"

#include <filesystem>

namespace Gameplay::Faction
{
    struct ClientDBExportResult
    {
    public:
        u32 factionCount = 0;
        u32 relationCount = 0;
        u32 standingCount = 0;
    };

    bool ExportClientDB(const Database::FactionTables& factionTables, const FactionRuntimeData& runtime, const std::filesystem::path& patchStagingDirectory, ClientDBExportResult& result);
}
