#pragma once
#include "Common.h"
class Player;
class Creature;
class WorldScript
{
public:
    explicit WorldScript(char const*) { }
    virtual ~WorldScript() = default;
    virtual void OnAfterConfigLoad(bool) { }
    virtual void OnStartup() { }
    virtual void OnShutdown() { }
};
class CreatureScript
{
public:
    explicit CreatureScript(char const*) { }
    virtual ~CreatureScript() = default;
    virtual bool OnGossipHello(Player*, Creature*) { return false; }
    virtual bool OnGossipSelect(Player*, Creature*, uint32, uint32) { return false; }
};
class PlayerScript
{
public:
    explicit PlayerScript(char const*) { }
    virtual ~PlayerScript() = default;
    virtual void OnPlayerLevelChanged(Player*, uint8) { }
    virtual void OnPlayerLogin(Player*) { }
};
