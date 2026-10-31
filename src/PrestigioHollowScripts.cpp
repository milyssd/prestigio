#include "PrestigioHollow.h"
#include "Chat.h"
#include "Creature.h"
#include "Log.h"
#include "Player.h"
#include "ScriptMgr.h"
#include "ScriptedGossip.h"
#include "WorldSession.h"
#include "WorldPacket.h"
#include "Opcodes.h"
#include "Spell.h"
#include "SpellInfo.h"

constexpr uint32 PH_GOSSIP_TEXT = 2069426;

enum PHAction : uint32
{
    PH_INFO = GOSSIP_ACTION_INFO_DEF + 1,
    PH_PRESTIGE = GOSSIP_ACTION_INFO_DEF + 2,
    PH_CONFIRM = GOSSIP_ACTION_INFO_DEF + 3,
    PH_BACK = GOSSIP_ACTION_INFO_DEF + 4,
    PH_CLOSE = GOSSIP_ACTION_INFO_DEF + 5
};

class PrestigioHollowWorld : public WorldScript
{
public:
    PrestigioHollowWorld() : WorldScript("PrestigioHollowWorld") { }
    void OnAfterConfigLoad(bool) override { sPrestigioHollowMgr->LoadConfig(); }
    void OnUpdate(uint32) override { sPrestigioHollowMgr->Update(); }
    void OnStartup() override { sPrestigioHollowMgr->Start(); }
    void OnShutdown() override { sPrestigioHollowMgr->Stop(); }
};

namespace
{
void ShowPrestigeMenu(Player* player, Creature* creature, std::optional<uint32> count, bool loading = false)
{
    ClearGossipMenuFor(player);
    std::string counter = loading ? "Consultando prestigios..." : count ? "Prestigios actuales: " + std::to_string(*count) : "Contador de prestigios no disponible";
    AddGossipItemFor(player, GOSSIP_ICON_CHAT, counter, GOSSIP_SENDER_MAIN, PH_INFO);
    if (sPrestigioHollowMgr->IsEnabled())
    {
        if (player->getClass() == CLASS_DEATH_KNIGHT)
        {
            AddGossipItemFor(player, GOSSIP_ICON_CHAT, "Los Caballeros de la Muerte no pueden prestigiar.", GOSSIP_SENDER_MAIN, PH_INFO);
        }
        else if (!sPrestigioHollowMgr->HasCompletedRequiredQuest(player))
        {
            AddGossipItemFor(player, GOSSIP_ICON_CHAT, "Debes completar la mision requerida para acceder al prestigio.", GOSSIP_SENDER_MAIN, PH_INFO);
        }
        else if (sPrestigioHollowMgr->GetMaxPrestige() > 0 && count && *count >= sPrestigioHollowMgr->GetMaxPrestige())
        {
            AddGossipItemFor(player, GOSSIP_ICON_CHAT, "Has alcanzado el nivel maximo de prestigios (" + std::to_string(sPrestigioHollowMgr->GetMaxPrestige()) + ").", GOSSIP_SENDER_MAIN, PH_INFO);
        }
        else
        {
            std::string label = "Volver a nivel 1 - Prestigio Hollow";
            if (player->GetLevel() < sPrestigioHollowMgr->GetRequiredLevel())
                label += " (requiere nivel " + std::to_string(sPrestigioHollowMgr->GetRequiredLevel()) + ")";
            AddGossipItemFor(player, GOSSIP_ICON_BATTLE, label, GOSSIP_SENDER_MAIN, PH_PRESTIGE);
        }
    }
    else
        AddGossipItemFor(player, GOSSIP_ICON_CHAT, "El modo prestigio esta desactivado.", GOSSIP_SENDER_MAIN, PH_INFO);
    AddGossipItemFor(player, GOSSIP_ICON_CHAT, "Necesitas un hueco libre en tus bolsas por pieza equipada.", GOSSIP_SENDER_MAIN, PH_INFO);
    AddGossipItemFor(player, GOSSIP_ICON_CHAT, "Cerrar", GOSSIP_SENDER_MAIN, PH_CLOSE);
    SendGossipMenuFor(player, PH_GOSSIP_TEXT, creature->GetGUID());
}
}

class npc_prestigio_hollow : public CreatureScript
{
public:
    npc_prestigio_hollow() : CreatureScript("npc_prestigio_hollow") { }
    bool OnGossipHello(Player* player, Creature* creature) override
    {
        if (creature->IsQuestGiver())
        {
            player->PrepareQuestMenu(creature->GetGUID());
            if (!sPrestigioHollowMgr->HasCompletedRequiredQuest(player))
            {
                if (player->PlayerTalkClass->GetQuestMenu().GetMenuItemCount() > 0)
                {
                    player->SendPreparedQuest(creature->GetGUID());
                    return true;
                }
            }
        }

        if (!sPrestigioHollowMgr->HasCompletedRequiredQuest(player))
        {
            ClearGossipMenuFor(player);
            AddGossipItemFor(player, GOSSIP_ICON_CHAT, "Debes completar la mision requerida para acceder al prestigio.", GOSSIP_SENDER_MAIN, PH_INFO);
            AddGossipItemFor(player, GOSSIP_ICON_CHAT, "Cerrar", GOSSIP_SENDER_MAIN, PH_CLOSE);
            SendGossipMenuFor(player, PH_GOSSIP_TEXT, creature->GetGUID());
            return true;
        }

        sPrestigioHollowMgr->RepairDefaultLanguages(player);
        ShowPrestigeMenu(player, creature, std::nullopt, true);
        sPrestigioHollowMgr->RequestCount(player, creature, [](Player* current, Creature* source, std::optional<uint32> count)
        {
            ShowPrestigeMenu(current, source, count);
        });
        return true;
    }

    bool OnGossipSelect(Player* player, Creature* creature, uint32 sender, uint32 action) override
    {
        if (sender != GOSSIP_SENDER_MAIN)
            return false;
        sPrestigioHollowMgr->CancelMenuRequest(player->GetGUID().GetCounter());
        if (action == PH_INFO || action == PH_BACK)
            return OnGossipHello(player, creature);
        if (action == PH_CLOSE)
        {
            CloseGossipMenuFor(player);
            return true;
        }
        if (action == PH_PRESTIGE)
        {
            if (!sPrestigioHollowMgr->CanPrestige(player))
                return OnGossipHello(player, creature);

            ClearGossipMenuFor(player);
            AddGossipItemFor(player, GOSSIP_ICON_CHAT,
                "Volveras a nivel 1 y se borraran las misiones activas y completadas. Tu equipo se guardara en tus bolsas. Volveras a la seleccion de personajes mientras se guarda.",
                GOSSIP_SENDER_MAIN, PH_INFO);
            AddGossipItemFor(player, GOSSIP_ICON_BATTLE, "CONFIRMAR PRESTIGIO", GOSSIP_SENDER_MAIN, PH_CONFIRM);
            AddGossipItemFor(player, GOSSIP_ICON_CHAT, "Volver al menu principal (cancelar)", GOSSIP_SENDER_MAIN, PH_BACK);
            SendGossipMenuFor(player, PH_GOSSIP_TEXT, creature->GetGUID());
            return true;
        }
        if (action == PH_CONFIRM)
        {
            if (!sPrestigioHollowMgr->Prestige(player))
                return OnGossipHello(player, creature);
            CloseGossipMenuFor(player);
            return true;
        }
        return false;
    }
};

class PrestigioHollowPlayer : public PlayerScript
{
public:
    PrestigioHollowPlayer() : PlayerScript("PrestigioHollowPlayer") { }

    void OnPlayerLogout(Player* player) override
    {
        sPrestigioHollowMgr->CancelMenuRequest(player->GetGUID().GetCounter());
    }

    void OnPlayerLogin(Player* player) override
    {
        if (player->getClass() == CLASS_DEATH_KNIGHT)
            return;

        sPrestigioHollowMgr->RepairDefaultLanguages(player);

        uint32 guid = player->GetGUID().GetCounter();
        sPrestigioDatabase->GetCount(guid, [guid](bool success, uint32 count)
        {
            if (!success)
                return;

            Player* p = ObjectAccessor::FindConnectedPlayer(ObjectGuid::Create<HighGuid::Player>(guid));
            if (!p)
                return;

            if (p->GetLevel() >= 80)
            {
                // Al alcanzar el nivel 80 se remueve el spell identificador de prestigio
                sPrestigioHollowMgr->RemovePrestigeSpell(p);
                // Si tenia habilidades o monturas retenidas, se restauran (solo para jugadores con prestigio)
                if (count > 0)
                    sPrestigioHollowMgr->RestoreRetainedData(p);
            }
            else if (count > 0)
            {
                // Niveles 1 al 79: asegurar personaje listo (spells iniciales, barras, stats, xp) y spell identificador
                sPrestigioHollowMgr->EnsurePrestigedPlayerReady(p, count);
                // Niveles 1 al 79: remover cualquier aura de alas de SysAlas
                sPrestigioHollowMgr->RemoveSysAlasAuras(p);
            }
        });
    }

    void OnPlayerLevelChanged(Player* player, uint8) override
    {
        if (player->getClass() == CLASS_DEATH_KNIGHT)
            return;

        uint32 guid = player->GetGUID().GetCounter();

        if (player->GetLevel() >= 80)
        {
            // 1. Quitar el spell identificador de prestigio (solo activo del 1 al 79)
            sPrestigioHollowMgr->RemovePrestigeSpell(player);

            // 2. Restaurar datos y notificar solo a jugadores que ya tengan prestigio (prestigio >= 1)
            sPrestigioDatabase->GetCount(guid, [guid](bool success, uint32 count)
            {
                if (!success || count == 0)
                    return;

                Player* p = ObjectAccessor::FindConnectedPlayer(ObjectGuid::Create<HighGuid::Player>(guid));
                if (!p)
                    return;

                sPrestigioHollowMgr->RestoreRetainedData(p);

                if (sPrestigioHollowMgr->IsEnabled() && p->GetLevel() >= sPrestigioHollowMgr->GetRequiredLevel())
                {
                    if (sPrestigioHollowMgr->GetMaxPrestige() == 0 || count < sPrestigioHollowMgr->GetMaxPrestige())
                    {
                        ChatHandler(p->GetSession()).PSendSysMessage(
                            "|cff8A2BE2[Prestigio Hollow]|r ¡Felicidades por alcanzar el nivel 80! Ya puedes volver a prestigiar.");
                    }
                }
            });
        }
        else
        {
            // Niveles 1 al 79: asegurar que no tenga alas de SysAlas y tenga su spell de rango
            sPrestigioDatabase->GetCount(guid, [guid](bool success, uint32 count)
            {
                if (success && count > 0)
                {
                    Player* p = ObjectAccessor::FindConnectedPlayer(ObjectGuid::Create<HighGuid::Player>(guid));
                    if (p && p->GetLevel() < 80)
                    {
                        sPrestigioHollowMgr->EnsurePrestigeSpell(p, count);
                        sPrestigioHollowMgr->RemoveSysAlasAuras(p);
                    }
                }
            });
        }
    }

    void OnPlayerTalentsReset(Player* player, bool) override
    {
        if (player->getClass() == CLASS_DEATH_KNIGHT || player->GetLevel() >= 80)
            return;

        uint32 guid = player->GetGUID().GetCounter();
        sPrestigioDatabase->GetCount(guid, [guid](bool success, uint32 count)
        {
            if (success && count > 0)
            {
                Player* p = ObjectAccessor::FindConnectedPlayer(ObjectGuid::Create<HighGuid::Player>(guid));
                if (p && p->GetLevel() < 80)
                    sPrestigioHollowMgr->EnsurePrestigeSpell(p, count);
            }
        });
    }

    void OnPlayerReleasedGhost(Player* player) override
    {
        if (player->getClass() == CLASS_DEATH_KNIGHT || player->GetLevel() >= 80)
            return;

        uint32 guid = player->GetGUID().GetCounter();
        sPrestigioDatabase->GetCount(guid, [guid](bool success, uint32 count)
        {
            if (success && count > 0)
            {
                Player* p = ObjectAccessor::FindConnectedPlayer(ObjectGuid::Create<HighGuid::Player>(guid));
                if (p && p->GetLevel() < 80)
                {
                    sPrestigioHollowMgr->EnsurePrestigeSpell(p, count);
                    sPrestigioHollowMgr->RemoveSysAlasAuras(p);
                }
            }
        });
    }

    void OnPlayerSpellCast(Player* player, Spell* spell, bool) override
    {
        if (!player || player->GetLevel() >= 80 || !spell)
            return;

        SpellInfo const* spellInfo = spell->GetSpellInfo();
        if (!spellInfo)
            return;

        // Si es un spell de SysAlas y el jugador esta en niveles 1 a 79 con prestigio activo
        if (sPrestigioHollowMgr->IsSysAlasSpell(spellInfo->Id))
        {
            uint32 guid = player->GetGUID().GetCounter();
            sPrestigioDatabase->GetCount(guid, [guid, spellInfo](bool success, uint32 count)
            {
                if (success && count > 0)
                {
                    Player* p = ObjectAccessor::FindConnectedPlayer(ObjectGuid::Create<HighGuid::Player>(guid));
                    if (p && p->GetLevel() < 80)
                    {
                        sPrestigioHollowMgr->RemoveSysAlasAuras(p);
                        ChatHandler(p->GetSession()).PSendSysMessage("|cffFF0000[SysAlas]|r No puedes activar alas mientras estas leveando en prestigio (niveles 1 a 79).");
                    }
                }
            });
        }
    }
};

class PrestigioHollowServer : public ServerScript
{
public:
    PrestigioHollowServer() : ServerScript("PrestigioHollowServer") { }
    bool CanPacketReceive(WorldSession* session, WorldPacket const& packet) override
    {
        if (!session || (packet.GetOpcode() != CMSG_PLAYER_LOGIN && packet.GetOpcode() != CMSG_CHAR_DELETE) || packet.size() < 8)
            return true;
        WorldPacket copy(packet);
        ObjectGuid guid;
        copy >> guid;
        if (!sPrestigioHollowMgr->IsBusy(guid.GetCounter()))
            return true;
        if (packet.GetOpcode() == CMSG_PLAYER_LOGIN)
            session->SendCharLoginFailed(LoginFailureReason::DuplicateCharacter);
        else
        {
            WorldPacket response(SMSG_CHAR_DELETE, 1);
            response << uint8(CHAR_DELETE_FAILED);
            session->SendPacket(&response);
        }
        return false;
    }
};

void AddPrestigioHollowScripts()
{
    new PrestigioHollowWorld();
    new npc_prestigio_hollow();
    new PrestigioHollowPlayer();
    new PrestigioHollowServer();
    LOG_INFO("module", "mod-prestigio-hollow: Modulo de prestigio inicializado correctamente con DB aislada.");
}
