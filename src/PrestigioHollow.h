#ifndef MOD_PRESTIGIO_HOLLOW_H
#define MOD_PRESTIGIO_HOLLOW_H

#include "Common.h"
#include "ObjectGuid.h"
#include "PrestigioDatabase.h"
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
    uint32 GetRequiredQuest() const { return _requiredQuest; }
    uint32 GetMaxPrestige() const { return _maxPrestige; }
    bool HasCompletedRequiredQuest(Player* player) const;
    uint32 GetPreserveMode() const { return _preservePermanentSpells; }

    void RequestCount(Player* player, Creature* creature,
        std::function<void(Player*, Creature*, std::optional<uint32>)> render);
    void CancelMenuRequest(uint32 guid);
    bool IsBusy(uint32 guid) const;
    bool CanPrestige(Player* player) const;
    bool Prestige(Player* player);

    struct PrestigeReward { uint32 itemId = 0; uint32 count = 1; };
    std::vector<PrestigeReward> GetRewardsForPrestige(uint32 prestigeCount) const;
    void SendPrestigeReward(ObjectGuid guid, uint32 prestigeCount) const;

    // Spells identificadores de prestigio (activos solo niveles 1 al 79)
    uint32 GetPrestigeSpell(uint32 prestigeCount) const;
    bool IsPrestigeSpell(uint32 spellId) const;
    void EnsurePrestigeSpell(Player* player, uint32 prestigeCount) const;
    void RemovePrestigeSpell(Player* player) const;
    void ResetPlayerLevelOne(Player* player) const;
    void EnsurePrestigedPlayerReady(Player* player, uint32 prestigeCount) const;

    // SysAlas: deteccion, remocion y purga
    bool IsSysAlasSpell(uint32 spellId) const;
    void RemoveSysAlasAuras(Player* player) const;
    void PurgeSysAlasActive(uint32 guid) const;

    // Retencion de profesiones, recetas y monturas
    void RestoreRetainedData(Player* player);

    // Actualizacion en memoria de configuracion proveniente de la DB
    void ApplyDbConfig(std::unordered_map<uint32, uint32> spells,
                       std::unordered_map<uint32, std::vector<PrestigeDbReward>> rewards,
                       std::unordered_set<uint32> sysAlasSpells);

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
    uint32 _requiredQuest{30077};
    bool _announceWorld{true};
    bool _giveStartingOutfit{true};
    uint32 _preservePermanentSpells{2}; // 0 = no, 1 = siempre activo, 2 = retencion hasta 80
    uint32 _maxPrestige{0};
    std::string _databaseName{"prestigio"};
    std::string _sysAlasDatabase{"sysalas"};

    std::unordered_map<uint32, std::vector<PrestigeReward>> _rewards;
    std::unordered_map<uint32, uint32> _spellsByLevel;
    std::unordered_set<uint32> _allPrestigeSpells;
    std::unordered_set<uint32> _sysAlasSpells;

    mutable std::mutex _busyMutex;
    std::unordered_set<uint32> _busy;
    std::unordered_map<uint32, uint64> _menuRequests;
    uint64 _nextMenuRequest = 0;
};

#define sPrestigioHollowMgr PrestigioHollowMgr::instance()

#endif
