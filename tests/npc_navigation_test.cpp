// Exercise the real gossip script with a minimal core interface and a fake manager.
#include "../src/PrestigioHollowScripts.cpp"
#include <algorithm>
#include <cassert>
#include <iostream>

namespace
{
bool enabled = true;
bool completePrestige = true;
unsigned prestigeCalls = 0;
}
PrestigioHollowMgr* PrestigioHollowMgr::instance() { static PrestigioHollowMgr manager; return &manager; }
void PrestigioHollowMgr::LoadConfig() { _enabled = enabled; }
uint32 PrestigioHollowMgr::GetPrestigeCount(uint32) const { return 3; }
bool PrestigioHollowMgr::CanPrestige(Player* player) const { return _enabled && player->GetLevel() >= _requiredLevel; }
bool PrestigioHollowMgr::Prestige(Player* player) { ++prestigeCalls; return CanPrestige(player) && completePrestige; }

bool HasAction(uint32 action)
{
    return std::any_of(testGossip.begin(), testGossip.end(), [action](TestGossipItem const& item) { return item.action == action; });
}
int main()
{
    Player player;
    Creature creature;
    npc_prestigio_hollow npc;
    auto select = [&](uint32 action) { return npc.OnGossipSelect(&player, &creature, GOSSIP_SENDER_MAIN, action); };

    assert(npc.OnGossipHello(&player, &creature));
    assert(HasAction(PH_PRESTIGE) && HasAction(PH_CLOSE));
    assert(select(PH_PRESTIGE));
    assert(HasAction(PH_CONFIRM) && HasAction(PH_BACK));
    assert(select(PH_BACK));
    assert(HasAction(PH_PRESTIGE) && !HasAction(PH_CONFIRM) && prestigeCalls == 0);
    assert(select(PH_CLOSE) && testClosed);

    player.level = 1;
    assert(npc.OnGossipHello(&player, &creature));
    assert(HasAction(PH_PRESTIGE)); // show the action and required level instead of hiding it
    assert(select(PH_PRESTIGE));
    assert(!HasAction(PH_CONFIRM) && prestigeCalls == 0);

    player.level = 80;
    assert(select(PH_PRESTIGE));
    completePrestige = false;
    assert(select(PH_CONFIRM));
    assert(!testClosed && HasAction(PH_PRESTIGE) && prestigeCalls == 1);
    completePrestige = true;
    assert(select(PH_PRESTIGE));
    assert(select(PH_CONFIRM) && testClosed && prestigeCalls == 2);

    assert(!npc.OnGossipSelect(&player, &creature, 999, PH_CONFIRM));
    assert(!select(999999) && prestigeCalls == 2);
    enabled = false;
    sPrestigioHollowMgr->LoadConfig();
    assert(npc.OnGossipHello(&player, &creature));
    assert(!HasAction(PH_PRESTIGE) && HasAction(PH_CLOSE));
    std::cout << "PASS: NPC main/confirm/back/close, low level, failed prestige, disabled mode and invalid actions\n";
}
