#pragma once
#include "Common.h"
struct TestGuid { uint32 GetCounter() const { return 42; } };
class Player
{
public:
    uint32 level = 80;
    uint32 GetLevel() const { return level; }
    TestGuid GetGUID() const { return {}; }
    void* GetSession() const { return nullptr; }
};
