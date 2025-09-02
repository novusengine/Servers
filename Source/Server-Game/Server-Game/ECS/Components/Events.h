#pragma once
#include <Base/Types.h>

#include <Gameplay/GameDefine.h>

#include <Meta/Generated/Shared/ProximityTriggerEnum.h>

namespace ECS::Events
{
    struct MapNeedsInitialization {};

    struct CharacterNeedsInitialization {};
    struct CharacterNeedsDeinitialization {};

    struct CharacterNeedsContainerUpdate {};
    struct CharacterNeedsDisplayUpdate {};
    struct CharacterNeedsVisualItemUpdate {};

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
        f32 orientation;
    };

    struct CreatureNeedsInitialization
    {
    public:
        ObjectGUID guid;
        u32 templateID;
        u32 displayID;

        u16 mapID;
        vec3 position;
        f32 orientation;
    };

    struct CreatureNeedsDeinitialization
    {
    public:
        ObjectGUID guid;
    };

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