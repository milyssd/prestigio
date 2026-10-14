#ifndef MOD_PRESTIGIO_HOLLOW_EQUIPMENT_STORAGE_H
#define MOD_PRESTIGIO_HOLLOW_EQUIPMENT_STORAGE_H

#include <cstdint>
#include <functional>
#include <map>
#include <vector>

namespace PrestigioHollow
{
// Reserve distinct compatible slots before moving anything. Matching also handles
// specialized bags without consuming the only slot suitable for a later item.
inline bool PlanEquipmentStorage(std::vector<std::vector<std::uint16_t>> const& candidates,
    std::vector<std::uint16_t>& destinations)
{
    destinations.clear();
    std::map<std::uint16_t, std::size_t> owners;
    std::function<bool(std::size_t, std::map<std::uint16_t, bool>&)> reserve;
    reserve = [&](std::size_t item, std::map<std::uint16_t, bool>& visited)
    {
        for (std::uint16_t slot : candidates[item])
        {
            if (visited[slot])
                continue;
            visited[slot] = true;
            auto owner = owners.find(slot);
            if (owner == owners.end() || reserve(owner->second, visited))
            {
                owners[slot] = item;
                return true;
            }
        }
        return false;
    };

    for (std::size_t item = 0; item < candidates.size(); ++item)
    {
        std::map<std::uint16_t, bool> visited;
        if (!reserve(item, visited))
            return false;
    }

    destinations.resize(candidates.size());
    for (auto const& owner : owners)
        destinations[owner.second] = owner.first;
    return true;
}
}
#endif
