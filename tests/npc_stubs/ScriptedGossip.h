#pragma once
#include "Common.h"
#include <string>
#include <vector>
class Player;
constexpr uint32 GOSSIP_ACTION_INFO_DEF = 1000;
constexpr uint32 GOSSIP_SENDER_MAIN = 1;
constexpr uint32 GOSSIP_ICON_CHAT = 0;
constexpr uint32 GOSSIP_ICON_BATTLE = 9;
struct TestGossipItem { std::string label; uint32 action; };
inline std::vector<TestGossipItem> testGossip;
inline bool testClosed = false;
inline unsigned testMenusOpened = 0;
inline void ClearGossipMenuFor(Player*) { testGossip.clear(); }
inline void AddGossipItemFor(Player*, uint32, std::string const& text, uint32, uint32 action)
{
    testGossip.push_back({text, action});
}
inline void SendGossipMenuFor(Player*, uint32, uint32) { testClosed = false; ++testMenusOpened; }
inline void CloseGossipMenuFor(Player*) { testClosed = true; }
