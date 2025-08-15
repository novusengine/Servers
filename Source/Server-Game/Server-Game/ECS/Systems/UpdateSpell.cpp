#include "UpdateSpell.h"

#include "Server-Game/ECS/Components/CastInfo.h"
#include "Server-Game/ECS/Components/UnitStatsComponent.h"
#include "Server-Game/ECS/Components/Tags.h"
#include "Server-Game/ECS/Singletons/NetworkState.h"
#include "Server-Game/ECS/Util/GridUtil.h"
#include "Server-Game/ECS/Util/MessageBuilderUtil.h"
#include "Server-Game/ECS/Util/UnitUtil.h"
#include "Server-Game/Util/ServiceLocator.h"

#include <Base/Util/DebugHandler.h>

#include <Gameplay/GameDefine.h>

#include <Network/Server.h>

#include <entt/entt.hpp>

namespace ECS::Systems
{
    void UpdateSpell::Init(entt::registry& registry)
    {
    }

    void UpdateSpell::Update(entt::registry& registry, f32 deltaTime)
    {
        Singletons::NetworkState& networkState = registry.ctx().get<Singletons::NetworkState>();
        
        //struct CastEvent
        //{
        //    GameDefine::ObjectGuid caster;
        //    GameDefine::ObjectGuid target;
        //};
        //
        //std::vector<CastEvent> castEvents;
        //
        //auto view = registry.view<Components::CastInfo>();
        //view.each([&](entt::entity entity, Components::CastInfo& castInfo)
        //{
        //    castInfo.duration += deltaTime;
        //
        //    if (castInfo.duration >= castInfo.castTime)
        //    {
        //        castEvents.push_back({ castInfo.caster, castInfo.target });
        //    }
        //});

        //for (const CastEvent& castEvent : castEvents)
        //{
        //    GameDefine::ObjectGuid caster = castEvent.caster;
        //    GameDefine::ObjectGuid target = castEvent.target;
        //
        //    Components::UnitStatsComponent& targetStats = registry.get<Components::UnitStatsComponent>(target);
        //    targetStats.currentHealth = glm::max(0.0f, targetStats.currentHealth - 35.0f);
        //    targetStats.healthIsDirty = true;
        //
        //    // Send Grid Message
        //    {
        //        std::shared_ptr<Bytebuffer> damageDealtMessage = Bytebuffer::Borrow<64>();
        //        if (!Util::MessageBuilder::CombatLog::BuildDamageDealtMessage(damageDealtMessage, caster, target, 35.0f))
        //            continue;
        //
        //        ECS::Util::Grid::SendToGrid(caster, damageDealtMessage, { .SendToSelf = true });
        //    }
        //
        //    registry.remove<Components::CastInfo>(caster);
        //}
    }
}