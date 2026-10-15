#pragma once
#include "Common.h"
class WorldPacket;
enum class LoginFailureReason { DuplicateCharacter };
class WorldSession
{
public:
    void SendCharLoginFailed(LoginFailureReason) { }
    void SendPacket(WorldPacket*) { }
};
