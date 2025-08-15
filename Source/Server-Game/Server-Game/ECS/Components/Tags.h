#pragma once
#include <Base/Types.h>

namespace ECS::Tags
{
    struct CharacterNeedsInitialization {};
    struct CharacterNeedsDeinitialization {};
    struct CharacterNeedsContainerUpdate {};
    struct CharacterNeedsDisplayUpdate {};
}