"""Run the production ApplyPrestige/ResetAllTalents/restore methods with a core state model.
SQL durability is checked separately by database_integration.cpp against real MySQL.
"""
from pathlib import Path
import os,subprocess,tempfile
root=Path(__file__).resolve().parents[1]
source=(root/'src/PrestigioHollow.cpp').read_text()
def method(name):
    start=source.index('void PrestigioHollowMgr::'+name+'(')
    brace=source.index('{',start);depth=1;end=brace+1
    while depth:
        depth += (source[end]=='{')-(source[end]=='}');end+=1
    return source[start:end]+'\n'
harness=r'''
#include "TestCheck.h"
#include "PermanentSkills.h"
#include "LanguagePreservation.h"
#include <array>
#include <functional>
#include <map>
#include <memory>
#include <string>
#include <unordered_set>
#include <vector>
using uint8=std::uint8_t;using uint16=std::uint16_t;using uint32=std::uint32_t;
constexpr unsigned PLAYERSPELL_REMOVED=9,SPEC_MASK_ALL=3,MAX_ACTION_BUTTONS=144,PLAYER_XP=0,SERVER_MSG_STRING=0;
constexpr unsigned SKILL_MOUNTS=777,SKILL_COMPANIONS=778,SKILL_RUNEFORGING=776;
constexpr int CHAR_DEL_QUEST_STATUS_DAILY_CHAR=1,CHAR_DEL_QUEST_STATUS_WEEKLY_CHAR=2,
CHAR_DEL_QUEST_STATUS_MONTHLY_CHAR=3,CHAR_DEL_QUEST_STATUS_SEASONAL_CHAR=4,CHAR_DEL_CHAR_ACTION=5;
#define LOG_INFO(...) ((void)0)
#define LOG_ERROR(...) ((void)0)
struct Guid{uint32 GetCounter()const{return 42;}};
struct Snapshot {uint8 level=80;uint16 profession=423;unsigned quests=5;bool equipped=true;};
struct Query{int id;void SetData(int,uint32){}};
struct PrestigeSaveTransaction{Snapshot state;bool allActions=false,position=false;void Append(Query* q){if(q->id==5)allActions=true;}};
struct DB{Query* GetPreparedStatement(int id){static Query q;q.id=id;return &q;}} CharacterDatabase;
struct Player;
struct WorldSession {bool loggedOut=false;void LogoutPlayer(bool save){CHECK(!save);loggedOut=true;}} session;
struct Player{
    struct Skill{uint16 step,value,max;};struct Talent{unsigned State=0,mask=3;};
    uint8 level=80;uint16 freeProf=0;unsigned quests=5;bool equipped=true,veto=false;
    std::map<uint32,Skill> skills{{171,{6,423,450}},{185,{6,450,450}},{137,{0,300,300}}};
    std::unordered_set<uint32> spells{1001,1002,813};
    Talent first{0,1},second{0,2};std::map<uint32,Talent*> talents{{1,&first},{2,&second}};
    unsigned buttons=144;
    void alive()const{CHECK(!session.loggedOut);}
    Guid GetGUID()const{alive();return {};}
    WorldSession* GetSession(){alive();return &session;}
    uint8 getClass()const{alive();return 2;}uint8 getRace()const{alive();return 10;}
    uint8 GetLevel()const{alive();return level;}
    std::string GetName()const{alive();return "probe";}
    auto const& GetSkillStatusMap()const{alive();return skills;}
    bool HasSkill(uint32 id)const{alive();return skills.contains(id);}
    uint16 GetSkillStep(uint32 id)const{alive();return skills.at(id).step;}
    uint16 GetPureSkillValue(uint32 id)const{alive();return skills.at(id).value;}
    uint16 GetPureMaxSkillValue(uint32 id)const{alive();return skills.at(id).max;}
    uint16 GetFreePrimaryProfessionPoints()const{alive();return freeProf;}
    void SetFreePrimaryProfessions(uint16 value){alive();freeProf=value;}
    void SetSkill(uint32 id,uint16 step,uint16 value,uint16 max){alive();skills[id]={step,value,max};}
    bool HasSpell(uint32 id)const{alive();return spells.contains(id);}
    void learnSpell(uint32 id){alive();spells.insert(id);if(id==1001){skills[171]={6,1,450};freeProf=1;}}
    void GiveLevel(uint8 value){alive();if(!veto){level=value;resetTalents(true);}}
    void resetTalents(bool){alive();first.State=PLAYERSPELL_REMOVED;first.mask=0;}
    auto const& GetTalentMap()const{alive();return talents;}
    void _removeTalent(uint32 id,uint8 mask){alive();talents[id]->mask &= ~mask;if(!talents[id]->mask)talents[id]->State=PLAYERSPELL_REMOVED;}
    void InitTalentForLevel(){alive();}
    void removeActionButton(uint8){alive();if(buttons)--buttons;}
    void SendActionButtons(uint32){alive();}
    void resetSpells(){alive();spells.clear();skills.clear();freeProf=2;}
    void SetUInt32Value(uint32,uint32){alive();}
    void SaveToDB(std::shared_ptr<PrestigeSaveTransaction> const& t,bool,bool){alive();t->state={level,GetPureSkillValue(171),quests,equipped};}
    static void SavePositionInDB(int,int,Guid,std::shared_ptr<PrestigeSaveTransaction> const& t){t->position=true;}
};
struct SpellMgr{void const* GetSpellInfo(uint32)const{return this;}} spellMgr;auto*sSpellMgr=&spellMgr;
struct ChatHandler{explicit ChatHandler(WorldSession*){}void SendSysMessage(char const*){}};
struct Cache{void UpdateCharacterLevel(Guid,uint8){}}cache;auto*sCharacterCache=&cache;
unsigned announcements=0;
struct Sessions{void SendServerMessage(uint32,std::string const&){++announcements;}}sessions;auto*sWorldSessionMgr=&sessions;
struct Language{uint32 skill_id,spell_id;};std::vector<Language>lang_description{{137,813}};
bool IsProfessionOrRidingSkill(uint32 id){return id==171||id==185;}
struct StartLoc{uint32 map;float x,y,z,o;};std::map<uint8,StartLoc>RaceStart{{10,{530,1,2,3,0}}};
int WorldLocation(uint32,float,float,float,float){return 0;}
namespace Acore::Crypto{template<unsigned N>std::array<uint8,N>GetRandomBytes(){return {};}}
template<class T>std::string ByteArrayToHexStr(T const&){return std::string(32,'a');}
struct PrestigioDatabase{
    bool cancel=false;std::shared_ptr<PrestigeSaveTransaction>original,changed;std::function<void(bool,uint32)>complete;
    static auto& Instance(){static PrestigioDatabase db;return db;}
    void Save(uint32,uint8,uint8,uint8,std::string,std::shared_ptr<PrestigeSaveTransaction>b,std::shared_ptr<PrestigeSaveTransaction>t,bool c,std::function<void(bool,uint32)>cb){
        CHECK(session.loggedOut);original=b;changed=t;cancel=c;complete=cb;}
};
struct PrestigioHollowMgr{
    using EquipmentMovePlan=std::vector<int>;bool _preservePermanentSpells=true,_announceWorld=true,busy=true;
    bool PlanEquippedItems(Player*,EquipmentMovePlan&)const{return true;}
    void StoreEquippedItems(Player*p,EquipmentMovePlan const&)const{p->equipped=false;}
    void RestoreEquippedItems(Player*p,EquipmentMovePlan const&)const{p->equipped=true;}
    void SetBusy(uint32,bool value){busy=value;}
    auto SnapshotPermanentSpells(Player*p)const{return p->spells;}
    void RestorePermanentSpells(Player*,std::unordered_set<uint32>const&)const;
    void ClearQuestProgress(Player*p)const{p->quests=0;}
    void RepairDefaultLanguages(Player*)const{}
    void GiveStartingOutfit(Player*)const{}
    void ResetAllTalents(Player*)const;
    void ApplyPrestige(Player*);
};
'''
tests=r'''
int main(){
    PrestigioHollowMgr mgr;Player player;
    mgr.ApplyPrestige(&player);
    auto&db=PrestigioDatabase::Instance();
    CHECK(session.loggedOut && mgr.busy && announcements==0 && !db.cancel);
    CHECK(db.original->state.level==80 && db.original->state.profession==423 && db.original->state.quests==5 && db.original->state.equipped);
    CHECK(db.changed->state.level==1 && db.changed->state.profession==423 && db.changed->state.quests==0 && !db.changed->state.equipped);
    CHECK(player.freeProf==0 && player.skills.at(185).value==450 && player.skills.at(137).value==300);
    CHECK(player.first.State==PLAYERSPELL_REMOVED && player.second.State==PLAYERSPELL_REMOVED);
    CHECK(player.buttons==0 && db.changed->allActions && db.changed->position);
    db.complete(true,1);CHECK(!mgr.busy && announcements==1);
    session.loggedOut=false;Player veto;veto.veto=true;mgr.busy=true;
    mgr.ApplyPrestige(&veto);CHECK(db.cancel && db.original->state.level==80 && veto.quests==5 && veto.equipped);
    db.complete(false,0);CHECK(!mgr.busy && announcements==1);
    std::cout<<"PASS: production reset preserves skills/languages, clears both talent specs/actions, defers success and cancels a veto before quests\n";
}
'''
with tempfile.TemporaryDirectory(prefix='prestigio-flow-') as temp:
    cpp=Path(temp)/'flow.cpp';exe=Path(temp)/'flow'
    cpp.write_text(harness+method('RestorePermanentSpells')+method('ResetAllTalents')+method('ApplyPrestige')+tests)
    subprocess.run([os.environ.get('CXX','g++'),'-std=c++20','-DNDEBUG','-Wall','-Wextra','-Werror','-I'+str(root/'tests'),'-I'+str(root/'src'),str(cpp),'-o',str(exe)],check=True)
    subprocess.run([str(exe)],check=True)
