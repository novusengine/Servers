#pragma once
#include <Base/Types.h>

#include <spake2-ee/crypto_spake.h>

namespace ECS
{
    enum class AuthenticationState : u8
    {
        None = 0,
        Step1 = 1,
        Step2 = 2,
        Step3 = 3,
        Failed = 4,
        Completed = 5
    };

    namespace Components
    {
        struct AuthenticationInfo
        {
        public:
            AuthenticationState state = AuthenticationState::None;

            crypto_spake_server_state serverState = {};
            unsigned char blob[crypto_spake_STOREDBYTES];
        };
    }
}