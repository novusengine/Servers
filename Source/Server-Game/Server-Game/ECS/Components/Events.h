#pragma once
#include <Base/Types.h>

#include <Gameplay/GameDefine.h>

#include <Meta/Generated/Shared/ProximityTriggerEnum.h>

#include <entt/fwd.hpp>

namespace ECS::Events
{
    struct MapNeedsInitialization {};

    struct CharacterNeedsInitialization {};
    struct CharacterNeedsDeinitialization {};
    struct CharacterNeedsContainerUpdate {};
    struct CharacterNeedsDisplayUpdate {};
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

        u16 mapID;
        vec3 position;
        vec3 scale;
        f32 orientation;

        std::string scriptName;
    };
    struct CreatureNeedsInitialization
    {
    public:
        ObjectGUID guid;
        u32 templateID;
        u32 displayID;

        u16 mapID;
        vec3 position;
        vec3 scale;
        f32 orientation;

        std::string scriptName;
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
        f32 timeToLeaveCombat;
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

    struct AuraRefreshed {};
    struct AuraExpired {};

    struct ProximityTriggerCreate
    {
    public:
        std::string name;
        Generated::ProximityTriggerFlagEnum flags;

        u16 mapID;
        vec3 position;
        vec3 extents;
    };
    struct ProximityTriggerNeedsInitialization
    {
    public:
        u32 triggerID;

        std::string name;
        Generated::ProximityTriggerFlagEnum flags;

        u16 mapID;
        vec3 position;
        vec3 extents;
    };
    struct ProximityTriggerNeedsDeinitialization
    {
    public:
        u32 triggerID;
    };
}