#pragma once
#include "Server-Game/ECS/Components/Events.h"
#include "Server-Game/ECS/Singletons/WorldState.h"

#include <Base/Types.h>

#include <Gameplay/Network/NetFields.h>

#include <entt/entt.hpp>

namespace ECS::Util::NetFields
{
    template <typename TEnum, typename T> requires std::is_enum_v<TEnum> && std::is_same_v<std::underlying_type_t<TEnum>, u16> && ::Network::HasMetaEnumTraits<TEnum> && std::is_trivially_copyable_v<std::decay_t<T>>
    void SetField(World& world, entt::entity entity, ::Network::NetFields<TEnum>& fields, TEnum startField, T&& value, u8 fieldByteOffset = 0, u8 fieldBitOffset = 0, u8 bitCount = sizeof(std::decay_t<T>) * 8)
    {
        fields.SetField(startField, std::forward<T>(value), fieldByteOffset, fieldBitOffset, bitCount);

        if (!world.AllOf<Events::ObjectNeedsNetFieldUpdate>(entity))
        {
            world.Emplace<Events::ObjectNeedsNetFieldUpdate>(entity);
        }
    }
}