#include "PrestigioDatabase.h"
#include "PrestigioHollow.h"
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

inline std::string Table(std::string const& db, std::string const& tbl)
{
    return "`" + db + "`.`" + tbl + "`";
}

bool SchemaReady(PrestigeConnection& connection, std::string const& db)
{
    // Garantizar que la base de datos exclusiva del sistema de prestigio exista
    connection.Execute("CREATE DATABASE IF NOT EXISTS `" + db + "` DEFAULT CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci");

    // Garantizar que todas las tablas del modulo existan en la base de datos 'prestigio'
    connection.Execute("CREATE TABLE IF NOT EXISTS " + Table(db, "mod_prestigio_hollow_stats") + " ("
        "`player_guid` INT UNSIGNED NOT NULL, `prestige_count` INT UNSIGNED NOT NULL DEFAULT 0, "
        "`last_prestiged_at` TIMESTAMP NULL DEFAULT NULL, PRIMARY KEY (`player_guid`)) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4");

    connection.Execute("CREATE TABLE IF NOT EXISTS " + Table(db, "mod_prestigio_hollow_history") + " ("
        "`history_id` BIGINT UNSIGNED NOT NULL AUTO_INCREMENT, `operation_id` CHAR(32) CHARACTER SET ascii COLLATE ascii_bin NULL, "
        "`player_guid` INT UNSIGNED NOT NULL, `account_id` INT UNSIGNED NOT NULL DEFAULT 0, "
        "`player_name` VARCHAR(12) NOT NULL DEFAULT '', `prestige_number` INT UNSIGNED NOT NULL, "
        "`prestiged_at` TIMESTAMP NOT NULL DEFAULT CURRENT_TIMESTAMP, `previous_level` TINYINT UNSIGNED NOT NULL, "
        "`class` TINYINT UNSIGNED NOT NULL, `race` TINYINT UNSIGNED NOT NULL, `total_time_played` INT UNSIGNED NOT NULL DEFAULT 0, "
        "PRIMARY KEY (`history_id`), UNIQUE KEY `uq_prestigio_operation` (`operation_id`), "
        "KEY `idx_player_guid` (`player_guid`)) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4");

    connection.Execute("CREATE TABLE IF NOT EXISTS " + Table(db, "mod_prestigio_hollow_operations") + " ("
        "`operation_id` CHAR(32) CHARACTER SET ascii COLLATE ascii_bin NOT NULL, `player_guid` INT UNSIGNED NOT NULL, "
        "`completed` TINYINT UNSIGNED NOT NULL, `prestige_count` INT UNSIGNED NOT NULL, "
        "`created_at` TIMESTAMP NOT NULL DEFAULT CURRENT_TIMESTAMP, PRIMARY KEY (`operation_id`)) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4");

    connection.Execute("CREATE TABLE IF NOT EXISTS " + Table(db, "mod_prestigio_hollow_spells") + " ("
        "`prestige_level` INT UNSIGNED NOT NULL, `spell_id` INT UNSIGNED NOT NULL, "
        "PRIMARY KEY (`prestige_level`)) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4");

    connection.Execute("CREATE TABLE IF NOT EXISTS " + Table(db, "mod_prestigio_hollow_rewards") + " ("
        "`id` INT UNSIGNED NOT NULL AUTO_INCREMENT, `prestige_level` INT UNSIGNED NOT NULL, "
        "`reward_item_id` INT UNSIGNED NOT NULL DEFAULT 0, `reward_item_count` INT UNSIGNED NOT NULL DEFAULT 1, "
        "PRIMARY KEY (`id`), KEY `idx_prestige_level` (`prestige_level`)) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4");

    connection.Execute("CREATE TABLE IF NOT EXISTS " + Table(db, "mod_prestigio_hollow_retention_skills") + " ("
        "`player_guid` INT UNSIGNED NOT NULL, `skill_id` INT UNSIGNED NOT NULL, "
        "`step` SMALLINT UNSIGNED NOT NULL, `value` SMALLINT UNSIGNED NOT NULL, `maximum` SMALLINT UNSIGNED NOT NULL, "
        "PRIMARY KEY (`player_guid`, `skill_id`)) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4");

    connection.Execute("CREATE TABLE IF NOT EXISTS " + Table(db, "mod_prestigio_hollow_retention_spells") + " ("
        "`player_guid` INT UNSIGNED NOT NULL, `spell_id` INT UNSIGNED NOT NULL, "
        "`spell_type` TINYINT UNSIGNED NOT NULL DEFAULT 0, PRIMARY KEY (`player_guid`, `spell_id`)) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4");

    return true;
}

bool ReadCount(PrestigeConnection& connection, std::string const& db, uint32 guid, uint32& count)
{
    count = 0;
    auto result = Query(connection, "SELECT prestige_count FROM " + Table(db, "mod_prestigio_hollow_stats") + " WHERE player_guid=" + std::to_string(guid));
    if (connection.failed)
        return false;
    if (result)
        count = result->Fetch()[0].Get<uint32>();
    return true;
}

enum class Outcome { Committed, Cancelled, Aborted, Unknown };

Outcome SaveOnce(PrestigeConnection& connection, std::string const& db, uint32 guid, uint32 accountId, std::string const& playerName,
    uint8 previousLevel, uint8 playerClass, uint8 race, uint32 totalTimePlayed,
    std::string const& operationId, PrestigeSaveTransaction const& baseline,
    PrestigeSaveTransaction const& transaction, bool cancelOnly, uint32& count)
{
    std::string const Stats = Table(db, "mod_prestigio_hollow_stats");
    std::string const Operations = Table(db, "mod_prestigio_hollow_operations");
    std::string const History = Table(db, "mod_prestigio_hollow_history");

    if (!connection.Execute("START TRANSACTION"))
        return Outcome::Unknown;
    auto cancel = [&connection]()
    {
        return connection.Execute("ROLLBACK") ? Outcome::Aborted : Outcome::Unknown;
    };
    auto id = std::to_string(guid);
    if (!connection.Execute("INSERT INTO " + Stats + " (player_guid,prestige_count) VALUES(" + id +
        ",0) ON DUPLICATE KEY UPDATE player_guid=VALUES(player_guid)"))
        return cancel();

    auto receipt = Query(connection, "SELECT completed,prestige_count FROM " + Operations +
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
    if (!ReadCount(connection, db, guid, old) || (!cancelOnly && old == std::numeric_limits<uint32>::max()))
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

    auto const& toExecute = cancelOnly ? baseline : transaction;
    if (!execute(toExecute))
        return cancel();

    uint32 next = cancelOnly ? old : old + 1;
    if (!cancelOnly)
    {
        std::string safeName = playerName;
        safeName.erase(std::remove_if(safeName.begin(), safeName.end(), [](char c) {
            return c == '\'' || c == '\\' || c == '"';
        }), safeName.end());

        if (!connection.Execute("UPDATE " + Stats + " SET prestige_count=" + std::to_string(next) +
                ", last_prestiged_at=NOW() WHERE player_guid=" + id) ||
            !connection.Execute("INSERT INTO " + History +
                " (player_guid,account_id,player_name,prestige_number,previous_level,class,race,total_time_played,operation_id) "
                "VALUES(" + id + "," + std::to_string(accountId) + ",'" + safeName + "'," + std::to_string(next) +
                "," + std::to_string(previousLevel) + "," + std::to_string(playerClass) + "," + std::to_string(race) +
                "," + std::to_string(totalTimePlayed) + ",'" + operationId + "')"))
            return cancel();
    }

    if (!connection.Execute("INSERT INTO " + Operations +
        " (operation_id,player_guid,completed,prestige_count) VALUES('" + operationId + "'," + id + "," +
        (cancelOnly ? "0" : "1") + "," + std::to_string(next) + ")"))
        return cancel();

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
    std::string databaseName{"prestigio"};
    std::string sysAlasDb{"sysalas"};
    std::unique_ptr<MySQLConnectionInfo> info;
    std::unique_ptr<PrestigeConnection> connection;

    bool Connect()
    {
        if (connection && !connection->failed && ready)
            return true;
        ready = false;
        connection.reset();
        info = std::make_unique<MySQLConnectionInfo>(settings);
        if (info->database.empty() || info->user.empty())
            return false;
        connection = std::make_unique<PrestigeConnection>(*info);
        if (connection->Open() || !SchemaReady(*connection, databaseName))
        {
            connection.reset();
            LOG_ERROR("module", "mod-prestigio-hollow: no se pudo inicializar schema en base de datos '{}'.", databaseName);
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

void PrestigioDatabase::Start(std::string const& connectionInfo, std::string const& dbName, std::string const& sysAlasDb, bool enabled)
{
    Stop();
    _impl->settings = connectionInfo;
    _impl->databaseName = dbName.empty() ? "prestigio" : dbName;
    _impl->sysAlasDb = sysAlasDb.empty() ? "sysalas" : sysAlasDb;
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
                success = ReadCount(*_impl->connection, _impl->databaseName, guid, count);
        _impl->ready = success;
        _impl->Complete([callback, success, count]() { callback(success, count); });
    }))
        _impl->Complete([callback]() { callback(false, 0); });
}

void PrestigioDatabase::Check(std::function<void(bool)> callback)
{
    if (!_impl->Enqueue([this, callback]()
    {
        bool success = _impl->enabled && _impl->Connect() && SchemaReady(*_impl->connection, _impl->databaseName);
        _impl->ready = success;
        _impl->Complete([callback, success]() { callback(success); });
    }))
        _impl->Complete([callback]() { callback(false); });
}

void PrestigioDatabase::LoadConfigData(ConfigCallback callback)
{
    _impl->Enqueue([this, callback]()
    {
        std::unordered_map<uint32, uint32> spells;
        std::unordered_map<uint32, std::vector<PrestigeDbReward>> rewards;
        std::unordered_set<uint32> sysAlasSpells;
        bool success = false;

        if (_impl->Connect())
        {
            success = true;
            // 1. Cargar spells permanentes para niveles 1-79
            auto spellRes = Query(*_impl->connection, "SELECT prestige_level, spell_id FROM " + Table(_impl->databaseName, "mod_prestigio_hollow_spells"));
            if (spellRes)
            {
                do
                {
                    Field* fields = spellRes->Fetch();
                    spells[fields[0].Get<uint32>()] = fields[1].Get<uint32>();
                } while (spellRes->NextRow());
            }

            // 2. Cargar multiples recompensas por nivel de prestigio
            auto rewRes = Query(*_impl->connection, "SELECT prestige_level, reward_item_id, reward_item_count FROM " + Table(_impl->databaseName, "mod_prestigio_hollow_rewards") + " ORDER BY id ASC");
            if (rewRes)
            {
                do
                {
                    Field* fields = rewRes->Fetch();
                    uint32 level = fields[0].Get<uint32>();
                    uint32 itemId = fields[1].Get<uint32>();
                    uint32 count = fields[2].Get<uint32>();
                    if (itemId > 0)
                        rewards[level].push_back({itemId, count ? count : 1});
                } while (rewRes->NextRow());
            }

            // 3. Cargar auras de alas de SysAlas (si la tabla existe)
            auto wingsRes = Query(*_impl->connection, "SELECT aura_id FROM " + Table(_impl->sysAlasDb, "sysalas_wings") + " WHERE aura_id > 0");
            if (wingsRes)
            {
                do
                {
                    Field* fields = wingsRes->Fetch();
                    sysAlasSpells.insert(fields[0].Get<uint32>());
                } while (wingsRes->NextRow());
            }
        }

        _impl->Complete([callback, success, spells, rewards, sysAlasSpells]() mutable
        {
            callback(success, std::move(spells), std::move(rewards), std::move(sysAlasSpells));
        });
    });
}

void PrestigioDatabase::Save(uint32 guid, uint32 accountId, std::string const& playerName,
    uint8 previousLevel, uint8 playerClass, uint8 race, uint32 totalTimePlayed,
    std::string operationId, std::shared_ptr<PrestigeSaveTransaction> baseline,
    std::shared_ptr<PrestigeSaveTransaction> transaction, bool cancelOnly, CountCallback callback)
{
    if (operationId.size() != 32 || !std::all_of(operationId.begin(), operationId.end(),
        [](unsigned char c) { return (c >= '0' && c <= '9') || (c >= 'a' && c <= 'f') || (c >= 'A' && c <= 'F'); }))
    {
        _impl->Complete([callback]() { callback(false, 0); });
        return;
    }

    std::lock_guard<std::mutex> lock(_impl->mutex);
    _impl->saves.push_back([this, guid, accountId, playerName, previousLevel, playerClass, race, totalTimePlayed, operationId = std::move(operationId), baseline, transaction, cancelOnly, callback]() mutable
    {
        uint32 count = 0;
        unsigned aborted = 0;
        for (;;)
        {
            if (_impl->Connect())
            {
                auto outcome = SaveOnce(*_impl->connection, _impl->databaseName, guid, accountId, playerName, previousLevel, playerClass, race,
                    totalTimePlayed, operationId, *baseline, *transaction, cancelOnly, count);
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
            std::unique_lock<std::mutex> guard(_impl->mutex);
            if (_impl->wake.wait_for(guard, std::chrono::seconds(2), [this]() { return _impl->stopping; }))
                return;
        }
    });
    _impl->wake.notify_one();
}

void PrestigioDatabase::SaveRetention(uint32 guid, std::vector<SavedSkillData> const& skills, std::vector<uint32> const& spells)
{
    _impl->Enqueue([this, guid, skills, spells]()
    {
        if (!_impl->Connect())
            return;

        std::string id = std::to_string(guid);
        _impl->connection->Execute("DELETE FROM " + Table(_impl->databaseName, "mod_prestigio_hollow_retention_skills") + " WHERE player_guid=" + id);
        _impl->connection->Execute("DELETE FROM " + Table(_impl->databaseName, "mod_prestigio_hollow_retention_spells") + " WHERE player_guid=" + id);

        for (auto const& sk : skills)
        {
            _impl->connection->Execute("INSERT INTO " + Table(_impl->databaseName, "mod_prestigio_hollow_retention_skills") +
                " (player_guid, skill_id, step, value, maximum) VALUES (" +
                id + "," + std::to_string(sk.id) + "," + std::to_string(sk.step) + "," +
                std::to_string(sk.value) + "," + std::to_string(sk.maximum) + ")");
        }

        for (uint32 spellId : spells)
        {
            _impl->connection->Execute("INSERT INTO " + Table(_impl->databaseName, "mod_prestigio_hollow_retention_spells") +
                " (player_guid, spell_id, spell_type) VALUES (" + id + "," + std::to_string(spellId) + ", 0)");
        }
    });
}

void PrestigioDatabase::LoadRetention(uint32 guid, RetentionCallback callback)
{
    _impl->Enqueue([this, guid, callback]()
    {
        std::vector<SavedSkillData> skills;
        std::vector<uint32> spells;
        bool success = false;

        if (_impl->Connect())
        {
            std::string id = std::to_string(guid);
            auto skRes = Query(*_impl->connection, "SELECT skill_id, step, value, maximum FROM " +
                Table(_impl->databaseName, "mod_prestigio_hollow_retention_skills") + " WHERE player_guid=" + id);
            if (skRes)
            {
                do
                {
                    Field* f = skRes->Fetch();
                    skills.push_back({f[0].Get<uint32>(), f[1].Get<uint16>(), f[2].Get<uint16>(), f[3].Get<uint16>()});
                } while (skRes->NextRow());
            }

            auto spRes = Query(*_impl->connection, "SELECT spell_id FROM " +
                Table(_impl->databaseName, "mod_prestigio_hollow_retention_spells") + " WHERE player_guid=" + id);
            if (spRes)
            {
                do
                {
                    Field* f = spRes->Fetch();
                    spells.push_back(f[0].Get<uint32>());
                } while (spRes->NextRow());
            }
            success = true;
        }

        _impl->Complete([callback, success, skills, spells]() mutable
        {
            callback(success, std::move(skills), std::move(spells));
        });
    });
}

void PrestigioDatabase::ClearRetention(uint32 guid)
{
    _impl->Enqueue([this, guid]()
    {
        if (!_impl->Connect())
            return;
        std::string id = std::to_string(guid);
        _impl->connection->Execute("DELETE FROM " + Table(_impl->databaseName, "mod_prestigio_hollow_retention_skills") + " WHERE player_guid=" + id);
        _impl->connection->Execute("DELETE FROM " + Table(_impl->databaseName, "mod_prestigio_hollow_retention_spells") + " WHERE player_guid=" + id);
    });
}

void PrestigioDatabase::PurgeSysAlasActive(uint32 guid)
{
    _impl->Enqueue([this, guid]()
    {
        if (!_impl->Connect())
            return;
        _impl->connection->Execute("DELETE FROM " + Table(_impl->sysAlasDb, "sysalas_active") + " WHERE guid=" + std::to_string(guid));
    });
}
