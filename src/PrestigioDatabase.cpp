#include "PrestigioDatabase.h"
#include "Log.h"
#include "MySQLConnection.h"
#include "QueryResult.h"
#include <limits>
#include <mutex>
#include <sstream>
#include <vector>

namespace
{
class PrestigeConnection final : public MySQLConnection
{
public:
    explicit PrestigeConnection(MySQLConnectionInfo& info) : MySQLConnection(info) { }
    bool failed = false;

private:
    void DoPrepareStatements() override { }
    bool _HandleMySQLErrno(uint32 error, char const*, uint8) override
    {
        // Core's default handler can abort the server on schema/network errors.
        // A module failure must only make its own feature unavailable.
        failed = true;
        LOG_ERROR("module", "mod-prestigio-hollow: error MySQL {} en la base propia; operacion cancelada.", error);
        return false;
    }
};
}

struct PrestigioDatabase::Impl
{
    mutable std::mutex mutex;
    std::unique_ptr<MySQLConnectionInfo> info;
    std::unique_ptr<PrestigeConnection> connection;
    bool ready = false;

    void Reset()
    {
        ready = false;
        connection.reset();
        info.reset();
    }
    QueryResult Query(std::string const& sql)
    {
        QueryResult result(connection->Query(sql));
        if (!result || !result->GetRowCount() || !result->NextRow())
            return {};
        return result;
    }
};

PrestigioDatabase::PrestigioDatabase() : _impl(std::make_unique<Impl>()) { }
PrestigioDatabase::~PrestigioDatabase() = default;
PrestigioDatabase& PrestigioDatabase::Instance() { static PrestigioDatabase database; return database; }

bool PrestigioDatabase::Open(std::string const& text)
{
    std::lock_guard<std::mutex> lock(_impl->mutex);
    _impl->Reset();
    std::vector<std::string> fields;
    std::istringstream stream(text);
    std::string part;
    while (std::getline(stream, part, ';')) fields.push_back(part);
    if (fields.size() < 5 || fields.size() > 6 || fields[0].empty() || fields[1].empty() ||
        fields[2].empty() || fields[4] != "mod_prestigio_hollow")
    {
        LOG_ERROR("module", "mod-prestigio-hollow: configura DatabaseInfo con host;puerto;usuario;clave;mod_prestigio_hollow. No se conecto a ninguna base.");
        return false;
    }
    if (fields[0] != ".")
    {
        try
        {
            std::size_t used = 0;
            auto port = std::stoul(fields[1], &used);
            if (used != fields[1].size() || port == 0 || port > 65535) return false;
        }
        catch (...) { return false; }
    }
    _impl->info = std::make_unique<MySQLConnectionInfo>(text);
    _impl->connection = std::make_unique<PrestigeConnection>(*_impl->info);
    if (_impl->connection->Open() != 0)
    {
        _impl->Reset();
        LOG_ERROR("module", "mod-prestigio-hollow: base propia no disponible; worldserver continua y el prestigio queda bloqueado.");
        return false;
    }
    // Validate fields without modifying schema or requiring existing player rows.
    _impl->Query("SELECT player_guid,prestige_count FROM mod_prestigio_hollow_stats LIMIT 0");
    _impl->Query("SELECT history_id,player_guid,prestige_number,prestiged_at,previous_level,class,race FROM mod_prestigio_hollow_history LIMIT 0");
    if (_impl->connection->failed)
    {
        _impl->Reset();
        LOG_ERROR("module", "mod-prestigio-hollow: aplica data/sql/db-prestigio/base/mod_prestigio_hollow.sql en la base propia.");
        return false;
    }
    _impl->ready = true;
    LOG_INFO("module", "mod-prestigio-hollow: base independiente mod_prestigio_hollow conectada.");
    return true;
}
void PrestigioDatabase::Close()
{
    std::lock_guard<std::mutex> lock(_impl->mutex);
    _impl->Reset();
}
bool PrestigioDatabase::IsReady() const
{
    std::lock_guard<std::mutex> lock(_impl->mutex);
    return _impl->ready;
}
bool PrestigioDatabase::GetCount(uint32 guid, uint32& count)
{
    std::lock_guard<std::mutex> lock(_impl->mutex);
    count = 0;
    if (!_impl->ready) return false;
    auto result = _impl->Query("SELECT prestige_count FROM mod_prestigio_hollow_stats WHERE player_guid=" + std::to_string(guid));
    if (_impl->connection->failed) { _impl->Reset(); return false; }
    if (result) count = result->Fetch()[0].Get<uint32>();
    return true;
}
bool PrestigioDatabase::Record(uint32 guid, uint8 previousLevel, uint8 playerClass, uint8 race, uint32& count)
{
    std::lock_guard<std::mutex> lock(_impl->mutex);
    if (!_impl->ready) return false;
    auto& db = *_impl->connection;
    std::string id = std::to_string(guid);
    if (!db.Execute("START TRANSACTION")) { _impl->Reset(); return false; }
    auto cancel = [&]() { db.Execute("ROLLBACK"); _impl->Reset(); return false; };
    if (!db.Execute("INSERT INTO mod_prestigio_hollow_stats(player_guid,prestige_count) VALUES(" + id + ",0) ON DUPLICATE KEY UPDATE player_guid=VALUES(player_guid)")) return cancel();
    auto result = _impl->Query("SELECT prestige_count FROM mod_prestigio_hollow_stats WHERE player_guid=" + id + " FOR UPDATE");
    if (!result || db.failed) return cancel();
    uint32 old = result->Fetch()[0].Get<uint32>();
    if (old == std::numeric_limits<uint32>::max()) return cancel();
    uint32 next = old + 1;
    if (!db.Execute("UPDATE mod_prestigio_hollow_stats SET prestige_count=" + std::to_string(next) + " WHERE player_guid=" + id)) return cancel();
    if (!db.Execute("INSERT INTO mod_prestigio_hollow_history(player_guid,prestige_number,previous_level,class,race) VALUES(" + id + "," + std::to_string(next) + "," + std::to_string(previousLevel) + "," + std::to_string(playerClass) + "," + std::to_string(race) + ")")) return cancel();
    if (!db.Execute("COMMIT")) return cancel();
    count = next;
    return true;
}
