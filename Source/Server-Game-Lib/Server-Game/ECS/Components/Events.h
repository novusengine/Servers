#pragma once

#include <Base/Types.h>

#include <Gameplay/GameDefine.h>

#include <MetaGen/Shared/ProximityTrigger/ProximityTrigger.h>

#include <entt/fwd.hpp>

namespace ECS::Events
{
    struct MapNeedsInitialization
    {
    public:
        u8 retryCount = 0;
        u64 retryAfterEpoch = 0;
    };

    struct CharacterNeedsInitialization {};
    struct CharacterNeedsDeinitialization {};
    struct CharacterNeedsContainerUpdate {};
    struct CharacterNeedsRecalculateStatsUpdate {};
    struct CharacterNeedsVisualItemUpdate {};
    struct CharacterNeedsStatUpdate {};
    struct CharacterNeedsResistanceUpdate {};

    struct CharacterWorldTransfer
    {
    public:
        u32 targetMapID;
        vec3 targetPosition;
        f32 targetOrientation;
    };

    struct CreatureCreate
    {
    public:
        u32 templateID;
        u32 displayID;

        u32 mapID;
        vec3 position;
        vec3 scale;
        f32 orientation;

        std::string scriptName;
        u32 spawnTimeInSecMin = 120;
        u32 spawnTimeInSecMax = 120;
        f32 wanderDistance = 0.0f;
        u16 movementType = 0;
    };

    struct CreatureNeedsInitialization
    {
    public:
        ObjectGUID guid;
        u32 templateID;
        u32 displayID;

        u32 mapID;
        vec3 position;
        vec3 scale;
        f32 orientation;

        std::string scriptName;
        u32 spawnTimeInSecMin = 120;
        u32 spawnTimeInSecMax = 120;
        f32 wanderDistance = 0.0f;
        u16 movementType = 0;
    };

    struct CreatureNeedsDeinitialization
    {
    public:
        ObjectGUID guid;
    };

    struct CreatureAddScript
    {
    public:
        std::string scriptName;
    };

    struct CreatureRemoveScript {};
    struct CreatureNeedsThreatTableUpdate {};

    struct UnitEnterCombat
    {
    public:
        entt::entity target;
    };

    struct UnitDied
    {
    public:
        entt::entity killerEntity;
    };

    struct UnitResurrected
    {
    public:
        entt::entity resurrectorEntity;
    };

    struct UnitNeedsPowerUpdate {};
    struct UnitNeedsFactionUpdate {};

    struct AuraRefreshed {};
    struct AuraExpired {};

    struct ProximityTriggerCreate
    {
    public:
        std::string name;
        MetaGen::Shared::ProximityTrigger::ProximityTriggerFlagEnum flags;

        u32 mapID;
        vec3 position;
        vec3 extents;
    };

    struct ProximityTriggerNeedsInitialization
    {
    public:
        u32 triggerID;

        std::string name;
        MetaGen::Shared::ProximityTrigger::ProximityTriggerFlagEnum flags;

        u32 mapID;
        vec3 position;
        vec3 extents;
    };

    struct ProximityTriggerNeedsDeinitialization
    {
    public:
        u32 triggerID;
    };
}
