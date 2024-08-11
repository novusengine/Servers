#pragma once
#include <Base/Types.h>
#include <Base/Container/ConcurrentQueue.h>
#include <Base/Memory/Bytebuffer.h>

#include <memory>
#include <mutex>

#include <entt/fwd.hpp>
#include <robinhood/robinhood.h>

namespace ECS::Singletons
{
    struct GridUpdateFlag
    {
        u32 SendToSelf : 1 = 0;
    };

    struct GridUpdate
    {
    public:
        entt::entity owner;
        GridUpdateFlag flags;

        std::shared_ptr<Bytebuffer> buffer;
    };

    struct GridCellEntityList
    {
    public:
        robin_hood::unordered_set<entt::entity> list;
        robin_hood::unordered_set<entt::entity> entering;
        robin_hood::unordered_set<entt::entity> leaving;
    };

    struct GridCell
    {
    public:
        GridCellEntityList players;
        moodycamel::ConcurrentQueue<GridUpdate> updates;
    };

    struct GridSingleton
    {
    public:
        GridCell cell;
    };
}