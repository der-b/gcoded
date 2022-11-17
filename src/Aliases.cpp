#include "Aliases.hh"
#include <iostream>
#include <thread>

/*
 * to_str()
 */
const std::string &to_str(enum Aliases::State state)
{
    static const std::string states[] = { "UNKNOWN",
                                          "INIT",
                                          "OK",
                                          "ERR_FILE",
                                          "READONLY",
                                          "<UNKNOWN_STATE>" };
    if (state > Aliases::State::__LAST_ENTRY) {
        state = Aliases::State::__LAST_ENTRY;
    }
    return states[static_cast<size_t>(state)];
}


/*
 * constructor()
 */
Aliases::Aliases(const Config &config)
    : m_db(nullptr),
      m_config(config),
      m_state(State::INIT)
{
    int ret = sqlite3_open(m_config.aliases_file().c_str(), &m_db);
    if (SQLITE_OK != ret) {
        m_db = nullptr;
        m_state = State::ERR_FILE;
        return;
    }

    ret = sqlite3_busy_timeout(m_db, 10);
    if (SQLITE_OK != ret) {
        std::cerr << "WARN: Failed to set busy timeout\n";
    }

    Inotify::get().register_listener(m_config.aliases_file().string(), Inotify::MODIFY, this);

    m_state = State::OK;
    // check, whether we can write to the database or not
    // TODO: Make user version a parameter
    ret = sqlite3_exec(m_db, "PRAGMA user_version = 0", NULL, NULL, NULL);
    if (SQLITE_OK != ret) {
        m_state = State::READONLY;
        if (ret != SQLITE_READONLY) {
            std::string err = "Could not set user version: ";
            err += sqlite3_errmsg(m_db);
            throw std::runtime_error(err);
        }
    }

    if (m_state == State::OK) {
        std::string create_provider_table =
            "CREATE TABLE IF NOT EXISTS provider_alias "
            "(alias TEXT)";
        ret = sqlite3_exec(m_db, create_provider_table.c_str(), NULL, NULL, NULL);
        if (SQLITE_OK != ret) {
            std::string err = "Aliases::";
            err += __func__;
            err += "(): Could not create provider_alias table: ";
            err += sqlite3_errmsg(m_db);
            sqlite3_close(m_db);
            m_db = nullptr;
            throw std::runtime_error(err);
        }

        std::string create_alias_table =
            "CREATE TABLE IF NOT EXISTS alias "
            "(device TEXT UNIQUE PRIMARY KEY NOT NULL,"
            " alias TEXT UNIQUE NOT NULL)";
        ret = sqlite3_exec(m_db, create_alias_table.c_str(), NULL, NULL, NULL);
        if (SQLITE_OK != ret) {
            std::string err = "Aliases::";
            err += __func__;
            err += "(): Could not create alias table: ";
            err += sqlite3_errmsg(m_db);
            sqlite3_close(m_db);
            m_db = nullptr;
            throw std::runtime_error(err);
        }
    }

    if (1 < provider_alias_count()) {
        throw std::runtime_error("Alias database is incorrect: It has more than one alias for the provider.");
    }

}


/*
 * destructor()
 */
Aliases::~Aliases()
{
    if (m_db) {
        sqlite3_close(m_db);
        m_db = nullptr;
    }
    Inotify::get().unregister_listener(std::nullopt, this);
    m_state = State::UNKNOWN;
}


/*
 * provider_alias()
 */
std::optional<std::string> Aliases::provider_alias()
{
    if (!m_db) {
        return std::nullopt;
    }

    sqlite3_stmt *stmt;
    std::string s_stmt = "SELECT alias FROM provider_alias";

    int ret = sqlite3_prepare_v2(m_db,
                                 s_stmt.data(),
                                 s_stmt.size(),
                                 &stmt,
                                 NULL);
    if (SQLITE_OK != ret) {
        sqlite3_finalize(stmt);
        std::string err = "Aliases::";
        err += __func__;
        err += "(): Failed to prepare select statement: ";
        err += sqlite3_errmsg(m_db);
        throw std::runtime_error(err);
    }

    std::string alias;

    ret = sqlite3_step(stmt);
    if (SQLITE_ROW == ret) {
        alias = (char *)sqlite3_column_text(stmt, 0);
        ret = sqlite3_step(stmt);
    }

    if (SQLITE_ROW == ret) {
        sqlite3_finalize(stmt);
        throw std::runtime_error("Alias database is incorrect: It has more than one alias for the provider.");
    }

    if (SQLITE_DONE != ret) {
        sqlite3_finalize(stmt);
        std::string err = "Aliases::";
        err += __func__;
        err += "(): Failed to execute select statement: ";
        err += sqlite3_errmsg(m_db);
        throw std::runtime_error(err);
    }

    sqlite3_finalize(stmt);

    return alias;
}


/*
 * set_provider_alias()
 */
bool Aliases::set_provider_alias(const std::string &alias)
{
    if (m_state != State::OK) {
        return false;
    }

    Transaction trans(m_db);

    const int count = provider_alias_count();
    sqlite3_stmt *stmt;
    std::string s_stmt;
    if (0 == count) {
        s_stmt = "INSERT INTO provider_alias (alias) VALUES (?)";
    } else {
        s_stmt = "UPDATE provider_alias SET alias = ?";
    }

    int ret = sqlite3_prepare_v2(m_db,
                                 s_stmt.data(),
                                 s_stmt.size(),
                                 &stmt,
                                 NULL);
    if (SQLITE_OK != ret) {
        std::string err = "Aliases::";
        err += __func__;
        err += "(): Failed prepare INSERT/UPDATE statement: ";
        err += sqlite3_errmsg(m_db);
        sqlite3_finalize(stmt);
        throw std::runtime_error(err);
    }

    ret = sqlite3_bind_text(stmt, 1, alias.data(), alias.size(), NULL);
    if (SQLITE_OK != ret) {
        sqlite3_finalize(stmt);
        std::string err = "Aliases::";
        err += __func__;
        err += "(): Failed to bind alias parameter: ";
        err += sqlite3_errmsg(m_db);
        throw std::runtime_error(err);
    }

    ret = sqlite3_step(stmt);
    if (SQLITE_CONSTRAINT == ret) {
        std::string err = "Aliases::";
        err += __func__;
        err += "(): Constraint error while execuion of INSERT/UPDATE statment: ";
        err += sqlite3_errmsg(m_db);
        sqlite3_finalize(stmt);
        throw std::runtime_error(err);
    }

    if (SQLITE_DONE != ret) {
        std::string err = "Aliases::";
        err += __func__;
        err += "(): Execution of the INSERT/UPDATE statment failed: ";
        err += sqlite3_errmsg(m_db);
        sqlite3_finalize(stmt);
        throw std::runtime_error(err);
    }

    sqlite3_finalize(stmt);

    trans.commit();
    return true;
}


/*
 * provider_alias_count()
 */
uint64_t Aliases::provider_alias_count()
{
    if (!m_db) {
        return 0;
    }

    sqlite3_stmt *stmt;
    std::string s_stmt = "SELECT count(*) FROM provider_alias";

    int ret = sqlite3_prepare_v2(m_db,
                                 s_stmt.data(),
                                 s_stmt.size(),
                                 &stmt,
                                 NULL);
    if (SQLITE_OK != ret) {
        sqlite3_finalize(stmt);
        std::string err = "Aliases::";
        err += __func__;
        err += "(): Failed to prepare select statement: ";
        err += sqlite3_errmsg(m_db);
        throw std::runtime_error(err);
    }

    uint64_t count = 0;

    ret = sqlite3_step(stmt);
    if (SQLITE_ROW == ret) {
        count = sqlite3_column_int64(stmt, 0);
        ret = sqlite3_step(stmt);
    }
    if (SQLITE_DONE != ret) {
        sqlite3_finalize(stmt);
        std::string err = "Aliases::";
        err += __func__;
        err += "(): Execution of the select statment failed: ";
        err += sqlite3_errmsg(m_db);
        throw std::runtime_error(err);
    }

    sqlite3_finalize(stmt);

    return count;
}


/*
 * set_alias()
 */
bool Aliases::set_alias(const std::string &device, const std::string &alias)
{
    if (m_state != State::OK) {
        return false;
    }

    if (0 == device.size())
    {
        return false;
    }

    if (0 == alias.size()) {
        return rm_alias(device);
    }

    sqlite3_stmt *stmt;
    std::string s_stmt = "INSERT INTO alias (device, alias) VALUES (?1, ?2) "
                         "ON CONFLICT (device) DO UPDATE SET alias = ?2";
    int ret = sqlite3_prepare_v2(m_db,
                                 s_stmt.data(),
                                 s_stmt.size(),
                                 &stmt,
                                 NULL);
    if (SQLITE_OK != ret) {
        sqlite3_finalize(stmt);
        std::string err = "Aliases::";
        err += __func__;
        err += "(): Failed prepare INSERT/UPDATE statement: ";
        err += sqlite3_errmsg(m_db);
        throw std::runtime_error(err);
    }

    ret = sqlite3_bind_text(stmt, 1, device.data(), device.size(), NULL);
    if (SQLITE_OK != ret) {
        sqlite3_finalize(stmt);
        std::string err = "Aliases::";
        err += __func__;
        err += "(): Failed to bind device parameter: ";
        err += sqlite3_errmsg(m_db);
        throw std::runtime_error(err);
    }

    ret = sqlite3_bind_text(stmt, 2, alias.data(), alias.size(), NULL);
    if (SQLITE_OK != ret) {
        sqlite3_finalize(stmt);
        std::string err = "Aliases::";
        err += __func__;
        err += "(): Failed to bind alias parameter: ";
        err += sqlite3_errmsg(m_db);
        throw std::runtime_error(err);
    }

    ret = sqlite3_step(stmt);
    if (SQLITE_CONSTRAINT == ret) {
        std::string err = "Aliases::";
        err += __func__;
        err += "(): Constraint error while execuion of INSERT/UPDATE statment: ";
        err += sqlite3_errmsg(m_db);
        sqlite3_finalize(stmt);
        throw std::runtime_error(err);
    }

    if (SQLITE_DONE != ret) {
        std::string err = "Aliases::";
        err += __func__;
        err += "(): Execution of the INSERT/UPDATE statment failed: ";
        err += sqlite3_errmsg(m_db);
        sqlite3_finalize(stmt);
        throw std::runtime_error(err);
    }

    sqlite3_finalize(stmt);

    // TODO: use sqlite3_changes64() ... unfortunately this function is not avialiable on rasbian
    //       i don't know how to check for this at compile time.
    if (0 == sqlite3_changes(m_db)) {
        return false;
    }

    return true;
}


/*
 * rm_alias()
 */
bool Aliases::rm_alias(const std::string &device)
{
    if (m_state != State::OK) {
        return false;
    }

    if (0 == device.size())
    {
        return false;
    }

    sqlite3_stmt *stmt;
    std::string s_stmt = "DELETE FROM alias WHERE device=?";
    int ret = sqlite3_prepare_v2(m_db,
                                 s_stmt.data(),
                                 s_stmt.size(),
                                 &stmt,
                                 NULL);
    if (SQLITE_OK != ret) {
        sqlite3_finalize(stmt);
        std::string err = "Aliases::";
        err += __func__;
        err += "(): Failed prepare DELETE statement: ";
        err += sqlite3_errmsg(m_db);
        throw std::runtime_error(err);
    }

    ret = sqlite3_bind_text(stmt, 1, device.data(), device.size(), NULL);
    if (SQLITE_OK != ret) {
        sqlite3_finalize(stmt);
        std::string err = "Aliases::";
        err += __func__;
        err += "(): Failed to bind device parameter: ";
        err += sqlite3_errmsg(m_db);
        throw std::runtime_error(err);
    }

    ret = sqlite3_step(stmt);
    if (SQLITE_DONE != ret) {
        std::string err = "Aliases::";
        err += __func__;
        err += "(): Execution of the DELETE statment failed: ";
        err += sqlite3_errmsg(m_db);
        sqlite3_finalize(stmt);
        throw std::runtime_error(err);
    }

    sqlite3_finalize(stmt);

    return true;
}


/*
 * get_aliases()
 */
void Aliases::get_aliases(std::map<std::string, std::string> &aliases)
{
    if (!m_db) {
        return;
    }

    sqlite3_stmt *stmt;
    std::string s_stmt = "SELECT device, alias FROM alias";
    int ret = sqlite3_prepare_v2(m_db,
                                 s_stmt.data(),
                                 s_stmt.size(),
                                 &stmt,
                                 NULL);
    if (SQLITE_OK != ret) {
        sqlite3_finalize(stmt);
        std::string err = "Aliases::";
        err += __func__;
        err += "(): Failed prepare DELETE statement: ";
        err += sqlite3_errmsg(m_db);
        throw std::runtime_error(err);
    }

    while (SQLITE_ROW == (ret = sqlite3_step(stmt))) {
        aliases[(char *)sqlite3_column_text(stmt, 0)] = (char *)sqlite3_column_text(stmt, 1);
    }

    if (SQLITE_DONE != ret) {
        std::string err = "Aliases::";
        err += __func__;
        err += "(): Execution of the DELETE statment failed: ";
        err += sqlite3_errmsg(m_db);
        sqlite3_finalize(stmt);
        throw std::runtime_error(err);
    }

    sqlite3_finalize(stmt);
}


/*
 * on_fs_event()
 */
void Aliases::on_fs_event(const std::string &path, uint32_t event_type, const std::optional<std::string> &name)
{
    const std::lock_guard<std::mutex> guard(m_listener_mutex);
    for (const auto &list: m_listeners) {
        list->on_alias_change();
    }
}


/**************************************
 * Transaction
 **************************************/

/*
 * Transaction()
 */
Aliases::Transaction::Transaction(sqlite3 *db)
    : m_db(db),
      m_committed(false)
{
    sqlite3_stmt *stmt;
    int ret;
    std::string s_stmt = "BEGIN IMMEDIATE;";
    ret = sqlite3_prepare_v2(m_db,
                             s_stmt.data(),
                             s_stmt.size(),
                             &stmt,
                             NULL);

    if (SQLITE_OK != ret) {
        std::string err = "Aliases::Transaction";
        err += __func__;
        err += "(): Failed prepare BEGIN IMMEDIATE TRANSACTION statement: ";
        err += sqlite3_errmsg(m_db);
        throw std::runtime_error(err);
    }

    ret = sqlite3_step(stmt);
    if (SQLITE_DONE != ret) {
        std::string err = "Aliases::Transaction";
        err += __func__;
        err += "(): failed to begin transaction:";
        err += sqlite3_errmsg(m_db);
        sqlite3_finalize(stmt);
        throw std::runtime_error(err);
    }

    sqlite3_finalize(stmt);
}


/*
 * commit()
 */
void Aliases::Transaction::commit()
{
    if (m_committed) {
        std::string err = "Aliases::Transaction";
        err += __func__;
        err += "(): You can not transmit a transaction more than once.";
        throw std::runtime_error(err);
    }
    m_committed = true;

    sqlite3_stmt *stmt;
    int ret;
    std::string s_stmt = "COMMIT TRANSACTION;";
    ret = sqlite3_prepare_v2(m_db,
                             s_stmt.data(),
                             s_stmt.size(),
                             &stmt,
                             NULL);

    if (SQLITE_OK != ret) {
        std::string err = "Aliases::Transaction";
        err += __func__;
        err += "(): Failed prepare COMMIT TRANSACTION statement: ";
        err += sqlite3_errmsg(m_db);
        throw std::runtime_error(err);
    }

    int counter = 1;
    while (SQLITE_BUSY == (ret = sqlite3_step(stmt)) && counter < 3) {
        counter++;
        std::this_thread::yield();
    }

    if (SQLITE_DONE != ret) {
        std::string err = "Aliases::Transaction";
        err += __func__;
        err += "(): failed to commit transaction:";
        err += sqlite3_errmsg(m_db);
        sqlite3_finalize(stmt);
        throw std::runtime_error(err);
    }

    sqlite3_finalize(stmt);
}

/*
 * ~Transaction()
 */
Aliases::Transaction::~Transaction() noexcept (false)
{
    if (m_committed) {
        return;
    }

    sqlite3_stmt *stmt;
    int ret;
    std::string s_stmt = "ROLLBACK TRANSACTION;";
    ret = sqlite3_prepare_v2(m_db,
                             s_stmt.data(),
                             s_stmt.size(),
                             &stmt,
                             NULL);

    if (SQLITE_OK != ret) {
        std::string err = "Aliases::Transaction";
        err += __func__;
        err += "(): Failed prepare ROLLBACK TRANSACTION statement: ";
        err += sqlite3_errmsg(m_db);
        throw std::runtime_error(err);
    }

    ret = sqlite3_step(stmt);
    if (SQLITE_DONE != ret) {
        std::string err = "Aliases::Transaction";
        err += __func__;
        err += "(): failed to rollback transaction: ";
        err += sqlite3_errmsg(m_db);
        sqlite3_finalize(stmt);
        throw std::runtime_error(err);
    }

    sqlite3_finalize(stmt);
}

