#include "PrestigioHollow.h"
#include "Chat.h"
#include "Config.h"
#include "DatabaseEnv.h"
#include "PrestigioDatabase.h"
#include "LanguagePreservation.h"
#include "PermanentSkills.h"
#include "CryptoRandom.h"
#include "Util.h"
#include "ObjectAccessor.h"
#include "Creature.h"
#include "ScriptedGossip.h"
#include "CharacterCache.h"
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
    if (_requiredLevel < 2 || _requiredLevel > 255)
        _requiredLevel = 80;
    if (_started)
        PrestigioDatabase::Instance().SetEnabled(_enabled);
}

void PrestigioHollowMgr::Start()
{
    _started = true;
    // Separate worker, same MySQL instance: native character state and own-schema receipts commit together.
    PrestigioDatabase::Instance().Start(sConfigMgr->GetOption<std::string>("CharacterDatabaseInfo", ""), _enabled);
}

void PrestigioHollowMgr::Stop()
{
    PrestigioDatabase::Instance().Stop();
    _started = false;
}

void PrestigioHollowMgr::Update()
{
    PrestigioDatabase::Instance().Poll();
}

bool PrestigioHollowMgr::IsBusy(uint32 guid) const
{
    std::lock_guard<std::mutex> lock(_busyMutex);
    return _busy.count(guid) != 0;
}

void PrestigioHollowMgr::SetBusy(uint32 guid, bool busy)
{
    std::lock_guard<std::mutex> lock(_busyMutex);
    if (busy)
        _busy.insert(guid);
    else
        _busy.erase(guid);
}

void PrestigioHollowMgr::CancelMenuRequest(uint32 guid)
{
    _menuRequests.erase(guid);
}

void PrestigioHollowMgr::RequestCount(Player* player, Creature* creature,
    std::function<void(Player*, Creature*, std::optional<uint32>)> render)
{
    auto guid = player->GetGUID();
    auto npc = creature->GetGUID();
    auto account = player->GetSession()->GetAccountId();
    auto ticket = ++_nextMenuRequest;
    _menuRequests[guid.GetCounter()] = ticket;
    PrestigioDatabase::Instance().GetCount(guid.GetCounter(), [this, guid, npc, account, ticket, render](bool ok, uint32 count)
    {
        auto it = _menuRequests.find(guid.GetCounter());
        if (it == _menuRequests.end() || it->second != ticket)
            return;
        _menuRequests.erase(it);
        Player* current = ObjectAccessor::FindConnectedPlayer(guid);
        if (!current || current->GetSession()->GetAccountId() != account ||
            current->PlayerTalkClass->GetGossipMenu().GetSenderGUID() != npc)
            return;
        if (Creature* source = ObjectAccessor::GetCreature(*current, npc))
            render(current, source, ok ? std::optional<uint32>(count) : std::nullopt);
    });
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
        plan.push_back({equipment[i], destinations[i], equipment[i]->GetSlot()});
    return true;
}

void PrestigioHollowMgr::StoreEquippedItems(Player* player, EquipmentMovePlan const& plan) const
{
    for (auto const& move : plan)
    {
        Item* item = move.item;
        ItemPosCountVec destination;
        destination.emplace_back(move.destination, item->GetCount());
        player->RemoveItem(INVENTORY_SLOT_BAG_0, item->GetSlot(), true);
        player->StoreItem(destination, item, true);
    }
}

void PrestigioHollowMgr::RestoreEquippedItems(Player* player, EquipmentMovePlan const& plan) const
{
    for (auto const& move : plan)
    {
        player->RemoveItem(move.item->GetBagSlot(), move.item->GetSlot(), true);
        player->EquipItem((uint16(INVENTORY_SLOT_BAG_0) << 8) | move.originalSlot, move.item, true);
    }
}

void PrestigioHollowMgr::ResetAllTalents(Player* player) const
{
    player->resetTalents(true);
    std::vector<uint32> talents;
    for (auto const& talent : player->GetTalentMap())
        if (talent.second && talent.second->State != PLAYERSPELL_REMOVED)
            talents.push_back(talent.first);
    for (uint32 id : talents)
        player->_removeTalent(id, SPEC_MASK_ALL);
    player->InitTalentForLevel();
    for (uint8 button = 0; button < MAX_ACTION_BUTTONS; ++button)
        player->removeActionButton(button);
    player->SendActionButtons(2);
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

bool PrestigioHollowMgr::CheckPlayer(Player* player) const
{
    if (!player || !player->IsInWorld())
        return false;
    auto chat = ChatHandler(player->GetSession());
    if (!_enabled) { chat.SendSysMessage("El modo prestigio está desactivado."); return false; }
    if (!player->IsAlive()) { chat.SendSysMessage("Debes estar vivo para prestigiar."); return false; }
    if (player->GetTradeData()) { chat.SendSysMessage("Cierra el intercambio antes de prestigiar."); return false; }
    if (player->GetLevel() < _requiredLevel) { chat.PSendSysMessage("Debes alcanzar nivel {} para prestigiar.", _requiredLevel); return false; }
    if (player->IsInCombat()) { chat.SendSysMessage("No puedes prestigiar en combate."); return false; }
    if (player->GetPetGUID()) { chat.SendSysMessage("Desinvoca tu mascota antes de prestigiar."); return false; }
    if (player->IsBeingTeleported() || player->IsInFlight() || player->InBattleground() || player->InArena())
    {
        chat.SendSysMessage("Termina el viaje o sal del campo de batalla antes de prestigiar.");
        return false;
    }
    if (RaceStart.find(player->getRace()) == RaceStart.end())
    {
        chat.SendSysMessage("No hay zona inicial configurada para tu raza.");
        return false;
    }
    return true;
}

bool PrestigioHollowMgr::CanPrestige(Player* player) const
{
    if (!CheckPlayer(player))
        return false;
    if (IsBusy(player->GetGUID().GetCounter()))
    {
        ChatHandler(player->GetSession()).SendSysMessage("Tu solicitud de prestigio ya está en proceso.");
        return false;
    }
    return true;
}

bool PrestigioHollowMgr::Prestige(Player* player)
{
    if (!CanPrestige(player))
        return false;
    auto guid = player->GetGUID();
    auto account = player->GetSession()->GetAccountId();
    SetBusy(guid.GetCounter(), true);
    ChatHandler(player->GetSession()).SendSysMessage("Comprobando el prestigio. Al guardar volverás a la selección de personajes.");
    PrestigioDatabase::Instance().Check([this, guid, account](bool ready)
    {
        Player* current = ObjectAccessor::FindConnectedPlayer(guid);
        if (!current || current->GetSession()->GetAccountId() != account)
        {
            SetBusy(guid.GetCounter(), false);
            return;
        }
        if (!ready || !CheckPlayer(current))
        {
            if (!ready)
                ChatHandler(current->GetSession()).SendSysMessage("El prestigio no está disponible. Tu personaje no se modificó; avisa a un GM.");
            SetBusy(guid.GetCounter(), false);
            return;
        }
        ApplyPrestige(current);
    });
    return true;
}

void PrestigioHollowMgr::ApplyPrestige(Player* player)
{
    auto guid = player->GetGUID();
    EquipmentMovePlan equipment;
    if (!PlanEquippedItems(player, equipment))
    {
        SetBusy(guid.GetCounter(), false);
        return;
    }
    auto permanent = SnapshotPermanentSpells(player);
    auto languages = PrestigioHollow::SnapshotLanguages(player, lang_description);
    auto skills = PrestigioHollow::SnapshotSkills(player, [this](uint32 id)
    {
        return _preservePermanentSpells && (IsProfessionOrRidingSkill(id) || id == SKILL_MOUNTS ||
            id == SKILL_COMPANIONS || id == SKILL_RUNEFORGING);
    });
    uint16 freeProfessions = player->GetFreePrimaryProfessionPoints();
    uint8 previousLevel = player->GetLevel();
    uint8 playerClass = player->getClass(), race = player->getRace();
    auto name = player->GetName();
    auto baseline = std::make_shared<PrestigeSaveTransaction>();
    player->SaveToDB(baseline, false, true);
    StoreEquippedItems(player, equipment);
    player->GiveLevel(1);
    bool cancelled = player->GetLevel() != 1;
    auto changed = std::make_shared<PrestigeSaveTransaction>();
    if (cancelled)
    {
        RestoreEquippedItems(player, equipment);
        ChatHandler(player->GetSession()).SendSysMessage("Otro módulo impidió bajar de nivel. El prestigio se canceló sin borrar tus misiones ni contar otro prestigio.");
    }
    else
    {
        ClearQuestProgress(player);
        player->SetUInt32Value(PLAYER_XP, 0);
        ResetAllTalents(player);
        player->resetSpells();
        RestorePermanentSpells(player, permanent);
        PrestigioHollow::RestoreSkills(player, skills);
        if (_preservePermanentSpells)
            player->SetFreePrimaryProfessions(freeProfessions);
        PrestigioHollow::RestoreLanguages(player, languages);
        RepairDefaultLanguages(player);
        GiveStartingOutfit(player);
        for (auto statement : {CHAR_DEL_QUEST_STATUS_DAILY_CHAR, CHAR_DEL_QUEST_STATUS_WEEKLY_CHAR,
            CHAR_DEL_QUEST_STATUS_MONTHLY_CHAR, CHAR_DEL_QUEST_STATUS_SEASONAL_CHAR})
        {
            auto query = CharacterDatabase.GetPreparedStatement(statement);
            query->SetData(0, guid.GetCounter());
            changed->Append(query);
        }
        player->SaveToDB(changed, false, true);
        // Inactive-spec actions are not loaded into m_actionButtons. Clear all specs in this same save.
        auto actions = CharacterDatabase.GetPreparedStatement(CHAR_DEL_CHAR_ACTION);
        actions->SetData(0, guid.GetCounter());
        changed->Append(actions);
        auto const& location = RaceStart.at(race);
        Player::SavePositionInDB(WorldLocation(location.map, location.x, location.y, location.z, location.o), 0, guid, changed);
    }
    auto token = ByteArrayToHexStr(Acore::Crypto::GetRandomBytes<16>());
    bool announce = _announceWorld;
    WorldSession* session = player->GetSession();
    // No ordinary logout/autosave may persist the in-memory reset outside the atomic transaction.
    // This call deletes Player. Nothing below captures or touches that pointer.
    session->LogoutPlayer(false);
    PrestigioDatabase::Instance().Save(guid.GetCounter(), previousLevel, playerClass, race,
        token, baseline, changed, cancelled, [this, guid, name, announce, previousLevel](bool success, uint32 count)
    {
        SetBusy(guid.GetCounter(), false);
        sCharacterCache->UpdateCharacterLevel(guid, success ? 1 : previousLevel);
        if (success)
        {
            LOG_INFO("module", "mod-prestigio-hollow: prestigio {} de {} guardado atomicamente.", count, name);
            if (announce)
                sWorldSessionMgr->SendServerMessage(SERVER_MSG_STRING, "[Prestigio Hollow] " + name +
                    " volvió a nivel 1. Prestigio: " + std::to_string(count));
        }
        else
            LOG_ERROR("module", "mod-prestigio-hollow: prestigio de {} cancelado; estado original restaurado y contador sin incrementar.", name);
    });
}
