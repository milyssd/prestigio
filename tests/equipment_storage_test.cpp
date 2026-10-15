#include "../src/EquipmentStorage.h"
#include <algorithm>
#include "TestCheck.h"
#include <iostream>
#include <set>

using Slots = std::vector<std::vector<std::uint16_t>>;

void ExpectFits(Slots const& candidates)
{
    std::vector<std::uint16_t> destinations;
    CHECK(PrestigioHollow::PlanEquipmentStorage(candidates, destinations));
    CHECK(destinations.size() == candidates.size());
    CHECK(std::set<std::uint16_t>(destinations.begin(), destinations.end()).size() == destinations.size());
    for (std::size_t i = 0; i < candidates.size(); ++i)
        CHECK(std::find(candidates[i].begin(), candidates[i].end(), destinations[i]) != candidates[i].end());
}

void ExpectFull(Slots const& candidates)
{
    std::vector<std::uint16_t> destinations{999};
    CHECK(!PrestigioHollow::PlanEquipmentStorage(candidates, destinations));
    CHECK(destinations.empty()); // never expose a partial move plan
}

int main()
{
    ExpectFits({});
    ExpectFits({{0xFF17}});
    ExpectFits({{0xFF17, 0x1300}, {0xFF17, 0x1300}}); // exact capacity, backpack + bag
    ExpectFull({{0xFF17}, {0xFF17}}); // don't double-book a single free slot
    ExpectFull({{0xFF17}, {}}); // specialized bags cannot hold the second item
    ExpectFits({{0xFF17, 0x1300}, {0xFF17}}); // move flexible item to specialized bag
    ExpectFits({{1, 2}, {2, 3}, {1}}); // multiple reassignment steps
    ExpectFull({{1, 2}, {1, 2}, {1, 2}});
    std::cout << "PASS: 8 equipment storage scenarios\n";
}
