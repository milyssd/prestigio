"""Compile the actual starting-outfit method against a minimal inventory model."""
from pathlib import Path
import os
import subprocess
import tempfile

root = Path(__file__).resolve().parents[1]
source = (root / 'src/PrestigioHollow.cpp').read_text()
start = source.index('void PrestigioHollowMgr::GiveStartingOutfit(')
end = source.index('void PrestigioHollowMgr::TeleportToRaceStart(', start)
method = source[start:end]
harness = r'''
#include <algorithm>
#include <cassert>
#include <cstdint>
#include <iostream>
#include <map>
using uint32 = std::uint32_t;
using uint8 = std::uint8_t;
constexpr uint8 MAX_OUTFIT_ITEMS = 3;
struct CharStartOutfitEntry { int ItemId[MAX_OUTFIT_ITEMS] = {6948, 100, 0}; };
struct ItemTemplate { uint32 BuyCount; int MaxCount; };
struct TestObjectMgr {
    std::map<uint32, ItemTemplate> items{{6948, {1, 1}}, {100, {5, 0}}};
    ItemTemplate const* GetItemTemplate(uint32 id) const {
        auto it = items.find(id); return it == items.end() ? nullptr : &it->second;
    }
} objects;
TestObjectMgr* sObjectMgr = &objects;
CharStartOutfitEntry outfit;
CharStartOutfitEntry const* GetCharStartOutfitEntry(uint8, uint8, uint8) { return &outfit; }
unsigned warnings = 0;
struct ChatHandler {
    explicit ChatHandler(void*) { }
    template<class... Args> void PSendSysMessage(char const*, Args...) { ++warnings; }
};
struct Player {
    std::map<uint32, uint32> inventory, bank, granted;
    bool fail = false;
    uint8 getRace() const { return 10; }
    uint8 getClass() const { return 2; }
    uint8 getGender() const { return 0; }
    void* GetSession() const { return nullptr; }
    uint32 GetItemCount(uint32 id, bool inBank) const {
        auto a = inventory.find(id), b = bank.find(id);
        return (a == inventory.end() ? 0 : a->second) + (inBank && b != bank.end() ? b->second : 0);
    }
    bool StoreNewItemInBestSlots(uint32 id, uint32 amount) {
        if (fail) return false;
        granted[id] += amount; inventory[id] += amount; return true;
    }
};
struct PrestigioHollowMgr {
    bool _giveStartingOutfit = true;
    void GiveStartingOutfit(Player* player) const;
};
'''
tests = r'''
int main() {
    PrestigioHollowMgr mgr;
    Player existing; existing.inventory[6948] = 1;
    mgr.GiveStartingOutfit(&existing);
    assert(existing.granted[6948] == 0 && existing.granted[100] == 5);

    Player banked; banked.bank[6948] = 1;
    mgr.GiveStartingOutfit(&banked);
    assert(banked.granted[6948] == 0);

    Player missing;
    mgr.GiveStartingOutfit(&missing);
    assert(missing.granted[6948] == 1);
    mgr.GiveStartingOutfit(&missing);
    assert(missing.granted[6948] == 1); // repeated prestige preserves the original unique item

    objects.items[100] = {5, 3};
    Player limited; limited.inventory[100] = 2;
    mgr.GiveStartingOutfit(&limited);
    assert(limited.granted[100] == 1);

    Player over; over.inventory[100] = 9;
    mgr.GiveStartingOutfit(&over);
    assert(over.granted[100] == 0);

    Player failed; failed.fail = true;
    mgr.GiveStartingOutfit(&failed);
    assert(warnings == 2);

    mgr._giveStartingOutfit = false;
    Player disabled;
    mgr.GiveStartingOutfit(&disabled);
    assert(disabled.granted.empty());
    std::cout << "PASS: unique items in inventory/bank, missing item, repeat prestige, limits, failures and disabled outfit\n";
}
'''
with tempfile.TemporaryDirectory(prefix='prestigio-outfit-') as temp:
    test_source = Path(temp) / 'outfit_test.cpp'
    executable = Path(temp) / 'outfit_test'
    test_source.write_text(harness + method + tests)
    subprocess.run([os.environ.get('CXX', 'g++'), '-std=c++20', '-Wall', '-Wextra', '-Werror', str(test_source), '-o', str(executable)], check=True)
    subprocess.run([str(executable)], check=True)
