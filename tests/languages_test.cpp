#include "../src/LanguagePreservation.h"
#include "TestCheck.h"
#include <iostream>
#include <map>
#include <set>
using U32 = std::uint32_t;
using U16 = std::uint16_t;
struct Language { U32 skill_id, spell_id; };
struct Skill { U16 step, value, maximum; };
struct Player
{
    std::map<U32, Skill> skills;
    std::set<U32> spells;
    bool HasSkill(U32 id) const { return skills.contains(id); }
    bool HasSpell(U32 id) const { return spells.contains(id); }
    U16 GetSkillStep(U32 id) const { return skills.at(id).step; }
    U16 GetPureSkillValue(U32 id) const { return skills.at(id).value; }
    U16 GetPureMaxSkillValue(U32 id) const { return skills.at(id).maximum; }
    void learnSpell(U32 id) { spells.insert(id); }
    void SetSkill(U32 id, U16 step, U16 value, U16 maximum) { skills[id]={step,value,maximum}; }
};
int main()
{
    Language catalog[] = {{0,0},{98,668},{109,669},{137,813}};
    Player player;
    player.skills[137]={0,300,300}; player.spells.insert(813);
    auto saved = PrestigioHollow::SnapshotLanguages(&player,catalog);
    CHECK(saved.size()==1);
    player.skills.clear(); player.spells.clear(); // simulate resetSpells at level 1
    PrestigioHollow::RestoreLanguages(&player,saved);
    CHECK(player.HasSpell(813) && player.HasSkill(137) && player.skills[137].value==300);
    CHECK(!player.HasSpell(668) && !player.HasSpell(669)); // no extra faction languages

    Player skillOnly; skillOnly.skills[98]={2,200,300};
    saved=PrestigioHollow::SnapshotLanguages(&skillOnly,catalog);
    skillOnly.skills.clear(); PrestigioHollow::RestoreLanguages(&skillOnly,saved);
    CHECK(skillOnly.HasSpell(668) && skillOnly.skills[98].step==2 && skillOnly.skills[98].value==200);

    Player spellOnly; spellOnly.spells.insert(669);
    saved=PrestigioHollow::SnapshotLanguages(&spellOnly,catalog);
    spellOnly.spells.clear(); PrestigioHollow::RestoreLanguages(&spellOnly,saved);
    CHECK(spellOnly.HasSkill(109) && spellOnly.HasSpell(669) && spellOnly.skills[109].maximum==300);

    Player empty;
    saved=PrestigioHollow::SnapshotLanguages(&empty,catalog);
    PrestigioHollow::RestoreLanguages(&empty,saved);
    CHECK(empty.skills.empty() && empty.spells.empty());
    std::cout << "PASS: spoken languages survive reset, partial language state is repaired, unknown languages stay unknown\n";
}
