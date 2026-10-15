#ifndef MOD_PRESTIGIO_HOLLOW_H
#define MOD_PRESTIGIO_HOLLOW_H

#include "Common.h"
#include <unordered_set>
#include <utility>
#include <vector>
class Player;
class Item;

class PrestigioHollowMgr
{
public:
    static PrestigioHollowMgr* instance();
    void LoadConfig();
    void Start();
    void Stop();
    void RepairDefaultLanguages(Player* player) const;
    bool IsEnabled() const { return _enabled; }
    uint32 GetRequiredLevel() const { return _requiredLevel; }
    uint32 GetPrestigeCount(uint32 guidLow) const;
    bool CanPrestige(Player* player) const;
    bool Prestige(Player* player);

private:
    using EquipmentMovePlan = std::vector<std::pair<Item*, uint16>>;
    bool PlanEquippedItems(Player* player, EquipmentMovePlan& plan) const;
    void StoreEquippedItems(Player* player, EquipmentMovePlan const& plan) const;
    void ClearQuestProgress(Player* player) const;
    std::unordered_set<uint32> SnapshotPermanentSpells(Player* player) const;
    void RestorePermanentSpells(Player* player, std::unordered_set<uint32> const& spells) const;
    void GiveStartingOutfit(Player* player) const;
    void TeleportToRaceStart(Player* player) const;

    bool _started{false};
    bool _enabled{true};
    uint32 _requiredLevel{80};
    bool _announceWorld{true};
    bool _giveStartingOutfit{true};
    bool _preservePermanentSpells{true};
};
#define sPrestigioHollowMgr PrestigioHollowMgr::instance()
#endif
