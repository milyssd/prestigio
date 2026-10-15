#ifndef MOD_PRESTIGIO_HOLLOW_PERMANENT_SKILLS_H
#define MOD_PRESTIGIO_HOLLOW_PERMANENT_SKILLS_H
#include <cstdint>
#include <vector>

namespace PrestigioHollow
{
struct SavedSkill
{
    std::uint32_t id;
    std::uint16_t step, value, maximum;
};
template<class PlayerType, class Predicate>
std::vector<SavedSkill> SnapshotSkills(PlayerType* player, Predicate preserve)
{
    std::vector<SavedSkill> saved;
    for (auto const& skill : player->GetSkillStatusMap())
    {
        auto id = skill.first;
        if (player->HasSkill(id) && preserve(id))
            saved.push_back({id, player->GetSkillStep(id), player->GetPureSkillValue(id), player->GetPureMaxSkillValue(id)});
    }
    return saved;
}
template<class PlayerType>
void RestoreSkills(PlayerType* player, std::vector<SavedSkill> const& saved)
{
    for (auto const& skill : saved)
        player->SetSkill(skill.id, skill.step, skill.value, skill.maximum);
}
}
#endif
