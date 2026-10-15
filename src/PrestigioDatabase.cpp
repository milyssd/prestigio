#include "PrestigioDatabase.h"
#include "Log.h"
#include "MySQLWorkaround.h"
#include "PreparedStatement.h"
#include "QueryResult.h"
#include <algorithm>
#include <atomic>
#include <chrono>
#include <condition_variable>
#include <deque>
#include <limits>
#include <mutex>
#include <thread>

namespace
{
constexpr char Stats[] = "`mod_prestigio_hollow`.`mod_prestigio_hollow_stats`";
constexpr char Operations[] = "`mod_prestigio_hollow`.`mod_prestigio_hollow_operations`";
constexpr char History[] = "`mod_prestigio_hollow`.`mod_prestigio_hollow_history`";

class PrestigeConnection final : public CharacterDatabaseConnection
{
public:
    explicit PrestigeConnection(MySQLConnectionInfo& info) : CharacterDatabaseConnection(nullptr, info), _info(info) { }
    bool failed = false;
    uint32 error = 0;

    uint32 Open() override
    {
        Close();
        failed = false;
        error = 0;
        m_prepareError = false;
        MYSQL* handle = mysql_init(nullptr);
        if (!handle)
            return 1;
        unsigned int timeout = 3;
        mysql_options(handle, MYSQL_OPT_CONNECT_TIMEOUT, &timeout);
        mysql_options(handle, MYSQL_OPT_READ_TIMEOUT, &timeout);
        mysql_options(handle, MYSQL_OPT_WRITE_TIMEOUT, &timeout);
        mysql_options(handle, MYSQL_SET_CHARSET_NAME, "utf8mb4");
        unsigned int port = 0;
        char const* socket = nullptr;
        std::string host = _info.host;
        if (host == ".")
        {
#ifdef _WIN32
            unsigned int protocol = MYSQL_PROTOCOL_PIPE;
#else
            unsigned int protocol = MYSQL_PROTOCOL_SOCKET;
            host = "localhost";
            socket = _info.port_or_socket.c_str();
#endif
            mysql_options(handle, MYSQL_OPT_PROTOCOL, &protocol);
        }
        else
        {
            try
            {
                std::size_t used = 0;
                auto number = std::stoul(_info.port_or_socket, &used);
                if (used != _info.port_or_socket.size() || !number || number > 65535)
                {
                    mysql_close(handle);
                    return 1;
                }
                port = static_cast<unsigned int>(number);
            }
            catch (...)
            {
                mysql_close(handle);
                return 1;
            }
        }
        if (!_info.ssl.empty())
        {
            mysql_ssl_mode ssl = _info.ssl == "ssl" ? SSL_MODE_REQUIRED : SSL_MODE_DISABLED;
            mysql_options(handle, MYSQL_OPT_SSL_MODE, &ssl);
        }
        if (!mysql_real_connect(handle, host.c_str(), _info.user.c_str(), _info.password.c_str(),
            _info.database.c_str(), port, socket, 0))
        {
            error = mysql_errno(handle);
            mysql_close(handle);
            return error ? error : 1;
        }
        m_Mysql = reinterpret_cast<MySQLHandle*>(handle);
        mysql_autocommit(handle, 1);
        if (!PrepareStatements())
        {
            Close();
            return error ? error : 1;
        }
        return 0;
    }

private:
    bool _HandleMySQLErrno(uint32 number, char const*, uint8) override
    {
        failed = true;
        error = number;
        LOG_ERROR("module", "mod-prestigio-hollow: MySQL {}. Operacion cancelada; no se detiene worldserver.", number);
        return false;
    }
    MySQLConnectionInfo& _info;
};

QueryResult Query(PrestigeConnection& connection, std::string const& sql)
{
    QueryResult result(connection.Query(sql));
    if (!result || !result->GetRowCount() || !result->NextRow())
        return {};
    return result;
}

bool SchemaReady(PrestigeConnection& connection)
{
    Query(connection, std::string("SELECT player_guid,prestige_count FROM ") + Stats + " LIMIT 0");
    Query(connection, std::string("SELECT operation_id,player_guid,prestige_number,prestiged_at,previous_level,class,race FROM ") + History + " LIMIT 0");
    Query(connection, std::string("SELECT operation_id,player_guid,completed,prestige_count FROM ") + Operations + " LIMIT 0");
    if (connection.failed)
        return false;
    auto engines = Query(connection,
        "SELECT COUNT(*),COUNT(IF(ENGINE='InnoDB',1,NULL)) FROM information_schema.TABLES "
        "WHERE TABLE_SCHEMA='mod_prestigio_hollow' AND TABLE_NAME IN ('mod_prestigio_hollow_stats','mod_prestigio_hollow_history','mod_prestigio_hollow_operations')");
    if (!engines || engines->Fetch()[0].Get<uint64>() != 3 || engines->Fetch()[1].Get<uint64>() != 3)
        return false;
    auto native = Query(connection,
        "SELECT COUNT(*) FROM information_schema.TABLES WHERE TABLE_SCHEMA=DATABASE() AND ENGINE<>'InnoDB' "
        "AND (TABLE_NAME='characters' OR LEFT(TABLE_NAME,10)='character_' OR TABLE_NAME IN ('item_instance','mail','mail_items'))");
    if (!native || native->Fetch()[0].Get<uint64>() != 0)
        return false;
    // Validate write grants without creating dummy rows or changing player data.
    return connection.Execute(std::string("INSERT INTO ") + Stats + " (player_guid,prestige_count) SELECT 0,0 WHERE FALSE") &&
        connection.Execute(std::string("UPDATE ") + Stats + " SET prestige_count=prestige_count WHERE FALSE") &&
        connection.Execute(std::string("INSERT INTO ") + History +
            " (player_guid,prestige_number,previous_level,class,race,operation_id) SELECT 0,0,0,0,0,NULL WHERE FALSE") &&
        connection.Execute(std::string("INSERT INTO ") + Operations +
            " (operation_id,player_guid,completed,prestige_count) SELECT '',0,0,0 WHERE FALSE");
}

bool ReadCount(PrestigeConnection& connection, uint32 guid, uint32& count)
{
    count = 0;
    auto result = Query(connection, std::string("SELECT prestige_count FROM ") + Stats + " WHERE player_guid=" + std::to_string(guid));
    if (connection.failed)
        return false;
    if (result)
        count = result->Fetch()[0].Get<uint32>();
    return true;
}

enum class Outcome { Committed, Cancelled, Aborted, Unknown };

Outcome SaveOnce(PrestigeConnection& connection, uint32 guid, uint8 previousLevel, uint8 playerClass,
    uint8 race, std::string const& operationId, PrestigeSaveTransaction const& baseline,
    PrestigeSaveTransaction const& transaction, bool cancelOnly, uint32& count)
{
    if (!connection.Execute("START TRANSACTION"))
        return Outcome::Unknown;
    auto cancel = [&connection]()
    {
        return connection.Execute("ROLLBACK") ? Outcome::Aborted : Outcome::Unknown;
    };
    auto id = std::to_string(guid);
    if (!connection.Execute(std::string("INSERT INTO ") + Stats + " (player_guid,prestige_count) VALUES(" + id +
        ",0) ON DUPLICATE KEY UPDATE player_guid=VALUES(player_guid)"))
        return cancel();
    // The row lock serializes retries with an earlier connection whose COMMIT response was lost.
    auto receipt = Query(connection, std::string("SELECT completed,prestige_count FROM ") + Operations +
        " WHERE operation_id='" + operationId + "' AND player_guid=" + id);
    if (connection.failed)
        return cancel();
    if (receipt)
    {
        bool completed = receipt->Fetch()[0].Get<uint8>() != 0;
        count = receipt->Fetch()[1].Get<uint32>();
        connection.Execute("ROLLBACK");
        return completed ? Outcome::Committed : Outcome::Cancelled;
    }
    uint32 old = 0;
    if (!ReadCount(connection, guid, old) || (!cancelOnly && old == std::numeric_limits<uint32>::max()))
        return cancel();
    auto execute = [&connection](PrestigeSaveTransaction const& captured)
    {
        for (auto const& statement : captured.Statements())
        {
            bool success = statement.type == SQL_ELEMENT_RAW
                ? connection.Execute(std::get<std::string>(statement.element))
                : connection.Execute(std::get<PreparedStatementBase*>(statement.element));
            if (!success)
                return false;
        }
        return true;
    };
    // The original save inserts any unsaved NEW items before the reset's updates.
    // On a rejected reset, this same snapshot restores the last live character state.
    if (!execute(baseline) || (!cancelOnly && !execute(transaction)))
        return cancel();
    uint32 next = cancelOnly ? old : old + 1;
    if (!cancelOnly && (!connection.Execute(std::string("UPDATE ") + Stats + " SET prestige_count=" + std::to_string(next) + " WHERE player_guid=" + id) ||
        !connection.Execute(std::string("INSERT INTO ") + History +
            " (player_guid,prestige_number,previous_level,class,race,operation_id) VALUES(" + id + "," + std::to_string(next) +
            "," + std::to_string(previousLevel) + "," + std::to_string(playerClass) + "," + std::to_string(race) + ",'" + operationId + "')")))
        return cancel();
    if (!connection.Execute(std::string("INSERT INTO ") + Operations +
        " (operation_id,player_guid,completed,prestige_count) VALUES('" + operationId + "'," + id + "," +
        (cancelOnly ? "0" : "1") + "," + std::to_string(next) + ")"))
        return cancel();
    // Unlike ExecuteTransaction, check COMMIT and resolve an unknown result with the durable receipt.
    if (!connection.Execute("COMMIT"))
        return Outcome::Unknown;
    count = next;
    return cancelOnly ? Outcome::Cancelled : Outcome::Committed;
}
}

struct PrestigioDatabase::Impl
{
    using Job = std::function<void()>;
    mutable std::mutex mutex;
    std::condition_variable wake;
    std::deque<Job> jobs, saves, completed;
    std::thread worker;
    std::atomic<bool> ready{false};
    std::atomic<bool> enabled{true};
    bool stopping = true;
    std::string settings;
    std::unique_ptr<MySQLConnectionInfo> info;
    std::unique_ptr<PrestigeConnection> connection;

    bool Connect()
    {
        if (connection && !connection->failed && ready)
            return true;
        ready = false;
        connection.reset();
        info = std::make_unique<MySQLConnectionInfo>(settings);
        if (info->database.empty() || info->database == "mod_prestigio_hollow" || info->user.empty())
            return false;
        connection = std::make_unique<PrestigeConnection>(*info);
        if (connection->Open() || !SchemaReady(*connection))
        {
            connection.reset();
            LOG_ERROR("module", "mod-prestigio-hollow: revisar SQL atomico, InnoDB y permisos sobre la base propia en la instancia de personajes.");
            return false;
        }
        ready = true;
        return true;
    }
    bool Enqueue(Job job)
    {
        std::lock_guard<std::mutex> lock(mutex);
        if (stopping || jobs.size() >= 1024)
            return false;
        jobs.push_back(std::move(job));
        wake.notify_one();
        return true;
    }
    void Complete(Job job)
    {
        std::lock_guard<std::mutex> lock(mutex);
        completed.push_back(std::move(job));
    }
    void Run()
    {
        if (enabled)
            Connect();
        for (;;)
        {
            Job job;
            {
                std::unique_lock<std::mutex> lock(mutex);
                wake.wait_for(lock, std::chrono::seconds(5), [&]() { return stopping || !jobs.empty() || !saves.empty(); });
                if (stopping && jobs.empty() && saves.empty())
                    break;
                if (!saves.empty())
                {
                    job = std::move(saves.front());
                    saves.pop_front();
                }
                else if (!jobs.empty())
                {
                    job = std::move(jobs.front());
                    jobs.pop_front();
                }
            }
            if (job)
                job();
            else if (!enabled)
            {
                connection.reset();
                ready = false;
            }
            else if (Connect())
            {
                connection->Ping();
                if (connection->failed)
                {
                    ready = false;
                    connection.reset();
                }
            }
        }
        connection.reset();
        info.reset();
        ready = false;
        mysql_thread_end();
    }
};

PrestigioDatabase::PrestigioDatabase() : _impl(std::make_unique<Impl>()) { }
PrestigioDatabase::~PrestigioDatabase() { Stop(); }
PrestigioDatabase& PrestigioDatabase::Instance() { static PrestigioDatabase database; return database; }
void PrestigioDatabase::Start(std::string const& characterConnectionInfo, bool enabled)
{
    Stop();
    _impl->settings = characterConnectionInfo;
    _impl->enabled = enabled;
    _impl->stopping = false;
    _impl->worker = std::thread([this]() { _impl->Run(); });
}
void PrestigioDatabase::SetEnabled(bool enabled)
{
    _impl->enabled = enabled;
    if (!enabled)
        _impl->ready = false;
    _impl->wake.notify_one();
}
void PrestigioDatabase::Stop()
{
    {
        std::lock_guard<std::mutex> lock(_impl->mutex);
        _impl->stopping = true;
        _impl->jobs.clear();
        _impl->wake.notify_all();
    }
    if (_impl->worker.joinable())
        _impl->worker.join();
    std::lock_guard<std::mutex> lock(_impl->mutex);
    _impl->jobs.clear();
    _impl->completed.clear();
}
bool PrestigioDatabase::IsReady() const { return _impl->ready; }
void PrestigioDatabase::Poll()
{
    std::deque<Impl::Job> completed;
    {
        std::lock_guard<std::mutex> lock(_impl->mutex);
        completed.swap(_impl->completed);
    }
    for (auto& callback : completed)
        callback();
}
void PrestigioDatabase::GetCount(uint32 guid, CountCallback callback)
{
    if (!_impl->Enqueue([this, guid, callback]()
    {
        uint32 count = 0;
        bool success = false;
        for (unsigned attempt = 0; attempt < 2 && !success; ++attempt)
            if (_impl->enabled && _impl->Connect())
                success = ReadCount(*_impl->connection, guid, count);
        _impl->ready = success;
        _impl->Complete([callback, success, count]() { callback(success, count); });
    }))
        _impl->Complete([callback]() { callback(false, 0); });
}
void PrestigioDatabase::Check(std::function<void(bool)> callback)
{
    if (!_impl->Enqueue([this, callback]()
    {
        bool success = _impl->enabled && _impl->Connect() && SchemaReady(*_impl->connection);
        _impl->ready = success;
        _impl->Complete([callback, success]() { callback(success); });
    }))
        _impl->Complete([callback]() { callback(false); });
}
void PrestigioDatabase::Save(uint32 guid, uint8 previousLevel, uint8 playerClass, uint8 race,
    std::string operationId, std::shared_ptr<PrestigeSaveTransaction> baseline,
    std::shared_ptr<PrestigeSaveTransaction> transaction, bool cancelOnly, CountCallback callback)
{
    if (operationId.size() != 32 || !std::all_of(operationId.begin(), operationId.end(),
        [](unsigned char c) { return (c >= '0' && c <= '9') || (c >= 'a' && c <= 'f') || (c >= 'A' && c <= 'F'); }))
    {
        _impl->Complete([callback]() { callback(false, 0); });
        return;
    }
    // A captured character is already offline. Never discard its save because the read queue is full.
    std::lock_guard<std::mutex> lock(_impl->mutex);
    _impl->saves.push_back([this, guid, previousLevel, playerClass, race, operationId = std::move(operationId), baseline, transaction, cancelOnly, callback]() mutable
    {
        uint32 count = 0;
        unsigned aborted = 0;
        for (;;)
        {
            if (_impl->Connect())
            {
                auto outcome = SaveOnce(*_impl->connection, guid, previousLevel, playerClass, race,
                    operationId, *baseline, *transaction, cancelOnly, count);
                if (outcome == Outcome::Committed || outcome == Outcome::Cancelled)
                {
                    bool success = outcome == Outcome::Committed;
                    _impl->ready = true;
                    _impl->Complete([callback, success, count]() { callback(success, count); });
                    return;
                }
                if (outcome == Outcome::Aborted && ++aborted >= 2)
                    cancelOnly = true;
                _impl->connection.reset();
                _impl->ready = false;
            }
            // An unknown result must remain pending until the receipt/row lock resolves it.
            // World updates continue; only this character's login is held back.
            std::unique_lock<std::mutex> guard(_impl->mutex);
            if (_impl->wake.wait_for(guard, std::chrono::seconds(2), [this]() { return _impl->stopping; }))
                return;
        }
    });
    _impl->wake.notify_one();
}
