#ifndef MOD_PRESTIGIO_HOLLOW_H
#define MOD_PRESTIGIO_HOLLOW_H

#include "Common.h"
#include <unordered_set>
class Player;

class PrestigioHollowMgr
{
public:
    static PrestigioHollowMgr* instance();
    void LoadConfig();
    bool IsEnabled() const { return _enabled; }
    uint32 GetRequiredLevel() const { return _requiredLevel; }
    uint32 GetPrestigeCount(uint32 guidLow) const;
    bool Prestige(Player* player);

private:
    bool StoreEquippedItems(Player* player) const;
    void ClearQuestProgress(Player* player) const;
    void RecordPrestigeHistory(Player* player, uint32 prestigeNumber, uint8 previousLevel) const;
    std::unordered_set<uint32> SnapshotPermanentSpells(Player* player) const;
    void RestorePermanentSpells(Player* player, std::unordered_set<uint32> const& spells) const;
    void GiveStartingOutfit(Player* player) const;
    void TeleportToRaceStart(Player* player) const;

    bool _enabled{true};
    uint32 _requiredLevel{80};
    bool _announceWorld{true};
    bool _giveStartingOutfit{true};
    bool _preservePermanentSpells{true};
};
#define sPrestigioHollowMgr PrestigioHollowMgr::instance()
#endif
