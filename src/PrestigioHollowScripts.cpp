#include "PrestigioHollow.h"
#include "Chat.h"
#include "Creature.h"
#include "Log.h"
#include "Player.h"
#include "ScriptMgr.h"
#include "ScriptedGossip.h"

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
    void OnStartup() override { sPrestigioHollowMgr->Start(); }
    void OnShutdown() override { sPrestigioHollowMgr->Stop(); }
};

class npc_prestigio_hollow : public CreatureScript
{
public:
    npc_prestigio_hollow() : CreatureScript("npc_prestigio_hollow") { }

    bool OnGossipHello(Player* player, Creature* creature) override
    {
        sPrestigioHollowMgr->RepairDefaultLanguages(player);
        ClearGossipMenuFor(player);
        uint32 count = sPrestigioHollowMgr->GetPrestigeCount(player->GetGUID().GetCounter());
        AddGossipItemFor(player, GOSSIP_ICON_CHAT, "Prestigios actuales: " + std::to_string(count), GOSSIP_SENDER_MAIN, PH_INFO);
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
        return true;
    }

    bool OnGossipSelect(Player* player, Creature* creature, uint32 sender, uint32 action) override
    {
        if (sender != GOSSIP_SENDER_MAIN)
            return false;
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
                "Volverás a nivel 1 y se borrarán las misiones activas y completadas. Tu equipo se guardará en tus bolsas.",
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
    void OnPlayerLogin(Player* player) override { sPrestigioHollowMgr->RepairDefaultLanguages(player); }
    void OnPlayerLevelChanged(Player* player, uint8) override
    {
        if (sPrestigioHollowMgr->IsEnabled() && player->GetLevel() == sPrestigioHollowMgr->GetRequiredLevel())
            ChatHandler(player->GetSession()).PSendSysMessage("|cff8A2BE2[Prestigio Hollow]|r Ya puedes prestigiar.");
    }
};

void AddPrestigioHollowScripts()
{
    new PrestigioHollowWorld();
    new npc_prestigio_hollow();
    new PrestigioHollowPlayer();
    LOG_INFO("module", "mod-prestigio-hollow: NPC npc_prestigio_hollow registrado (menu con volver/cancelar).");
}
