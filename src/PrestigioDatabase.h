#ifndef MOD_PRESTIGIO_HOLLOW_DATABASE_H
#define MOD_PRESTIGIO_HOLLOW_DATABASE_H

#include "CharacterDatabase.h"
#include "Transaction.h"
#include <functional>
#include <memory>
#include <string>

// SaveToDB accepts this typed transaction. Only its worker reads the captured values.
class PrestigeSaveTransaction : public Transaction<CharacterDatabaseConnection>
{
public:
    std::vector<SQLElementData> const& Statements() const { return m_queries; }
};

class PrestigioDatabase
{
public:
    using CountCallback = std::function<void(bool, uint32)>;
    static PrestigioDatabase& Instance();
    ~PrestigioDatabase();
    void Start(std::string const& characterConnectionInfo, bool enabled = true);
    void SetEnabled(bool enabled);
    void Stop();
    void Poll();
    bool IsReady() const;
    void GetCount(uint32 guid, CountCallback callback);
    void Check(std::function<void(bool)> callback);
    void Save(uint32 guid, uint8 previousLevel, uint8 playerClass, uint8 race,
        std::string operationId, std::shared_ptr<PrestigeSaveTransaction> baseline,
        std::shared_ptr<PrestigeSaveTransaction> transaction, bool cancelOnly, CountCallback callback);

private:
    PrestigioDatabase();
    struct Impl;
    std::unique_ptr<Impl> _impl;
};
#endif
