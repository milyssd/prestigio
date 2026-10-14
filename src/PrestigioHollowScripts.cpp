#include "PrestigioHollow.h"
#include "Chat.h"
#include "Creature.h"
#include "Player.h"
#include "ScriptMgr.h"
#include "ScriptedGossip.h"

constexpr uint32 PH_GOSSIP_TEXT = 2069426;

enum PHAction : uint32
{
    PH_INFO     = GOSSIP_ACTION_INFO_DEF + 1,
    PH_PRESTIGE = GOSSIP_ACTION_INFO_DEF + 2,
    PH_CONFIRM  = GOSSIP_ACTION_INFO_DEF + 3
};
class PrestigioHollowWorld : public WorldScript
{
public: PrestigioHollowWorld():WorldScript("PrestigioHollowWorld"){}
void OnAfterConfigLoad(bool) override { sPrestigioHollowMgr->LoadConfig(); }
};
class npc_prestigio_hollow : public CreatureScript
{
public: npc_prestigio_hollow():CreatureScript("npc_prestigio_hollow"){}
bool OnGossipHello(Player* p, Creature* c) override
{
    ClearGossipMenuFor(p);
    uint32 n=sPrestigioHollowMgr->GetPrestigeCount(p->GetGUID().GetCounter());
    AddGossipItemFor(p,GOSSIP_ICON_CHAT,"Prestigios actuales: "+std::to_string(n),GOSSIP_SENDER_MAIN,PH_INFO);
    if (sPrestigioHollowMgr->IsEnabled() && p->GetLevel()>=sPrestigioHollowMgr->GetRequiredLevel())
        AddGossipItemFor(p,GOSSIP_ICON_BATTLE,"Volver a nivel 1 - Prestigio Hollow",GOSSIP_SENDER_MAIN,PH_PRESTIGE);
    else if (!sPrestigioHollowMgr->IsEnabled())
        AddGossipItemFor(p,GOSSIP_ICON_CHAT,"El modo prestigio está desactivado.",GOSSIP_SENDER_MAIN,PH_INFO);
    else AddGossipItemFor(p,GOSSIP_ICON_CHAT,"Necesitas nivel "+std::to_string(sPrestigioHollowMgr->GetRequiredLevel())+".",GOSSIP_SENDER_MAIN,PH_INFO);
    AddGossipItemFor(p,GOSSIP_ICON_CHAT,"Necesitas un hueco libre en tus bolsas por cada pieza equipada. No se usa el buzón.",GOSSIP_SENDER_MAIN,PH_INFO);
    SendGossipMenuFor(p,PH_GOSSIP_TEXT,c->GetGUID()); return true;
}
bool OnGossipSelect(Player* p, Creature* c, uint32 sender, uint32 a) override
{
    if (sender != GOSSIP_SENDER_MAIN)
        return false;
    if (a == PH_INFO)
        return OnGossipHello(p, c);
    ClearGossipMenuFor(p);
    if(a==PH_PRESTIGE){
        AddGossipItemFor(p,GOSSIP_ICON_CHAT,"Volverás a nivel 1. Se borrarán TODAS las misiones activas y completadas. Tu equipo se guardará en tus bolsas; si no cabe, se cancela. Objetos y logros se conservan.",GOSSIP_SENDER_MAIN,PH_INFO);
        AddGossipItemFor(p,GOSSIP_ICON_BATTLE,"CONFIRMAR PRESTIGIO",GOSSIP_SENDER_MAIN,PH_CONFIRM);
        SendGossipMenuFor(p,PH_GOSSIP_TEXT,c->GetGUID()); return true; }
    CloseGossipMenuFor(p);
    if (a == PH_CONFIRM)
        sPrestigioHollowMgr->Prestige(p);
    return true;
}
};
class PrestigioHollowPlayer : public PlayerScript
{
public: PrestigioHollowPlayer():PlayerScript("PrestigioHollowPlayer"){}
void OnPlayerLevelChanged(Player* p,uint8) override { if(sPrestigioHollowMgr->IsEnabled() && p->GetLevel()==sPrestigioHollowMgr->GetRequiredLevel()) ChatHandler(p->GetSession()).PSendSysMessage("|cff8A2BE2[Prestigio Hollow]|r Ya puedes prestigiar."); }
};
void AddPrestigioHollowScripts(){ new PrestigioHollowWorld(); new npc_prestigio_hollow(); new PrestigioHollowPlayer(); }
