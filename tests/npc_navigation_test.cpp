// Exercise the real gossip script with a minimal core interface and a fake manager.
#include "../src/PrestigioHollowScripts.cpp"
#include <algorithm>
#include "TestCheck.h"
#include <iostream>

namespace
{
bool enabled = true;
std::optional<uint32> availableCount=3;
bool pending=false;
bool completePrestige = true;
unsigned prestigeCalls = 0;
}
PrestigioHollowMgr* PrestigioHollowMgr::instance() { static PrestigioHollowMgr manager; return &manager; }
void PrestigioHollowMgr::LoadConfig() { _enabled = enabled; }
void PrestigioHollowMgr::Start() { }
void PrestigioHollowMgr::Stop() { }
void PrestigioHollowMgr::Update() { }
void PrestigioHollowMgr::CancelMenuRequest(uint32) { }
bool PrestigioHollowMgr::IsBusy(uint32) const { return pending; }
void PrestigioHollowMgr::RepairDefaultLanguages(Player*) const { }
void PrestigioHollowMgr::RequestCount(Player* player, Creature* creature, std::function<void(Player*, Creature*, std::optional<uint32>)> render) { render(player,creature,availableCount); }
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

    CHECK(npc.OnGossipHello(&player, &creature));
    CHECK(HasAction(PH_PRESTIGE) && HasAction(PH_CLOSE));
    CHECK(select(PH_PRESTIGE));
    CHECK(HasAction(PH_CONFIRM) && HasAction(PH_BACK));
    CHECK(select(PH_BACK));
    CHECK(HasAction(PH_PRESTIGE) && !HasAction(PH_CONFIRM) && prestigeCalls == 0);
    CHECK(select(PH_CLOSE) && testClosed);

    player.level = 1;
    CHECK(npc.OnGossipHello(&player, &creature));
    CHECK(HasAction(PH_PRESTIGE)); // show the action and required level instead of hiding it
    CHECK(select(PH_PRESTIGE));
    CHECK(!HasAction(PH_CONFIRM) && prestigeCalls == 0);

    player.level = 80;
    CHECK(select(PH_PRESTIGE));
    completePrestige = false;
    CHECK(select(PH_CONFIRM));
    CHECK(!testClosed && HasAction(PH_PRESTIGE) && prestigeCalls == 1);
    completePrestige = true;
    CHECK(select(PH_PRESTIGE));
    CHECK(select(PH_CONFIRM) && testClosed && prestigeCalls == 2);

    CHECK(!npc.OnGossipSelect(&player, &creature, 999, PH_CONFIRM));
    CHECK(!select(999999) && prestigeCalls == 2);
    availableCount=std::nullopt;
    CHECK(npc.OnGossipHello(&player,&creature));
    CHECK(std::any_of(testGossip.begin(),testGossip.end(),[](auto const& item){return item.label=="Contador de prestigios no disponible";}));
    CHECK(std::none_of(testGossip.begin(),testGossip.end(),[](auto const& item){return item.label=="Prestigios actuales: 0";}));
    PrestigioHollowServer server;
    WorldSession session;
    pending=true;
    CHECK(!server.CanPacketReceive(&session,WorldPacket(CMSG_PLAYER_LOGIN)));
    CHECK(!server.CanPacketReceive(&session,WorldPacket(CMSG_CHAR_DELETE)));
    pending=false;
    CHECK(server.CanPacketReceive(&session,WorldPacket(CMSG_PLAYER_LOGIN)));
    CHECK(server.CanPacketReceive(&session,WorldPacket(CMSG_CHAR_DELETE)));
    enabled = false;
    sPrestigioHollowMgr->LoadConfig();
    CHECK(npc.OnGossipHello(&player, &creature));
    CHECK(!HasAction(PH_PRESTIGE) && HasAction(PH_CLOSE));
    std::cout << "PASS: NPC main/confirm/back/close, low level, failed prestige, disabled mode and invalid actions\n";
}
