#ifndef MOD_PRESTIGIO_HOLLOW_H
#define MOD_PRESTIGIO_HOLLOW_H

#include "Common.h"
#include <functional>
#include <mutex>
#include <optional>
#include <unordered_map>
#include <unordered_set>
#include <vector>
class Player;
class Creature;
class Item;

class PrestigioHollowMgr
{
public:
    static PrestigioHollowMgr* instance();
    void LoadConfig();
    void Start();
    void Stop();
    void Update();
    void RepairDefaultLanguages(Player* player) const;
    bool IsEnabled() const { return _enabled; }
    uint32 GetRequiredLevel() const { return _requiredLevel; }
    void RequestCount(Player* player, Creature* creature,
        std::function<void(Player*, Creature*, std::optional<uint32>)> render);
    void CancelMenuRequest(uint32 guid);
    bool IsBusy(uint32 guid) const;
    bool CanPrestige(Player* player) const;
    bool Prestige(Player* player);

private:
    struct EquipmentMove { Item* item; uint16 destination; uint8 originalSlot; };
    using EquipmentMovePlan = std::vector<EquipmentMove>;
    bool CheckPlayer(Player* player) const;
    void ApplyPrestige(Player* player);
    bool PlanEquippedItems(Player* player, EquipmentMovePlan& plan) const;
    void StoreEquippedItems(Player* player, EquipmentMovePlan const& plan) const;
    void RestoreEquippedItems(Player* player, EquipmentMovePlan const& plan) const;
    void SetBusy(uint32 guid, bool busy);
    void ClearQuestProgress(Player* player) const;
    std::unordered_set<uint32> SnapshotPermanentSpells(Player* player) const;
    void RestorePermanentSpells(Player* player, std::unordered_set<uint32> const& spells) const;
    void ResetAllTalents(Player* player) const;
    void GiveStartingOutfit(Player* player) const;

    bool _started{false};
    bool _enabled{true};
    uint32 _requiredLevel{80};
    bool _announceWorld{true};
    bool _giveStartingOutfit{true};
    bool _preservePermanentSpells{true};
    mutable std::mutex _busyMutex;
    std::unordered_set<uint32> _busy;
    std::unordered_map<uint32, uint64> _menuRequests;
    uint64 _nextMenuRequest = 0;
};
#define sPrestigioHollowMgr PrestigioHollowMgr::instance()
#endif
