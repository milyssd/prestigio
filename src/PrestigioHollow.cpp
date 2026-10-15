#include "PrestigioHollow.h"
#include "Chat.h"
#include "Config.h"
#include "DatabaseEnv.h"
#include "PrestigioDatabase.h"
#include "LanguagePreservation.h"
#include "GameEventMgr.h"
#include "DBCStores.h"
#include "Item.h"
#include "Log.h"
#include "Bag.h"
#include "EquipmentStorage.h"
#include "ObjectMgr.h"
#include "Player.h"
#include "SpellMgr.h"
#include "WorldSession.h"
#include "WorldSessionMgr.h"
#include <algorithm>
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
    LOG_INFO("module", "mod-prestigio-hollow: Enable={}, RequiredLevel={}", _enabled, _requiredLevel);
    if (!_enabled)
        PrestigioDatabase::Instance().Close();
    else if (_started)
        PrestigioDatabase::Instance().Open(sConfigMgr->GetOption<std::string>("PrestigioHollow.DatabaseInfo", ""));
}

void PrestigioHollowMgr::Start()
{
    _started = true;
    if (_enabled)
        PrestigioDatabase::Instance().Open(sConfigMgr->GetOption<std::string>("PrestigioHollow.DatabaseInfo", ""));
}

void PrestigioHollowMgr::Stop()
{
    _started = false;
    PrestigioDatabase::Instance().Close();
}

uint32 PrestigioHollowMgr::GetPrestigeCount(uint32 guidLow) const
{
    uint32 count = 0;
    if (_enabled) PrestigioDatabase::Instance().GetCount(guidLow, count);
    return count;
}

void PrestigioHollowMgr::RepairDefaultLanguages(Player* player) const
{
    if (!player || !_enabled) return;
    auto info = sObjectMgr->GetPlayerInfo(player->getRace(), player->getClass());
    if (!info) return;
    for (auto const& language : lang_description)
    {
        if (!language.skill_id || !language.spell_id) continue;
        bool native = std::any_of(info->skills.begin(), info->skills.end(), [&](auto const& skill)
        {
            return skill.SkillId == language.skill_id;
        });
        if (!native) continue;
        if (!player->HasSpell(language.spell_id)) player->learnSpell(language.spell_id);
        if (!player->HasSkill(language.skill_id)) player->SetSkill(language.skill_id, 0, 300, 300);
    }
}

bool PrestigioHollowMgr::PlanEquippedItems(Player* player, EquipmentMovePlan& plan) const
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

    plan.clear();
    for (std::size_t i = 0; i < equipment.size(); ++i)
        plan.emplace_back(equipment[i], destinations[i]);
    return true;
}

void PrestigioHollowMgr::StoreEquippedItems(Player* player, EquipmentMovePlan const& plan) const
{
    for (auto const& move : plan)
    {
        Item* item = move.first;
        ItemPosCountVec destination;
        destination.emplace_back(move.second, item->GetCount());
        player->RemoveItem(INVENTORY_SLOT_BAG_0, item->GetSlot(), true);
        player->StoreItem(destination, item, true);
    }
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
    std::vector<uint32> active, rewarded;
    for (auto const& p : player->getQuestStatusMap()) active.push_back(p.first);
    for (uint32 id : player->getRewardedQuests()) rewarded.push_back(id);
    for (uint16 slot = 0; slot < MAX_QUEST_LOG_SIZE; ++slot) player->SetQuestSlot(slot, 0);
    for (uint32 id : active) player->RemoveActiveQuest(id, true);
    for (uint32 id : rewarded) player->RemoveRewardedQuest(id, true);
    player->ResetDailyQuestStatus();
    player->ResetWeeklyQuestStatus();
    player->ResetMonthlyQuestStatus();
    // Clear live seasonal state through the public API, without querying core tables.
    auto const& events = sGameEventMgr->GetEventMap();
    for (std::size_t event = 0; event < events.size() && event <= 65535; ++event)
        player->ResetSeasonalQuestStatus(uint16(event));
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
        if (ItemTemplate const* proto = sObjectMgr->GetItemTemplate(id))
        {
            uint32 count = proto->BuyCount;
            // Inventory and bank are retained. Do not recreate unique items such
            // as the hearthstone when the character already owns the allowed amount.
            if (proto->MaxCount > 0)
            {
                uint32 owned = player->GetItemCount(id, true);
                uint32 limit = uint32(proto->MaxCount);
                if (owned >= limit)
                    continue;
                count = std::min(count, limit - owned);
            }
            if (count && !player->StoreNewItemInBestSlots(id, count))
                ChatHandler(player->GetSession()).PSendSysMessage(
                    "No se pudo entregar el objeto inicial %u. El prestigio continúa y tus objetos anteriores se conservan.", id);
        }
    }
}

void PrestigioHollowMgr::TeleportToRaceStart(Player* player) const
{
    auto it = RaceStart.find(player->getRace());
    if (it == RaceStart.end() || !player->TeleportTo(it->second.map, it->second.x, it->second.y, it->second.z, it->second.o))
    {
        ChatHandler(player->GetSession()).SendSysMessage(
            "El prestigio se completó, pero falló el traslado a la zona inicial. No repitas el prestigio: avisa a un GM.");
        LOG_ERROR("module", "mod-prestigio-hollow: fallo el traslado inicial del personaje {} (raza {}).",
            player->GetGUID().GetCounter(), player->getRace());
    }
}

bool PrestigioHollowMgr::CanPrestige(Player* player) const
{
    if (!player) return false;
    if (!_enabled) { ChatHandler(player->GetSession()).SendSysMessage("El modo prestigio está desactivado."); return false; }
    if (!PrestigioDatabase::Instance().IsReady()) { ChatHandler(player->GetSession()).SendSysMessage("La base independiente de prestigio no está disponible. Avisa a un GM; tu personaje no se modificó."); return false; }
    if (!player->IsAlive()) { ChatHandler(player->GetSession()).SendSysMessage("Debes estar vivo para prestigiar."); return false; }
    if (player->GetTradeData()) { ChatHandler(player->GetSession()).SendSysMessage("Cierra el intercambio antes de prestigiar."); return false; }
    if (player->GetLevel() < _requiredLevel) { ChatHandler(player->GetSession()).PSendSysMessage("Debes alcanzar nivel %u para prestigiar.", _requiredLevel); return false; }
    if (player->IsInCombat()) { ChatHandler(player->GetSession()).PSendSysMessage("No puedes prestigiar en combate."); return false; }
    if (player->GetPetGUID()) { ChatHandler(player->GetSession()).PSendSysMessage("Desinvoca tu mascota antes de prestigiar."); return false; }

    return true;
}

bool PrestigioHollowMgr::Prestige(Player* player)
{
    if (!CanPrestige(player))
        return false;

    EquipmentMovePlan equipment;
    if (!PlanEquippedItems(player, equipment)) return false;
    auto permanent = SnapshotPermanentSpells(player);
    auto languages = PrestigioHollow::SnapshotLanguages(player, lang_description);
    uint8 previousLevel = player->GetLevel();
    uint32 count = 0;
    // Commit the module counter and history together before changing the character.
    if (!PrestigioDatabase::Instance().Record(player->GetGUID().GetCounter(), previousLevel,
        player->getClass(), player->getRace(), count))
    {
        ChatHandler(player->GetSession()).SendSysMessage("No se pudo registrar el prestigio en su base independiente. No se ha cambiado nada.");
        return false;
    }
    StoreEquippedItems(player, equipment);

    ClearQuestProgress(player);                 // active + completed quests are intentionally erased
    player->GiveLevel(1);                       // ALWAYS level 1, including DK
    player->SetUInt32Value(PLAYER_XP, 0);
    player->resetTalents(true);
    player->resetSpells();                       // remove level-80 combat progression
    RestorePermanentSpells(player, permanent);  // professions, riding, mounts/companions, runeforging
    PrestigioHollow::RestoreLanguages(player, languages); // always preserve languages, independent of config
    RepairDefaultLanguages(player);
    player->InitTalentForLevel();
    for (uint8 b = 0; b < MAX_ACTION_BUTTONS; ++b) player->removeActionButton(b);
    GiveStartingOutfit(player);

    if (_announceWorld)
        sWorldSessionMgr->SendServerMessage(SERVER_MSG_STRING, "|cff8A2BE2[Prestigio Hollow]|r |cffffff00" + player->GetName() + "|r volvió a nivel 1. Prestigio: |cff00ff00" + std::to_string(count) + "|r");
    // AzerothCore's reset APIs require the caller to delete daily/weekly/monthly/seasonal
    // persistence. Save before teleporting: a far teleport defers SaveToDB and would
    // otherwise commit only these deletes, without the rest of the character state.
    auto trans = CharacterDatabase.BeginTransaction();
    for (auto statement : {CHAR_DEL_QUEST_STATUS_DAILY_CHAR, CHAR_DEL_QUEST_STATUS_WEEKLY_CHAR,
        CHAR_DEL_QUEST_STATUS_MONTHLY_CHAR, CHAR_DEL_QUEST_STATUS_SEASONAL_CHAR})
    {
        auto query = CharacterDatabase.GetPreparedStatement(statement);
        query->SetData(0, player->GetGUID().GetCounter());
        trans->Append(query);
    }
    player->SaveToDB(trans, false, true);
    CharacterDatabase.CommitTransaction(trans);
    TeleportToRaceStart(player);
    // Also request a save of the destination; core defers it until a far teleport finishes.
    player->SaveToDB(false, true);
    ChatHandler(player->GetSession()).PSendSysMessage("Prestigio completado: nivel 1, misiones reiniciadas, objetos y logros conservados. Prestigio %u.", count);
    return true;
}
