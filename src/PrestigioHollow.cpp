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
#include "Mail.h"
#include "Log.h"
#include "Bag.h"
#include "EquipmentStorage.h"
#include "ObjectMgr.h"
#include "Player.h"
#include "ScriptMgr.h"
#include "Pet.h"
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

PrestigioHollowMgr* PrestigioHollowMgr::instance() { static PrestigioHollowMgr i; return &i; }

void PrestigioHollowMgr::LoadConfig()
{
    _enabled = sConfigMgr->GetOption<bool>("PrestigioHollow.Enable", true);
    _databaseName = sConfigMgr->GetOption<std::string>("PrestigioHollow.DatabaseName", "prestigio");
    _sysAlasDatabase = sConfigMgr->GetOption<std::string>("PrestigioHollow.SysAlas.Database", "sysalas");
    _requiredLevel = sConfigMgr->GetOption<uint32>("PrestigioHollow.RequiredLevel", 80);
    _maxPrestige = sConfigMgr->GetOption<uint32>("PrestigioHollow.MaxPrestige", 0);
    _announceWorld = sConfigMgr->GetOption<bool>("PrestigioHollow.AnnounceWorld", true);
    _giveStartingOutfit = sConfigMgr->GetOption<bool>("PrestigioHollow.GiveStartingOutfit", true);
    _preservePermanentSpells = sConfigMgr->GetOption<uint32>("PrestigioHollow.PreservePermanentSpells", 2);

    if (_requiredLevel < 2 || _requiredLevel > 255)
        _requiredLevel = 80;

    LOG_INFO("module", "mod-prestigio-hollow: Enable={}, DB='{}', SysAlasDB='{}', RequiredLevel={}, MaxPrestige={}, PreserveMode={}",
        _enabled, _databaseName, _sysAlasDatabase, _requiredLevel, _maxPrestige, _preservePermanentSpells);

    if (_started)
        PrestigioDatabase::Instance().SetEnabled(_enabled);
}

void PrestigioHollowMgr::Start()
{
    _started = true;
    std::string connInfo = sConfigMgr->GetOption<std::string>("PrestigioHollow.DatabaseInfo", "");
    if (connInfo.empty())
        connInfo = sConfigMgr->GetOption<std::string>("CharacterDatabaseInfo", "");

    PrestigioDatabase::Instance().Start(connInfo, _databaseName, _sysAlasDatabase, _enabled);

    // Cargar configuracion de recompensas, spells identificadores y auras de SysAlas desde la DB
    PrestigioDatabase::Instance().LoadConfigData([this](bool success,
        std::unordered_map<uint32, uint32> spells,
        std::unordered_map<uint32, std::vector<PrestigeDbReward>> rewards,
        std::unordered_set<uint32> sysAlasSpells)
    {
        if (success)
            ApplyDbConfig(std::move(spells), std::move(rewards), std::move(sysAlasSpells));
    });
}

void PrestigioHollowMgr::ApplyDbConfig(std::unordered_map<uint32, uint32> spells,
                                      std::unordered_map<uint32, std::vector<PrestigeDbReward>> rewards,
                                      std::unordered_set<uint32> sysAlasSpells)
{
    _spellsByLevel = std::move(spells);
    _allPrestigeSpells.clear();
    for (auto const& p : _spellsByLevel)
        if (p.second > 0)
            _allPrestigeSpells.insert(p.second);

    _rewards.clear();
    for (auto const& p : rewards)
    {
        std::vector<PrestigeReward> list;
        for (auto const& r : p.second)
            list.push_back({r.itemId, r.count});
        _rewards[p.first] = std::move(list);
    }

    _sysAlasSpells = std::move(sysAlasSpells);

    LOG_INFO("module", "mod-prestigio-hollow: Cargados desde DB: {} rangos con spell, {} niveles de recompensa, {} auras de SysAlas.",
        _spellsByLevel.size(), _rewards.size(), _sysAlasSpells.size());
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
    if (_preservePermanentSpells == 0) return out;
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
    auto const& events = sGameEventMgr->GetEventMap();
    for (std::size_t event = 0; event < events.size() && event <= 65535; ++event)
        player->ResetSeasonalQuestStatus(uint16(event));
}

void PrestigioHollowMgr::GiveStartingOutfit(Player* player) const
{
    if (!_giveStartingOutfit) return;
    CharStartOutfitEntry const* e = GetCharStartOutfitEntry(player->getRace(), player->getClass(), player->getGender());
    if (!e) return;
    for (std::size_t i = 0; i < MAX_OUTFIT_ITEMS; ++i)
    {
        if (e->ItemId[i] <= 0) continue;
        ItemPosCountVec dest;
        InventoryResult msg = player->CanStoreNewItem(NULL_BAG, NULL_SLOT, dest, e->ItemId[i], 1);
        if (msg == EQUIP_ERR_OK)
            if (Item* it = player->StoreNewItem(dest, e->ItemId[i], true))
                player->SendNewItem(it, 1, true, false);
    }
}

bool PrestigioHollowMgr::CheckPlayer(Player* player) const
{
    if (!player || !_enabled)
        return false;
    if (player->getClass() == CLASS_DEATH_KNIGHT)
        return false;
    if (player->GetLevel() < _requiredLevel)
        return false;
    if (player->IsInCombat())
        return false;
    if (player->GetGroup())
        return false;
    if (player->m_mover != player)
        return false;
    if (player->HasUnitState(UNIT_STATE_CASTING))
        return false;
    if (player->GetTradeData())
        return false;
    if (player->FindMap() && player->FindMap()->IsDungeon())
        return false;
    return true;
}

bool PrestigioHollowMgr::CanPrestige(Player* player) const
{
    return CheckPlayer(player) && !IsBusy(player->GetGUID().GetCounter());
}

bool PrestigioHollowMgr::Prestige(Player* player)
{
    if (!CanPrestige(player))
        return false;
    auto guid = player->GetGUID().GetCounter();
    SetBusy(guid, true);
    PrestigioDatabase::Instance().GetCount(guid, [this, guid](bool ok, uint32 count)
    {
        if (!ok || (_maxPrestige > 0 && count >= _maxPrestige))
        {
            SetBusy(guid, false);
            return;
        }
        Player* current = ObjectAccessor::FindConnectedPlayer(ObjectGuid::Create<HighGuid::Player>(guid));
        if (!current || !CheckPlayer(current))
        {
            SetBusy(guid, false);
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
        return (_preservePermanentSpells > 0) && (IsProfessionOrRidingSkill(id) || id == SKILL_MOUNTS ||
            id == SKILL_COMPANIONS || id == SKILL_RUNEFORGING);
    });
    uint16 freeProfessions = player->GetFreePrimaryProfessionPoints();
    uint8 previousLevel = player->GetLevel();
    uint8 playerClass = player->getClass(), race = player->getRace();
    auto name = player->GetName();
    auto baseline = std::make_shared<PrestigeSaveTransaction>();
    player->SaveToDB(baseline, false, true);
    StoreEquippedItems(player, equipment);
    player->_ApplyAllLevelScaleItemMods(false);
    player->SetLevel(1);
    player->InitRunes();
    player->InitStatsForLevel(true);
    player->InitTaxiNodesForLevel();
    player->InitGlyphsForLevel();
    player->InitTalentForLevel();
    player->SetUInt32Value(PLAYER_XP, 0);
    player->SetUInt32Value(PLAYER_NEXT_LEVEL_XP, sObjectMgr->GetXPForLevel(1));
    player->RemovePlayerFlag(PLAYER_FLAGS_NO_XP_GAIN);
    player->_ApplyAllLevelScaleItemMods(true);
    if (Pet* pet = player->GetPet())
        pet->SynchronizeLevelWithOwner();
    sScriptMgr->OnPlayerLevelChanged(player, previousLevel);

    bool cancelled = player->GetLevel() != 1;
    auto changed = std::make_shared<PrestigeSaveTransaction>();
    if (cancelled)
    {
        RestoreEquippedItems(player, equipment);
        ChatHandler(player->GetSession()).SendSysMessage("Otro modulo impidio bajar de nivel. El prestigio se cancelo sin borrar tus misiones ni contar otro prestigio.");
    }
    else
    {
        ClearQuestProgress(player);
        player->SetUInt32Value(PLAYER_XP, 0);
        player->SetUInt32Value(PLAYER_NEXT_LEVEL_XP, sObjectMgr->GetXPForLevel(1));
        player->RemovePlayerFlag(PLAYER_FLAGS_NO_XP_GAIN);
        player->RemoveAllAuras();
        ResetAllTalents(player);
        player->resetSpells();

        if (_preservePermanentSpells == 1)
        {
            RestorePermanentSpells(player, permanent);
            PrestigioHollow::RestoreSkills(player, skills);
            player->SetFreePrimaryProfessions(freeProfessions);
        }
        else if (_preservePermanentSpells == 2)
        {
            // MODO RETENCION: Se guardan en la DB exclusiva 'prestigio'
            std::vector<SavedSkillData> skData;
            for (auto const& s : skills)
                skData.push_back({s.id, s.step, s.value, s.maximum});
            std::vector<uint32> spData(permanent.begin(), permanent.end());
            PrestigioDatabase::Instance().SaveRetention(guid.GetCounter(), skData, spData);

            // Bloquear y purgar SysAlas de inmediato
            RemoveSysAlasAuras(player);
            PurgeSysAlasActive(guid.GetCounter());
        }

        PrestigioHollow::RestoreLanguages(player, languages);
        RepairDefaultLanguages(player);

        ResetPlayerLevelOne(player);
        GiveStartingOutfit(player);

        for (auto statement : {
            CHAR_DEL_CHAR_ACTION,
            CHAR_DEL_CHAR_SPELL,
            CHAR_DEL_CHAR_GLYPHS,
            CHAR_DEL_CHAR_TALENT,
            CHAR_DEL_CHAR_QUESTSTATUS,
            CHAR_DEL_CHAR_QUESTSTATUS_REWARDED,
            CHAR_DEL_QUEST_STATUS_DAILY_CHAR,
            CHAR_DEL_QUEST_STATUS_WEEKLY_CHAR,
            CHAR_DEL_QUEST_STATUS_MONTHLY_CHAR,
            CHAR_DEL_QUEST_STATUS_SEASONAL_CHAR
        })
        {
            auto query = CharacterDatabase.GetPreparedStatement(statement);
            query->SetData(0, guid.GetCounter());
            changed->Append(query);
        }
        player->SaveToDB(changed, false, true);

        auto const& location = RaceStart.at(race);
        Player::SavePositionInDB(WorldLocation(location.map, location.x, location.y, location.z, location.o), 0, guid, changed);
    }
    auto token = ByteArrayToHexStr(Acore::Crypto::GetRandomBytes<16>());
    WorldSession* session = player->GetSession();
    uint32 accountId = session ? session->GetAccountId() : 0;
    uint32 totalTimePlayed = player->GetTotalPlayedTime();
    if (session)
        session->LogoutPlayer(false);
    PrestigioDatabase::Instance().Save(guid.GetCounter(), accountId, name, previousLevel, playerClass, race,
        totalTimePlayed, token, baseline, changed, cancelled, [this, guid, name, previousLevel](bool success, uint32 count)
    {
        SetBusy(guid.GetCounter(), false);
        sCharacterCache->UpdateCharacterLevel(guid, success ? 1 : previousLevel);
        if (success)
        {
            LOG_INFO("module", "mod-prestigio-hollow: prestigio {} de {} guardado atomicamente en DB 'prestigio'.", count, name);
            SendPrestigeReward(guid, count);
        }
        else
            LOG_ERROR("module", "mod-prestigio-hollow: prestigio de {} cancelado; estado original restaurado.", name);
    });
}

std::vector<PrestigioHollowMgr::PrestigeReward> PrestigioHollowMgr::GetRewardsForPrestige(uint32 prestigeCount) const
{
    auto it = _rewards.find(prestigeCount);
    if (it != _rewards.end())
        return it->second;

    it = _rewards.find(0);
    if (it != _rewards.end())
        return it->second;

    return {};
}

void PrestigioHollowMgr::SendPrestigeReward(ObjectGuid guid, uint32 prestigeCount) const
{
    std::vector<PrestigeReward> rewards = GetRewardsForPrestige(prestigeCount);
    if (rewards.empty())
        return;

    std::vector<Item*> itemsToSend;
    CharacterDatabaseTransaction trans = CharacterDatabase.BeginTransaction();

    for (auto const& r : rewards)
    {
        if (r.itemId == 0)
            continue;
        uint32 count = r.count ? r.count : 1;
        ItemTemplate const* itemTemplate = sObjectMgr->GetItemTemplate(r.itemId);
        if (!itemTemplate)
        {
            LOG_ERROR("module", "mod-prestigio-hollow: Objeto de recompensa {} no existe en item_template.", r.itemId);
            continue;
        }

        Item* item = Item::CreateItem(r.itemId, count);
        if (!item)
        {
            LOG_ERROR("module", "mod-prestigio-hollow: No se pudo crear objeto {} para prestigio {}.", r.itemId, prestigeCount);
            continue;
        }

        item->SaveToDB(trans);
        itemsToSend.push_back(item);
    }

    if (itemsToSend.empty())
        return;

    std::string subject = "Recompensa de Prestigio " + std::to_string(prestigeCount);
    std::string body = "¡Enhorabuena por alcanzar el prestigio " + std::to_string(prestigeCount) + "! Aqui tienes tus recompensas.";

    MailDraft draft(subject, body);
    for (Item* item : itemsToSend)
        draft.AddItem(item);

    draft.SendMailTo(trans, MailReceiver(guid.GetCounter()), MailSender(MAIL_CREATURE, 2069426));
    CharacterDatabase.CommitTransaction(trans);
    LOG_INFO("module", "mod-prestigio-hollow: Enviadas {} recompensas por correo a GUID {} por prestigio {}.",
        itemsToSend.size(), guid.GetCounter(), prestigeCount);
}

uint32 PrestigioHollowMgr::GetPrestigeSpell(uint32 prestigeCount) const
{
    auto it = _spellsByLevel.find(prestigeCount);
    if (it != _spellsByLevel.end())
        return it->second;
    return 0;
}

bool PrestigioHollowMgr::IsPrestigeSpell(uint32 spellId) const
{
    return _allPrestigeSpells.count(spellId) != 0;
}

void PrestigioHollowMgr::EnsurePrestigeSpell(Player* player, uint32 prestigeCount) const
{
    if (!player || player->GetLevel() >= 80)
        return;

    uint32 targetSpell = GetPrestigeSpell(prestigeCount);

    // Remover cualquier spell de prestigio que no corresponda al actual
    for (uint32 s : _allPrestigeSpells)
    {
        if (s != targetSpell)
        {
            if (player->HasSpell(s))
                player->removeSpell(s, SPEC_MASK_ALL, false);
            player->RemoveAura(s);
        }
    }

    if (targetSpell > 0)
    {
        if (!player->HasSpell(targetSpell))
            player->learnSpell(targetSpell, false);
        if (!player->HasAura(targetSpell))
            player->AddAura(targetSpell, player);
    }
}

void PrestigioHollowMgr::RemovePrestigeSpell(Player* player) const
{
    if (!player)
        return;

    for (uint32 s : _allPrestigeSpells)
    {
        if (player->HasSpell(s))
            player->removeSpell(s, SPEC_MASK_ALL, false);
        player->RemoveAura(s);
    }
}

bool PrestigioHollowMgr::IsSysAlasSpell(uint32 spellId) const
{
    return _sysAlasSpells.count(spellId) != 0;
}

void PrestigioHollowMgr::RemoveSysAlasAuras(Player* player) const
{
    if (!player)
        return;
    for (uint32 auraId : _sysAlasSpells)
    {
        if (player->HasAura(auraId))
            player->RemoveAura(auraId);
    }
}

void PrestigioHollowMgr::PurgeSysAlasActive(uint32 guid) const
{
    PrestigioDatabase::Instance().PurgeSysAlasActive(guid);
}

void PrestigioHollowMgr::RestoreRetainedData(Player* player)
{
    if (!player || _preservePermanentSpells != 2)
        return;

    uint32 guid = player->GetGUID().GetCounter();
    PrestigioDatabase::Instance().LoadRetention(guid, [guid](bool success, std::vector<SavedSkillData> skills, std::vector<uint32> spells)
    {
        if (!success || (skills.empty() && spells.empty()))
            return;

        Player* p = ObjectAccessor::FindConnectedPlayer(ObjectGuid::Create<HighGuid::Player>(guid));
        if (!p)
            return;

        for (auto const& sk : skills)
        {
            uint16 currentVal = p->GetPureSkillValue(sk.id);
            uint16 targetVal = std::max(currentVal, sk.value);
            uint16 targetMax = std::max(p->GetPureMaxSkillValue(sk.id), sk.maximum);
            p->SetSkill(sk.id, sk.step, targetVal, targetMax);
        }

        for (uint32 spellId : spells)
        {
            if (sSpellMgr->GetSpellInfo(spellId) && !p->HasSpell(spellId))
                p->learnSpell(spellId, false);
        }

        PrestigioDatabase::Instance().ClearRetention(guid);
        LOG_INFO("module", "mod-prestigio-hollow: Datos retenidos ({} skills, {} spells) restaurados para GUID {}.",
            skills.size(), spells.size(), guid);
    });
}

void PrestigioHollowMgr::ResetPlayerLevelOne(Player* player) const
{
    if (!player)
        return;

    uint8 race = player->getRace();
    uint8 playerClass = player->getClass();
    PlayerInfo const* info = sObjectMgr->GetPlayerInfo(race, playerClass);
    if (!info)
        return;

    ChrClassesEntry const* classEntry = sChrClassesStore.LookupEntry(playerClass);
    uint8 powerType = classEntry ? classEntry->powerType : POWER_MANA;

    if (!player->HasShapeshiftAura())
        player->SetShapeshiftForm(FORM_NONE);

    player->SetFactionForRace(race);
    player->SetUInt32Value(UNIT_FIELD_BYTES_0, (race | (playerClass << 8) | (player->getGender() << 16) | (powerType << 24)));
    if (player->GetShapeshiftForm() == FORM_NONE)
        player->InitDisplayIds();
    player->SetByteValue(UNIT_FIELD_BYTES_2, 1, UNIT_BYTE2_FLAG_PVP);
    player->ReplaceAllUnitFlags(UNIT_FLAG_PLAYER_CONTROLLED);
    player->SetUInt32Value(PLAYER_FIELD_WATCHED_FACTION_INDEX, uint32(-1));

    player->_ApplyAllLevelScaleItemMods(false);
    player->SetLevel(1);
    player->InitRunes();
    player->InitStatsForLevel(true);
    player->InitTaxiNodesForLevel();
    player->InitGlyphsForLevel();
    player->InitTalentForLevel();

    player->SetUInt32Value(PLAYER_XP, 0);
    player->SetUInt32Value(PLAYER_NEXT_LEVEL_XP, sObjectMgr->GetXPForLevel(1));
    player->RemovePlayerFlag(PLAYER_FLAGS_NO_XP_GAIN);

    player->_ApplyAllLevelScaleItemMods(true);

    if (Pet* pet = player->GetPet())
        pet->SynchronizeLevelWithOwner();

    // Resetear skills de armas/defensa que hayan quedado desfasadas (ej. valor=400, max=5)
    auto const skillMap = player->GetSkillStatusMap();
    for (auto const& pair : skillMap)
    {
        uint32 skId = pair.first;
        if (!IsProfessionOrRidingSkill(skId) && skId != SKILL_MOUNTS && skId != SKILL_COMPANIONS && skId != SKILL_RUNEFORGING)
        {
            uint16 maxVal = player->GetPureMaxSkillValue(skId);
            if (maxVal == 0)
                maxVal = 5;
            uint16 curVal = player->GetPureSkillValue(skId);
            if (curVal > maxVal)
                player->SetSkill(skId, player->GetSkillStep(skId), 1, maxVal);
        }
    }

    // Habilidad 183 (Melee/Ataque basico) y Spell 6603 (Ataque)
    player->SetSkill(183, 0, 5, 5);
    player->learnSkillRewardedSpells(183, 1);
    if (!player->HasSpell(6603))
        player->learnSpell(6603, false);

    // Habilidades iniciales de raza y clase con sus spells asociados
    for (auto const& sk : info->skills)
    {
        uint16 maxSkillVal = player->GetMaxSkillValueForLevel();
        if (sk.Rank == 0)
        {
            SkillRaceClassInfoEntry const* rcInfo = GetSkillRaceClassInfo(sk.SkillId, race, playerClass);
            if (rcInfo)
            {
                if (GetSkillRangeType(rcInfo) == SKILL_RANGE_MONO)
                    maxSkillVal = 1;
                else if (GetSkillRangeType(rcInfo) == SKILL_RANGE_LANGUAGE)
                    maxSkillVal = 300;
            }
        }
        player->SetSkill(sk.SkillId, sk.Rank, 1, maxSkillVal);
        player->learnSkillRewardedSpells(sk.SkillId, 1);
    }

    // Hechizos iniciales de clase (ej. Descarga de relampagos, Ola de sanacion, Golpe siniestro, etc.)
    for (uint32 spellId : info->castSpells)
    {
        if (sSpellMgr->GetSpellInfo(spellId) && !player->HasSpell(spellId))
            player->learnSpell(spellId, false);
    }
    player->LearnCustomSpells();

    // Reasignar barras de accion iniciales
    for (uint8 b = 0; b < MAX_ACTION_BUTTONS; ++b)
        player->removeActionButton(b);

    for (auto const& act : info->action)
        player->addActionButton(act.button, act.action, act.type);

    player->SendActionButtons(1);
}

void PrestigioHollowMgr::EnsurePrestigedPlayerReady(Player* player, uint32 prestigeCount) const
{
    if (!player)
        return;

    player->RemovePlayerFlag(PLAYER_FLAGS_NO_XP_GAIN);
    if (player->GetUInt32Value(PLAYER_NEXT_LEVEL_XP) == 0 && player->GetLevel() < 80)
        player->SetUInt32Value(PLAYER_NEXT_LEVEL_XP, sObjectMgr->GetXPForLevel(player->GetLevel()));

    if (player->GetLevel() == 1 && (!player->HasSpell(6603) || player->GetSpellMap().size() <= 2))
    {
        LOG_INFO("module", "mod-prestigio-hollow: Reparando nivel 1, hechizos y barras para {} (GUID {}) con prestigio {}.",
            player->GetName(), player->GetGUID().GetCounter(), prestigeCount);
        ResetPlayerLevelOne(player);
        player->SaveToDB(false, false);
    }
    else if (player->GetLevel() == 1)
    {
        PlayerInfo const* info = sObjectMgr->GetPlayerInfo(player->getRace(), player->getClass());
        if (info)
        {
            bool hasAnyAction = false;
            for (uint8 b = 0; b < MAX_ACTION_BUTTONS; ++b)
            {
                ActionButton const* btn = player->GetActionButton(b);
                if (btn && btn->GetAction() != 0)
                {
                    hasAnyAction = true;
                    break;
                }
            }
            if (!hasAnyAction)
            {
                for (auto const& act : info->action)
                    player->addActionButton(act.button, act.action, act.type);
                player->SendActionButtons(1);
                player->SaveToDB(false, false);
            }
        }
    }

    RepairDefaultLanguages(player);
    EnsurePrestigeSpell(player, prestigeCount);
}
