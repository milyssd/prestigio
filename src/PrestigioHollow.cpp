#include "PrestigioHollow.h"
#include "Chat.h"
#include "Config.h"
#include "DatabaseEnv.h"
#include "DBCStores.h"
#include "Item.h"
#include "Bag.h"
#include "EquipmentStorage.h"
#include "ObjectMgr.h"
#include "Player.h"
#include "SpellMgr.h"
#include "WorldSession.h"
#include "WorldSessionMgr.h"
#include <unordered_map>
#include <vector>

namespace
{
struct StartLoc { uint32 map; float x, y, z, o; };
std::unordered_map<uint8, StartLoc> const RaceStart = {
    {RACE_HUMAN,{0,-8949.95f,-132.493f,83.5312f,3.142f}},
    {RACE_ORC,{1,-618.518f,-4251.67f,38.718f,6.20f}},
    {RACE_DWARF,{0,-6240.32f,331.033f,382.757f,5.20f}},
    {RACE_NIGHTELF,{1,10311.3f,832.463f,1326.41f,5.70f}},
    {RACE_UNDEAD_PLAYER,{0,1676.35f,1678.68f,121.67f,1.60f}},
    {RACE_TAUREN,{1,-2917.58f,-257.98f,52.9968f,0.0f}},
    {RACE_GNOME,{0,-6240.95f,331.493f,382.5312f,5.20f}},
    {RACE_TROLL,{1,-618.518f,-4251.67f,38.718f,6.20f}},
    {RACE_BLOODELF,{530,10349.6f,-6357.29f,33.4026f,5.30f}},
    {RACE_DRAENEI,{530,-3961.64f,-13931.2f,100.615f,2.08f}}
};
}

PrestigioHollowMgr* PrestigioHollowMgr::instance(){ static PrestigioHollowMgr i; return &i; }

void PrestigioHollowMgr::LoadConfig()
{
    _enabled = sConfigMgr->GetOption<bool>("PrestigioHollow.Enable", true);
    _requiredLevel = sConfigMgr->GetOption<uint32>("PrestigioHollow.RequiredLevel", 80);
    _announceWorld = sConfigMgr->GetOption<bool>("PrestigioHollow.AnnounceWorld", true);
    _giveStartingOutfit = sConfigMgr->GetOption<bool>("PrestigioHollow.GiveStartingOutfit", true);
    _preservePermanentSpells = sConfigMgr->GetOption<bool>("PrestigioHollow.PreservePermanentSpells", true);
}

uint32 PrestigioHollowMgr::GetPrestigeCount(uint32 guidLow) const
{
    QueryResult r = CharacterDatabase.Query("SELECT `prestige_count` FROM `mod_prestigio_hollow_stats` WHERE `player_guid`=" + std::to_string(guidLow));
    return r ? r->Fetch()[0].Get<uint32>() : 0;
}

void PrestigioHollowMgr::RecordPrestigeHistory(Player* player, uint32 prestigeNumber, uint8 previousLevel) const
{
    if (!player)
        return;

    uint32 guid = player->GetGUID().GetCounter();
    uint8 race = player->getRace();
    uint8 playerClass = player->getClass();

    CharacterDatabase.Execute(
        "INSERT IGNORE INTO `mod_prestigio_hollow_history` "
        "(`player_guid`,`prestige_number`,`prestiged_at`,`previous_level`,`class`,`race`) VALUES (" +
        std::to_string(guid) + "," +
        std::to_string(prestigeNumber) + ",NOW()," +
        std::to_string(previousLevel) + "," +
        std::to_string(playerClass) + "," +
        std::to_string(race) + ")");
}

bool PrestigioHollowMgr::StoreEquippedItems(Player* player) const
{
    std::vector<uint16> freeSlots;
    for (uint8 slot = INVENTORY_SLOT_ITEM_START; slot < INVENTORY_SLOT_ITEM_END; ++slot)
        if (!player->GetItemByPos(INVENTORY_SLOT_BAG_0, slot))
            freeSlots.push_back((uint16(INVENTORY_SLOT_BAG_0) << 8) | slot);

    for (uint8 bagSlot = INVENTORY_SLOT_BAG_START; bagSlot < INVENTORY_SLOT_BAG_END; ++bagSlot)
        if (Bag* bag = player->GetBagByPos(bagSlot))
            for (uint32 slot = 0; slot < bag->GetBagSize(); ++slot)
                if (!bag->GetItemByPos(slot))
                    freeSlots.push_back((uint16(bagSlot) << 8) | slot);

    std::vector<Item*> equipment;
    std::vector<std::vector<uint16>> candidates;
    for (uint8 slot = EQUIPMENT_SLOT_START; slot < EQUIPMENT_SLOT_END; ++slot)
    {
        Item* item = player->GetItemByPos(INVENTORY_SLOT_BAG_0, slot);
        if (!item)
            continue;

        InventoryResult result = player->CanUnequipItem(item->GetPos(), false);
        if (result != EQUIP_ERR_OK)
        {
            player->SendEquipError(result, item);
            ChatHandler(player->GetSession()).SendSysMessage(
                "No puedes prestigiar: no se puede desequipar una de tus piezas. No se ha cambiado nada.");
            return false;
        }

        equipment.push_back(item);
        candidates.emplace_back();
        for (uint16 pos : freeSlots)
        {
            ItemPosCountVec destination;
            if (player->CanStoreItem(pos >> 8, pos & 255, destination, item, false) == EQUIP_ERR_OK &&
                destination.size() == 1 && destination.front().pos == pos &&
                destination.front().count == item->GetCount())
                candidates.back().push_back(pos);
        }
    }

    std::vector<uint16> destinations;
    if (!PrestigioHollow::PlanEquipmentStorage(candidates, destinations))
    {
        ChatHandler(player->GetSession()).PSendSysMessage(
            "No tienes espacio para guardar todo tu equipo. Necesitas %u huecos libres compatibles "
            "en la mochila o las bolsas, uno por pieza equipada. No se ha cambiado nada.",
            uint32(equipment.size()));
        return false;
    }

    // Every destination is empty, compatible and reserved for one existing item.
    // Move the original instances: preserve GUIDs, enchants, gems and durability.
    for (std::size_t i = 0; i < equipment.size(); ++i)
    {
        Item* item = equipment[i];
        ItemPosCountVec destination;
        destination.emplace_back(destinations[i], item->GetCount());
        player->RemoveItem(INVENTORY_SLOT_BAG_0, item->GetSlot(), true);
        player->StoreItem(destination, item, true);
    }
    return true;
}

std::unordered_set<uint32> PrestigioHollowMgr::SnapshotPermanentSpells(Player* player) const
{
    std::unordered_set<uint32> out;
    if (!_preservePermanentSpells) return out;
    PlayerSpellMap const& spellMap = player->GetSpellMap();
    for (auto const& pair : spellMap)
    {
        if (!pair.second || pair.second->State == PLAYERSPELL_REMOVED) continue;
        uint32 spellId = pair.first;
        SkillLineAbilityMapBounds bounds = sSpellMgr->GetSkillLineAbilityMapBounds(spellId);
        for (SkillLineAbilityMap::const_iterator it = bounds.first; it != bounds.second; ++it)
        {
            SkillLineAbilityEntry const* a = it->second;
            if (!a) continue;
            uint32 skill = a->SkillLine;
            if (IsProfessionOrRidingSkill(skill) || skill == SKILL_MOUNTS || skill == SKILL_COMPANIONS || skill == SKILL_RUNEFORGING)
            { out.insert(spellId); break; }
        }
    }
    return out;
}

void PrestigioHollowMgr::RestorePermanentSpells(Player* player, std::unordered_set<uint32> const& spells) const
{
    for (uint32 id : spells) if (sSpellMgr->GetSpellInfo(id) && !player->HasSpell(id)) player->learnSpell(id);
}

void PrestigioHollowMgr::ClearQuestProgress(Player* player) const
{
    uint32 guid = player->GetGUID().GetCounter();
    std::vector<uint32> active, rewarded;
    for (auto const& p : player->getQuestStatusMap()) active.push_back(p.first);
    for (uint32 id : player->getRewardedQuests()) rewarded.push_back(id);
    for (uint16 slot = 0; slot < MAX_QUEST_LOG_SIZE; ++slot) player->SetQuestSlot(slot, 0);
    for (uint32 id : active) player->RemoveActiveQuest(id, true);
    for (uint32 id : rewarded) player->RemoveRewardedQuest(id, true);
    player->ResetDailyQuestStatus();
    player->ResetWeeklyQuestStatus();
    player->ResetMonthlyQuestStatus();
    QueryResult seasonal = CharacterDatabase.Query("SELECT DISTINCT `event` FROM `character_queststatus_seasonal` WHERE `guid`=" + std::to_string(guid));
    if (seasonal) do { player->ResetSeasonalQuestStatus(seasonal->Fetch()[0].Get<uint16>()); } while (seasonal->NextRow());
    for (char const* table : {"character_queststatus","character_queststatus_rewarded","character_queststatus_daily","character_queststatus_weekly","character_queststatus_monthly","character_queststatus_seasonal"})
        CharacterDatabase.Execute(std::string("DELETE FROM `") + table + "` WHERE `guid`=" + std::to_string(guid));
}

void PrestigioHollowMgr::GiveStartingOutfit(Player* player) const
{
    if (!_giveStartingOutfit) return;
    CharStartOutfitEntry const* e = GetCharStartOutfitEntry(player->getRace(), player->getClass(), player->getGender());
    if (!e) return;
    for (uint8 i = 0; i < MAX_OUTFIT_ITEMS; ++i)
    {
        if (e->ItemId[i] <= 0) continue;
        uint32 id = e->ItemId[i];
        if (ItemTemplate const* proto = sObjectMgr->GetItemTemplate(id)) player->StoreNewItemInBestSlots(id, proto->BuyCount);
    }
}

void PrestigioHollowMgr::TeleportToRaceStart(Player* player) const
{
    auto it = RaceStart.find(player->getRace());
    if (it != RaceStart.end()) player->TeleportTo(it->second.map,it->second.x,it->second.y,it->second.z,it->second.o);
}

bool PrestigioHollowMgr::Prestige(Player* player)
{
    if (!player) return false;
    if (!_enabled) { ChatHandler(player->GetSession()).SendSysMessage("El modo prestigio está desactivado."); return false; }
    if (!player->IsAlive()) { ChatHandler(player->GetSession()).SendSysMessage("Debes estar vivo para prestigiar."); return false; }
    if (player->GetTradeData()) { ChatHandler(player->GetSession()).SendSysMessage("Cierra el intercambio antes de prestigiar."); return false; }
    if (player->GetLevel() < _requiredLevel) { ChatHandler(player->GetSession()).PSendSysMessage("Debes alcanzar nivel %u para prestigiar.", _requiredLevel); return false; }
    if (player->IsInCombat()) { ChatHandler(player->GetSession()).PSendSysMessage("No puedes prestigiar en combate."); return false; }
    if (player->GetPetGUID()) { ChatHandler(player->GetSession()).PSendSysMessage("Desinvoca tu mascota antes de prestigiar."); return false; }

    uint32 guid = player->GetGUID().GetCounter();
    uint8 previousLevel = player->GetLevel();
    uint32 count = GetPrestigeCount(guid) + 1;
    auto permanent = SnapshotPermanentSpells(player);
    if (!StoreEquippedItems(player)) return false;

    ClearQuestProgress(player);                 // active + completed quests are intentionally erased
    player->GiveLevel(1);                       // ALWAYS level 1, including DK
    player->SetUInt32Value(PLAYER_XP, 0);
    player->resetTalents(true);
    player->resetSpells();                       // remove level-80 combat progression
    RestorePermanentSpells(player, permanent);  // professions, riding, mounts/companions, runeforging
    player->InitTalentForLevel();
    for (uint8 b = 0; b < MAX_ACTION_BUTTONS; ++b) player->removeActionButton(b);
    CharacterDatabase.Execute("DELETE FROM `character_action` WHERE `guid`=" + std::to_string(guid));
    GiveStartingOutfit(player);

    CharacterDatabase.Execute("INSERT INTO `mod_prestigio_hollow_stats` (`player_guid`,`prestige_count`) VALUES (" + std::to_string(guid) + "," + std::to_string(count) + ") ON DUPLICATE KEY UPDATE `prestige_count`=VALUES(`prestige_count`)");
    RecordPrestigeHistory(player, count, previousLevel);
    if (_announceWorld)
        sWorldSessionMgr->SendServerMessage(SERVER_MSG_STRING, "|cff8A2BE2[Prestigio Hollow]|r |cffffff00" + player->GetName() + "|r volvió a nivel 1. Prestigio: |cff00ff00" + std::to_string(count) + "|r");
    TeleportToRaceStart(player);
    player->SaveToDB(false, true);
    ChatHandler(player->GetSession()).PSendSysMessage("Prestigio completado: nivel 1, misiones reiniciadas, objetos y logros conservados. Prestigio %u.", count);
    return true;
}
