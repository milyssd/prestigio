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
        std::string label = "Volver a nivel 1 - Prestigio Hollow";
        if (player->GetLevel() < sPrestigioHollowMgr->GetRequiredLevel())
            label += " (requiere nivel " + std::to_string(sPrestigioHollowMgr->GetRequiredLevel()) + ")";
        AddGossipItemFor(player, GOSSIP_ICON_BATTLE, label, GOSSIP_SENDER_MAIN, PH_PRESTIGE);
    }
    else
        AddGossipItemFor(player, GOSSIP_ICON_CHAT, "El modo prestigio está desactivado.", GOSSIP_SENDER_MAIN, PH_INFO);
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
                "Volverás a nivel 1 y se borrarán las misiones activas y completadas. Tu equipo se guardará en tus bolsas. Volverás a la selección de personajes mientras se guarda.",
                GOSSIP_SENDER_MAIN, PH_INFO);
            AddGossipItemFor(player, GOSSIP_ICON_BATTLE, "CONFIRMAR PRESTIGIO", GOSSIP_SENDER_MAIN, PH_CONFIRM);
            AddGossipItemFor(player, GOSSIP_ICON_CHAT, "Volver al menú principal (cancelar)", GOSSIP_SENDER_MAIN, PH_BACK);
            SendGossipMenuFor(player, PH_GOSSIP_TEXT, creature->GetGUID());
            return true;
        }
        if (action == PH_CONFIRM)
        {
            // Failed checks report the cause and keep navigation available.
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
    void OnPlayerLogout(Player* player) override { sPrestigioHollowMgr->CancelMenuRequest(player->GetGUID().GetCounter()); }
    void OnPlayerLogin(Player* player) override { sPrestigioHollowMgr->RepairDefaultLanguages(player); }
    void OnPlayerLevelChanged(Player* player, uint8) override
    {
        if (sPrestigioHollowMgr->IsEnabled() && player->GetLevel() == sPrestigioHollowMgr->GetRequiredLevel())
            ChatHandler(player->GetSession()).PSendSysMessage("|cff8A2BE2[Prestigio Hollow]|r Ya puedes prestigiar.");
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
    LOG_INFO("module", "mod-prestigio-hollow: NPC npc_prestigio_hollow registrado (menu con volver/cancelar).");
}
