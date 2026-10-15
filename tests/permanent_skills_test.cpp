#include "../src/PermanentSkills.h"
#include "TestCheck.h"
#include <map>
struct Player
{
    struct Skill { std::uint16_t step, value, maximum; bool removed=false; };
    std::map<std::uint32_t,Skill> skills;
    auto const& GetSkillStatusMap() const { return skills; }
    bool HasSkill(std::uint32_t id) const { return skills.contains(id) && !skills.at(id).removed; }
    auto GetSkillStep(std::uint32_t id) const { return skills.at(id).step; }
    auto GetPureSkillValue(std::uint32_t id) const { return skills.at(id).value; }
    auto GetPureMaxSkillValue(std::uint32_t id) const { return skills.at(id).maximum; }
    void SetSkill(std::uint32_t id, std::uint16_t step, std::uint16_t value, std::uint16_t maximum) { skills[id]={step,value,maximum}; }
};
int main()
{
    Player p;
    p.skills={{171,{6,423,450}}, {185,{6,450,450}}, {762,{4,300,300}}, {43,{0,400,400}}, {164,{6,400,450,true}}};
    auto saved=PrestigioHollow::SnapshotSkills(&p, [](auto id){return id!=43;});
    CHECK(saved.size()==3);
    p.skills.clear();
    p.skills[171]={6,1,450}; // relearning a rank must not erase the original progress
    PrestigioHollow::RestoreSkills(&p,saved);
    CHECK(p.skills.at(171).value==423 && p.skills.at(171).step==6 && p.skills.at(171).maximum==450);
    CHECK(p.skills.at(185).value==450 && p.skills.at(762).value==300);
    CHECK(!p.skills.contains(43) && !p.skills.contains(164));
    CHECK(PrestigioHollow::SnapshotSkills(&p, [](auto){return false;}).empty());
    std::cout<<"PASS: partial/max profession progress, riding, removed skills and disabled preservation\n";
}
