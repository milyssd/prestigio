#ifndef MOD_PRESTIGIO_HOLLOW_LANGUAGES_H
#define MOD_PRESTIGIO_HOLLOW_LANGUAGES_H
#include <algorithm>
#include <cstdint>
#include <vector>

namespace PrestigioHollow
{
struct SavedLanguage
{
    std::uint32_t skill, spell;
    std::uint16_t step, value, maximum;
};
template<class PlayerType, class LanguageRange>
std::vector<SavedLanguage> SnapshotLanguages(PlayerType* player, LanguageRange const& catalog)
{
    std::vector<SavedLanguage> saved;
    for (auto const& language : catalog)
    {
        if (!language.skill_id || !language.spell_id) continue;
        if (!player->HasSkill(language.skill_id) && !player->HasSpell(language.spell_id)) continue;
        bool skillKnown = player->HasSkill(language.skill_id);
        saved.push_back({language.skill_id, language.spell_id,
            skillKnown ? player->GetSkillStep(language.skill_id) : std::uint16_t(0),
            skillKnown ? player->GetPureSkillValue(language.skill_id) : std::uint16_t(300),
            skillKnown ? player->GetPureMaxSkillValue(language.skill_id) : std::uint16_t(300)});
    }
    return saved;
}
template<class PlayerType>
void RestoreLanguages(PlayerType* player, std::vector<SavedLanguage> const& saved)
{
    for (auto const& language : saved)
    {
        if (!player->HasSpell(language.spell)) player->learnSpell(language.spell);
        auto value = std::max<std::uint16_t>(1, language.value);
        player->SetSkill(language.skill, language.step, value, std::max(value, language.maximum));
    }
}
}
#endif
