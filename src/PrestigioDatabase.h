#ifndef MOD_PRESTIGIO_HOLLOW_DATABASE_H
#define MOD_PRESTIGIO_HOLLOW_DATABASE_H

#include "CharacterDatabase.h"
#include "Transaction.h"
#include <functional>
#include <memory>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

class PrestigeSaveTransaction : public Transaction<CharacterDatabaseConnection>
{
public:
    std::vector<SQLElementData> const& Statements() const { return m_queries; }
};

struct SavedSkillData
{
    uint32 id;
    uint16 step, value, maximum;
};

struct PrestigeDbReward
{
    uint32 itemId;
    uint32 count;
};

class PrestigioDatabase
{
public:
    using CountCallback = std::function<void(bool, uint32)>;
    using RetentionCallback = std::function<void(bool, std::vector<SavedSkillData>, std::vector<uint32>)>;
    using ConfigCallback = std::function<void(bool, std::unordered_map<uint32, uint32>, std::unordered_map<uint32, std::vector<PrestigeDbReward>>, std::unordered_set<uint32>)>;

    static PrestigioDatabase& Instance();
    ~PrestigioDatabase();

    void Start(std::string const& connectionInfo, std::string const& dbName, std::string const& sysAlasDb, bool enabled = true);
    void SetEnabled(bool enabled);
    void Stop();
    void Poll();
    bool IsReady() const;

    void GetCount(uint32 guid, CountCallback callback);
    void Check(std::function<void(bool)> callback);
    void LoadConfigData(ConfigCallback callback);

    void Save(uint32 guid, uint32 accountId, std::string const& playerName, uint8 previousLevel,
        uint8 playerClass, uint8 race, uint32 totalTimePlayed,
        std::string operationId, std::shared_ptr<PrestigeSaveTransaction> baseline,
        std::shared_ptr<PrestigeSaveTransaction> transaction, bool cancelOnly,
        CountCallback callback);

    void SaveRetention(uint32 guid, std::vector<SavedSkillData> const& skills, std::vector<uint32> const& spells);
    void LoadRetention(uint32 guid, RetentionCallback callback);
    void ClearRetention(uint32 guid);
    void PurgeSysAlasActive(uint32 guid);

private:
    PrestigioDatabase();
    struct Impl;
    std::unique_ptr<Impl> _impl;
};

#define sPrestigioDatabase (&PrestigioDatabase::Instance())

#endif
