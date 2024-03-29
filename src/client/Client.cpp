#include <iostream>
#include <cstring>
#include <functional>
#include "Client.hh"
#include "mqtt_messages/MsgDeviceState.hh"
#include "mqtt_messages/MsgPrint.hh"
#include "mqtt_messages/MsgPrintResponse.hh"
#include "mqtt_messages/MsgPrintProgress.hh"
#include "mqtt_messages/MsgSensorReadings.hh"
#include "mqtt_messages/MsgAliases.hh"
#include "mqtt_messages/MsgAliasesSet.hh"
#include "mqtt_messages/MsgAliasesSetProvider.hh"

/*
 * Client()
 */
Client::Client(const ConfigGcode &conf)
    : m_conf(conf),
      m_mqtt(conf)
{
    int ret = sqlite3_open(":memory:", &m_db);
    if (ret) {
        sqlite3_close(m_db);
        m_db = nullptr;
        std::string err = "Could not open sqlite3 in memory database: ";
        err += sqlite3_errmsg(m_db);

        throw std::runtime_error(err);
    }

    // make like expressions case sensitive
    ret = sqlite3_exec(m_db, "PRAGMA case_sensitive_like = true", NULL, NULL, NULL);
    if (ret) {
        sqlite3_close(m_db);
        m_db = nullptr;
        std::string err = "Could not make like expressions case sensitive: ";
        err += sqlite3_errmsg(m_db);
        throw std::runtime_error(err);
    }

    { // create devices table
        std::string s_stmt = "CREATE TABLE devices "
                             "( "
                             "provider TEXT, "
                             "device TEXT, "
                             "state INTEGER DEFAULT 0, "
                             "print_percentage INTEGER DEFAULT 0, "
                             "print_remaining_time INTEGER DEFAULT 0, "
                             "device_alias TEXT DEFAULT NULL, "
                             "PRIMARY KEY (provider, device)"
                             ")";
        ret = sqlite3_exec(m_db, s_stmt.c_str(), NULL, NULL, NULL);

        if (SQLITE_OK != ret) {
            std::string err = "Client::Client(): Failed create table devices: ";
            err += sqlite3_errmsg(m_db);
            sqlite3_close(m_db);
            m_db = nullptr;
            throw std::runtime_error(err);
        }
    }

    { // create provider alias table
        std::string s_stmt = "CREATE TABLE provider_alias "
                             "( "
                             "provider TEXT NOT NULL UNIQUE PRIMARY KEY, "
                             "alias TEXT)";
        ret = sqlite3_exec(m_db, s_stmt.c_str(), NULL, NULL, NULL);

        if (SQLITE_OK != ret) {
            std::string err = "Client::Client(): Failed create table provider_alias: ";
            err += sqlite3_errmsg(m_db);
            sqlite3_close(m_db);
            m_db = nullptr;
            throw std::runtime_error(err);
        }
    }

    { // create sensor readings table
        std::string s_stmt = "CREATE TABLE sensor_readings "
                             "( "
                             "provider TEXT, "
                             "device TEXT, "
                             "sensor_name TEXT, "
                             "current_value REAL NOT NULL, "
                             "set_point REAL DEFAULT NULL, "
                             "unit TEXT DEFAULT NULL, "
                             "PRIMARY KEY (provider, device, sensor_name)"
                             ")";
        ret = sqlite3_exec(m_db, s_stmt.c_str(), NULL, NULL, NULL);

        if (SQLITE_OK != ret) {
            std::string err = "Client::Client(): Failed create table sensor_readings: ";
            err += sqlite3_errmsg(m_db);
            sqlite3_close(m_db);
            m_db = nullptr;
            throw std::runtime_error(err);
        }
    }

    m_mqtt.register_listener(this);

    std::string state_topic = conf.mqtt_prefix() + "/clients/+/+/state";
    std::string print_topic = conf.mqtt_prefix() + "/clients/+/+/print_response";
    std::string print_progress_topic = conf.mqtt_prefix() + "/clients/+/+/print_progress";
    std::string sensor_readings_topic = conf.mqtt_prefix() + "/clients/+/+/sensor_readings";
    std::string aliases_topic = conf.mqtt_prefix() + "/aliases/+";
    m_mqtt.subscribe(state_topic);
    m_mqtt.subscribe(print_topic);
    m_mqtt.subscribe(print_progress_topic);
    m_mqtt.subscribe(sensor_readings_topic);
    m_mqtt.subscribe(aliases_topic);

    m_mqtt.start();

    m_running = true;
    m_timeout_task = std::thread([this]() {
        while(m_running) {
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
            {
                const std::lock_guard<std::mutex> guard(m_mutex);
                const auto now = std::chrono::steady_clock::now();
                for (auto iter = m_print_callbacks.begin(); iter != m_print_callbacks.end();) {
                    if (now > iter->second.timeout) {
                        iter->second.callback(*iter->second.device, Device::PrintResult::NET_ERR_TIMEOUT),
                        iter = m_print_callbacks.erase(iter);
                    } else {
                        iter++;
                    }
                }
            }
        }
    });
}

/*
 * ~Client()
 */
Client::~Client()
{
    m_running = false;
    m_timeout_task.join();
    m_mqtt.unregister_listener(this);
    m_mqtt.stop();

    if (m_db) {
        sqlite3_close(m_db);
        m_db = nullptr;
    }
}


/*
 * on_message()
 */
void Client::on_message(const char *topic, const char *payload, size_t payload_len)
{
    const std::string prefix = m_conf.mqtt_prefix() + "/clients/";
    const std::string alias_prefix = m_conf.mqtt_prefix() + "/aliases/";
    const std::string state_postfix = "/state";
    const std::string print_postfix = "/print_response";
    const std::string print_progress_postfix = "/print_progress";
    const std::string sensor_readings_postfix = "/sensor_readings";

    if (0 == prefix.compare(0, prefix.size(), topic, prefix.size())) {
        if (   0 <= std::strlen(topic) - state_postfix.size()
            && 0 == state_postfix.compare(0, state_postfix.size(), topic + std::strlen(topic) - state_postfix.size())) {

            const char *first = topic + prefix.size();
            const char *last = topic + std::strlen(topic) - state_postfix.size();
            const char *pos = std::find(first, last, '/');
            if (pos >= last) {
                std::cerr << "Unexpected topic format: " << topic << "\n";
                return;
            }
            const std::string provider(first, pos);
            const std::string device(pos+1, last);
            const std::vector<char> msg_buf(payload, payload + payload_len);

            MsgDeviceState msg;
            msg.decode(msg_buf);

            sqlite3_stmt *stmt;
            std::string s_stmt = "INSERT INTO devices (provider, device, state) VALUES (?1, ?2, ?3) "
                                 "ON CONFLICT (provider, device) DO UPDATE SET state = ?3";
            int ret = sqlite3_prepare_v2(m_db,
                                         s_stmt.data(),
                                         s_stmt.size(),
                                         &stmt,
                                         NULL);
            if (SQLITE_OK != ret) {
                sqlite3_finalize(stmt);
                std::string err = "Client::on_message(): Failed prepare INSERT statement: ";
                err += sqlite3_errmsg(m_db);
                throw std::runtime_error(err);
            }

            ret = sqlite3_bind_text(stmt, 1, provider.data(), provider.size(), NULL);
            if (SQLITE_OK != ret) {
                sqlite3_finalize(stmt);
                std::string err = "Client::on_message(): Failed to bind provider parameter: ";
                err += sqlite3_errmsg(m_db);
                throw std::runtime_error(err);
            }

            ret = sqlite3_bind_text(stmt, 2, device.data(), device.size(), NULL);
            if (SQLITE_OK != ret) {
                sqlite3_finalize(stmt);
                std::string err = "Client::on_message(): Failed to bind device parameter: ";
                err += sqlite3_errmsg(m_db);
                throw std::runtime_error(err);
            }

            ret = sqlite3_bind_int(stmt, 3, static_cast<int>(msg.device_state()));
            if (SQLITE_OK != ret) {
                sqlite3_finalize(stmt);
                std::string err = "Client::on_message(): Failed to bind state parameter: ";
                err += sqlite3_errmsg(m_db);
                throw std::runtime_error(err);
            }
            ret = sqlite3_step(stmt);
            if (SQLITE_CONSTRAINT == ret) {
                std::string err = "Client::on_message(): Constraint error while execuion of INSERT statment: ";
                err += sqlite3_errmsg(m_db);
                sqlite3_finalize(stmt);
                throw std::runtime_error(err);
            }

            if (SQLITE_DONE != ret) {
                std::string err = "Client::on_message(): Execution of the INSERT statment failed: ";
                err += sqlite3_errmsg(m_db);
                sqlite3_finalize(stmt);
                throw std::runtime_error(err);
            }

            sqlite3_finalize(stmt);

        } else if (   0 <= std::strlen(topic) - print_postfix.size()
                   && 0 == print_postfix.compare(0, print_postfix.size(), topic + std::strlen(topic) - print_postfix.size())) {
            const char *first = topic + prefix.size();
            const char *last = topic + std::strlen(topic) - print_postfix.size();
            const char *pos = std::find(first, last, '/');
            if (pos >= last) {
                std::cerr << "Unexpected topic format: " << topic << "\n";
                return;
            }
            const std::string provider(first, pos);
            const std::string device(pos+1, last);

            const std::vector<char> msg_buf(payload, payload + payload_len);
            MsgPrintResponse msg_response;
            msg_response.decode(msg_buf);

            std::pair<uint64_t, uint64_t> key{ msg_response.request_code_part1(), msg_response.request_code_part2() };
            const std::lock_guard<std::mutex> guard(m_mutex);
            auto iter = m_print_callbacks.find(key);
            if (m_print_callbacks.end() == iter) {
                return;
            }
            iter->second.callback(*iter->second.device, msg_response.print_result());
            m_print_callbacks.erase(iter);
            return;

        } else if (   0 <= std::strlen(topic) - print_progress_postfix.size()
                   && 0 == print_progress_postfix.compare(0, print_progress_postfix.size(), topic + std::strlen(topic) - print_progress_postfix.size())) {
            const char *first = topic + prefix.size();
            const char *last = topic + std::strlen(topic) - print_progress_postfix.size();
            const char *pos = std::find(first, last, '/');
            if (pos >= last) {
                std::cerr << "Unexpected topic format: " << topic << "\n";
                return;
            }
            const std::string provider(first, pos);
            const std::string device(pos+1, last);

            const std::vector<char> msg_buf(payload, payload + payload_len);
            MsgPrintProgress msg_progress;
            msg_progress.decode(msg_buf);

            sqlite3_stmt *stmt;
            std::string s_stmt = "INSERT INTO devices (provider, device, print_percentage, print_remaining_time) VALUES (?1, ?2, ?3, ?4) "
                                 "ON CONFLICT (provider, device) DO UPDATE SET print_percentage = ?3, print_remaining_time = ?4";
            int ret = sqlite3_prepare_v2(m_db,
                                         s_stmt.data(),
                                         s_stmt.size(),
                                         &stmt,
                                         NULL);
            if (SQLITE_OK != ret) {
                sqlite3_finalize(stmt);
                std::string err = "Client::on_message(): Failed prepare INSERT statement: ";
                err += sqlite3_errmsg(m_db);
                throw std::runtime_error(err);
            }

            ret = sqlite3_bind_text(stmt, 1, provider.data(), provider.size(), NULL);
            if (SQLITE_OK != ret) {
                sqlite3_finalize(stmt);
                std::string err = "Client::on_message(): Failed to bind provider parameter: ";
                err += sqlite3_errmsg(m_db);
                throw std::runtime_error(err);
            }

            ret = sqlite3_bind_text(stmt, 2, device.data(), device.size(), NULL);
            if (SQLITE_OK != ret) {
                sqlite3_finalize(stmt);
                std::string err = "Client::on_message(): Failed to bind device parameter: ";
                err += sqlite3_errmsg(m_db);
                throw std::runtime_error(err);
            }

            ret = sqlite3_bind_int(stmt, 3, msg_progress.percentage());
            if (SQLITE_OK != ret) {
                sqlite3_finalize(stmt);
                std::string err = "Client::on_message(): Failed to bind print_percentage parameter: ";
                err += sqlite3_errmsg(m_db);
                throw std::runtime_error(err);
            }

            ret = sqlite3_bind_int(stmt, 4, msg_progress.remaining_time());
            if (SQLITE_OK != ret) {
                sqlite3_finalize(stmt);
                std::string err = "Client::on_message(): Failed to bind print_remaining_time parameter: ";
                err += sqlite3_errmsg(m_db);
                throw std::runtime_error(err);
            }
            ret = sqlite3_step(stmt);
            if (SQLITE_CONSTRAINT == ret) {
                std::string err = "Client::on_message(): Constraint error while execuion of INSERT statment: ";
                err += sqlite3_errmsg(m_db);
                sqlite3_finalize(stmt);
                throw std::runtime_error(err);
            }

            if (SQLITE_DONE != ret) {
                std::string err = "Client::on_message(): Execution of the INSERT statment failed: ";
                err += sqlite3_errmsg(m_db);
                sqlite3_finalize(stmt);
                throw std::runtime_error(err);
            }

            sqlite3_finalize(stmt);


        } else if (   0 <= std::strlen(topic) - sensor_readings_postfix.size()
                   && 0 == sensor_readings_postfix.compare(0, sensor_readings_postfix.size(), topic + std::strlen(topic) - sensor_readings_postfix.size())) {
            const char *first = topic + prefix.size();
            const char *last = topic + std::strlen(topic) - sensor_readings_postfix.size();
            const char *pos = std::find(first, last, '/');
            if (pos >= last) {
                std::cerr << "Unexpected topic format: " << topic << "\n";
                return;
            }
            const std::string provider(first, pos);
            const std::string device(pos+1, last);

            const std::vector<char> msg_buf(payload, payload + payload_len);
            MsgSensorReadings msg_sensor_readings;
            msg_sensor_readings.decode(msg_buf);

            sqlite3_stmt *stmt;
            std::string s_stmt = "INSERT INTO sensor_readings (provider, "
                                                              "device, "
                                                              "sensor_name, "
                                                              "current_value, "
                                                              "set_point, "
                                                              "unit) "
                                 "VALUES (?1, ?2, ?3, ?4, ?5, ?6) "
                                 "ON CONFLICT (provider, device, sensor_name) DO UPDATE SET current_value = ?4, set_point = ?5, unit = ?6";
            int ret = sqlite3_prepare_v2(m_db,
                                         s_stmt.data(),
                                         s_stmt.size(),
                                         &stmt,
                                         NULL);
            if (SQLITE_OK != ret) {
                sqlite3_finalize(stmt);
                std::string err = "Client::on_message(): Failed prepare INSERT statement: ";
                err += sqlite3_errmsg(m_db);
                throw std::runtime_error(err);
            }

            ret = sqlite3_bind_text(stmt, 1, provider.data(), provider.size(), NULL);
            if (SQLITE_OK != ret) {
                std::string err = "Client::on_message(): Failed to bind provider parameter: ";
                err += sqlite3_errmsg(m_db);
                sqlite3_finalize(stmt);
                throw std::runtime_error(err);
            }

            ret = sqlite3_bind_text(stmt, 2, device.data(), device.size(), NULL);
            if (SQLITE_OK != ret) {
                std::string err = "Client::on_message(): Failed to bind device parameter: ";
                err += sqlite3_errmsg(m_db);
                sqlite3_finalize(stmt);
                throw std::runtime_error(err);
            }

            for (const auto &sr: msg_sensor_readings.sensor_readings()) {
                ret = sqlite3_bind_text(stmt, 3, sr.first.data(), sr.first.size(), NULL);
                if (SQLITE_OK != ret) {
                    std::string err = "Client::on_message(): Failed to bind sensor_name parameter: ";
                    err += sqlite3_errmsg(m_db);
                    sqlite3_finalize(stmt);
                    throw std::runtime_error(err);
                }

                ret = sqlite3_bind_double(stmt, 4, sr.second.current_value);
                if (SQLITE_OK != ret) {
                    std::string err = "Client::on_message(): Failed to bind current_value parameter: ";
                    err += sqlite3_errmsg(m_db);
                    sqlite3_finalize(stmt);
                    throw std::runtime_error(err);
                }

                if (sr.second.set_point) {
                    ret = sqlite3_bind_double(stmt, 5, *sr.second.set_point);
                    if (SQLITE_OK != ret) {
                        std::string err = "Client::on_message(): Failed to bind set_point parameter: ";
                        err += sqlite3_errmsg(m_db);
                        sqlite3_finalize(stmt);
                        throw std::runtime_error(err);
                    }
                } else {
                    ret = sqlite3_bind_null(stmt, 5);
                    if (SQLITE_OK != ret) {
                        std::string err = "Client::on_message(): Failed to bind set_point parameter: ";
                        err += sqlite3_errmsg(m_db);
                        sqlite3_finalize(stmt);
                        throw std::runtime_error(err);
                    }
                }

                if (sr.second.unit) {
                    ret = sqlite3_bind_text(stmt, 6, sr.second.unit->data(), sr.second.unit->size(), NULL);
                    if (SQLITE_OK != ret) {
                        std::string err = "Client::on_message(): Failed to bind unit parameter: ";
                        err += sqlite3_errmsg(m_db);
                        sqlite3_finalize(stmt);
                        throw std::runtime_error(err);
                    }
                } else {
                    ret = sqlite3_bind_null(stmt, 6);
                    if (SQLITE_OK != ret) {
                        std::string err = "Client::on_message(): Failed to bind unit parameter: ";
                        err += sqlite3_errmsg(m_db);
                        sqlite3_finalize(stmt);
                        throw std::runtime_error(err);
                    }
                }

                ret = sqlite3_step(stmt);
                if (SQLITE_CONSTRAINT == ret) {
                    std::string err = "Client::on_message(): Constraint error while execuion of INSERT statment: ";
                    err += sqlite3_errmsg(m_db);
                    sqlite3_finalize(stmt);
                    throw std::runtime_error(err);
                }

                if (SQLITE_DONE != ret) {
                    std::string err = "Client::on_message(): Execution of the INSERT statment failed: ";
                    err += sqlite3_errmsg(m_db);
                    sqlite3_finalize(stmt);
                    throw std::runtime_error(err);
                }

                ret = sqlite3_reset(stmt);
                if (SQLITE_OK != ret) {
                    std::string err = "Client::on_message(): failed to reset statement: ";
                    err += sqlite3_errmsg(m_db);
                    sqlite3_finalize(stmt);
                    throw std::runtime_error(err);
                }
            }
            sqlite3_finalize(stmt);

        } else {
            std::cerr << "Unexpected mqtt topic postfix: " << topic << "\n";
            return;
        }
    } else if (0 == alias_prefix.compare(0, alias_prefix.size(), topic, alias_prefix.size())) {
        const char *first = topic + alias_prefix.size();
        const char *last = topic + std::strlen(topic);
        const char *pos = std::find(first, last, '/');
        if (pos != last) {
            std::cerr << "Unexpected topic format: " << topic << "\n";
            return;
        }
        const std::string provider(first, last);

        const std::vector<char> msg_buf(payload, payload + payload_len);
        MsgAliases msg_aliases;
        msg_aliases.decode(msg_buf);

        if (msg_aliases.provider_alias().size()) {
            sqlite3_stmt *stmt;
            std::string s_stmt = "INSERT INTO provider_alias (provider, alias) VALUES (?1, ?2) "
                                 "ON CONFLICT (provider) DO UPDATE SET alias = ?2";
            int ret = sqlite3_prepare_v2(m_db,
                                         s_stmt.data(),
                                         s_stmt.size(),
                                         &stmt,
                                         NULL);
            if (SQLITE_OK != ret) {
                sqlite3_finalize(stmt);
                std::string err = "Client::on_message(): Failed prepare INSERT statement: ";
                err += sqlite3_errmsg(m_db);
                throw std::runtime_error(err);
            }

            ret = sqlite3_bind_text(stmt, 1, provider.data(), provider.size(), NULL);
            if (SQLITE_OK != ret) {
                sqlite3_finalize(stmt);
                std::string err = "Client::on_message(): Failed to bind provider parameter: ";
                err += sqlite3_errmsg(m_db);
                throw std::runtime_error(err);
            }

            ret = sqlite3_bind_text(stmt, 2, msg_aliases.provider_alias().data(), msg_aliases.provider_alias().size(), NULL);
            if (SQLITE_OK != ret) {
                sqlite3_finalize(stmt);
                std::string err = "Client::on_message(): Failed to bind alias parameter: ";
                err += sqlite3_errmsg(m_db);
                throw std::runtime_error(err);
            }
            ret = sqlite3_step(stmt);
            if (SQLITE_CONSTRAINT == ret) {
                std::string err = "Client::on_message(): Constraint error while execuion of INSERT statment: ";
                err += sqlite3_errmsg(m_db);
                sqlite3_finalize(stmt);
                throw std::runtime_error(err);
            }

            if (SQLITE_DONE != ret) {
                std::string err = "Client::on_message(): Execution of the INSERT statment failed: ";
                err += sqlite3_errmsg(m_db);
                sqlite3_finalize(stmt);
                throw std::runtime_error(err);
            }

            sqlite3_finalize(stmt);
        } else {
            sqlite3_stmt *stmt;
            std::string s_stmt = "DELETE FROM  provider_alias WHERE provider = ?1";
            int ret = sqlite3_prepare_v2(m_db,
                                         s_stmt.data(),
                                         s_stmt.size(),
                                         &stmt,
                                         NULL);
            if (SQLITE_OK != ret) {
                sqlite3_finalize(stmt);
                std::string err = "Client::on_message(): Failed prepare DELETE statement: ";
                err += sqlite3_errmsg(m_db);
                throw std::runtime_error(err);
            }

            ret = sqlite3_bind_text(stmt, 1, provider.data(), provider.size(), NULL);
            if (SQLITE_OK != ret) {
                sqlite3_finalize(stmt);
                std::string err = "Client::on_message(): Failed to bind provider parameter: ";
                err += sqlite3_errmsg(m_db);
                throw std::runtime_error(err);
            }

            ret = sqlite3_step(stmt);
            if (SQLITE_DONE != ret) {
                std::string err = "Client::on_message(): Execution of the DELETE statment failed: ";
                err += sqlite3_errmsg(m_db);
                sqlite3_finalize(stmt);
                throw std::runtime_error(err);
            }

            sqlite3_finalize(stmt);
        }

        for (const auto &alias: msg_aliases.aliases()) {
            sqlite3_stmt *stmt;
            std::string s_stmt = "INSERT INTO devices (provider, device, device_alias) VALUES (?1, ?2, ?3) "
                                 "ON CONFLICT (provider, device) DO UPDATE SET device_alias = ?3";
            int ret = sqlite3_prepare_v2(m_db,
                                         s_stmt.data(),
                                         s_stmt.size(),
                                         &stmt,
                                         NULL);
            if (SQLITE_OK != ret) {
                sqlite3_finalize(stmt);
                std::string err = "Client::on_message(): Failed prepare INSERT statement: ";
                err += sqlite3_errmsg(m_db);
                throw std::runtime_error(err);
            }

            ret = sqlite3_bind_text(stmt, 1, provider.data(), provider.size(), NULL);
            if (SQLITE_OK != ret) {
                sqlite3_finalize(stmt);
                std::string err = "Client::on_message(): Failed to bind provider parameter: ";
                err += sqlite3_errmsg(m_db);
                throw std::runtime_error(err);
            }

            ret = sqlite3_bind_text(stmt, 2, alias.first.data(), alias.first.size(), NULL);
            if (SQLITE_OK != ret) {
                sqlite3_finalize(stmt);
                std::string err = "Client::on_message(): Failed to bind device parameter: ";
                err += sqlite3_errmsg(m_db);
                throw std::runtime_error(err);
            }

            ret = sqlite3_bind_text(stmt, 3, alias.second.data(), alias.second.size(), NULL);
            if (SQLITE_OK != ret) {
                sqlite3_finalize(stmt);
                std::string err = "Client::on_message(): Failed to bind alias parameter: ";
                err += sqlite3_errmsg(m_db);
                throw std::runtime_error(err);
            }
            ret = sqlite3_step(stmt);
            if (SQLITE_CONSTRAINT == ret) {
                std::string err = "Client::on_message(): Constraint error while execuion of INSERT statment: ";
                err += sqlite3_errmsg(m_db);
                sqlite3_finalize(stmt);
                throw std::runtime_error(err);
            }

            if (SQLITE_DONE != ret) {
                std::string err = "Client::on_message(): Execution of the INSERT statment failed: ";
                err += sqlite3_errmsg(m_db);
                sqlite3_finalize(stmt);
                throw std::runtime_error(err);
            }

            sqlite3_finalize(stmt);
        }

    } else {
        std::cerr << "Unexpected mqtt topic prefix\n";
        return;
    }
}


/*
 * devices()
 */
std::unique_ptr<std::vector<Client::DeviceInfo>> Client::devices(const std::string &hint)
{
    return devices(hint, m_conf.resolve_aliases());
}


/*
 * devices()
 */
std::unique_ptr<std::vector<Client::DeviceInfo>> Client::devices(const std::string &hint, bool resolve_aliases)
{
    int ret;
    std::unique_ptr<std::vector<Client::DeviceInfo>> devices = std::make_unique<std::vector<Client::DeviceInfo>>();
    sqlite3_stmt *stmt;

    std::pair<std::string, std::string> sql_hints = convert_hint(hint);

    std::string s_stmt =  "SELECT d.provider, d.device, d.state, d.print_percentage, d.print_remaining_time, d.device_alias, a.alias "
                          "FROM devices AS d "
                          "LEFT JOIN provider_alias AS a ON d.provider = a.provider "
                          "WHERE d.state!=0 ";
    if (resolve_aliases) {
        s_stmt += "AND (   d.device_alias LIKE '" + sql_hints.second + "' ESCAPE '\\' "
                  "       OR (d.device_alias IS NULL AND d.device LIKE '" + sql_hints.second + "' ESCAPE '\\')) "
                  "AND (   a.alias LIKE '" + sql_hints.first + "' ESCAPE '\\' "
                  "     OR (a.alias IS NULL AND d.provider LIKE '" + sql_hints.first + "' ESCAPE '\\' )) ";
    } else {
        s_stmt += "AND d.device LIKE '" + sql_hints.second + "' ESCAPE '\\' "
                  "AND d.provider LIKE '" + sql_hints.first + "' ESCAPE '\\' ";
    }
    s_stmt += "ORDER BY d.device_alias, d.device, a.alias, d.provider;";

    ret = sqlite3_prepare_v2(m_db,
                             s_stmt.data(),
                             s_stmt.size(),
                             &stmt,
                             NULL);
    if (SQLITE_OK != ret) {
        std::string err = "Client::devices(): Faild to prepare SELECT statement: ";
        err += sqlite3_errmsg(m_db);
        throw std::runtime_error(err);
    }

    while (SQLITE_ROW == (ret = sqlite3_step(stmt))) {
        devices->emplace_back();
        devices->back().provider = (const char *)sqlite3_column_text(stmt, 0);
        devices->back().name = (const char *)sqlite3_column_text(stmt, 1);
        devices->back().state = (Device::State)sqlite3_column_int(stmt, 2);
        devices->back().print_percentage = sqlite3_column_int(stmt, 3);
        devices->back().print_remaining_time = sqlite3_column_int(stmt, 4);
        if (sqlite3_column_text(stmt, 5)) {
            devices->back().device_alias = (const char *)sqlite3_column_text(stmt, 5);
        }
        if (sqlite3_column_text(stmt, 6)) {
            devices->back().provider_alias = (const char *)sqlite3_column_text(stmt, 6);
        }
    }

    sqlite3_finalize(stmt);

    return devices;
}


/*
 * send()
 */
void Client::print(const Client::DeviceInfo &dev, const std::string &gcode, std::function<void(const DeviceInfo &dev, Device::PrintResult)> callback)
{
    using namespace std::chrono_literals;

    // TODO: Allow to skip this test
    if (dev.state != Device::State::OK) {
        callback(dev, Device::PrintResult::ERR_INVALID_STATE);
        return;
    }

    MsgPrint print(gcode);
    std::pair<uint64_t, uint64_t> key{ print.request_code_part1(), print.request_code_part2() };
    // TODO: Make the timeout configurable!
    struct print_callback_helper value(std::chrono::steady_clock::now() + 1s, callback, dev);
    {
        const std::lock_guard<std::mutex> guard(m_mutex);
        m_print_callbacks.emplace(std::pair(key, value));
    }
    std::vector<char> payload;
    print.encode(payload);
    std::string topic = m_conf.mqtt_prefix() + "/clients/" + dev.provider + "/" + dev.name + "/print_request";
    m_mqtt.publish(topic, payload);
}


/*
 * get_provider_aliases()
 */
std::unique_ptr<std::map<std::string, std::string>> Client::get_provider_aliases()
{
    int ret;
    std::unique_ptr<std::map<std::string, std::string>> aliases = std::make_unique<std::map<std::string, std::string>>();
    sqlite3_stmt *stmt;

    std::string s_stmt = "SELECT provider, alias FROM provider_alias";
    ret = sqlite3_prepare_v2(m_db,
                             s_stmt.data(),
                             s_stmt.size(),
                             &stmt,
                             NULL);
    if (SQLITE_OK != ret) {
        std::string err = "Client::devices(): Faild to prepare SELECT statement: ";
        err += sqlite3_errmsg(m_db);
        throw std::runtime_error(err);
    }

    while (SQLITE_ROW == (ret = sqlite3_step(stmt))) {
        std::string provider = (const char *)sqlite3_column_text(stmt, 0);
        std::string alias = (const char *)sqlite3_column_text(stmt, 1);
        (*aliases)[provider] = alias;
    }

    sqlite3_finalize(stmt);

    return aliases;
}


/*
 * get_device_aliases()
 */
std::unique_ptr<std::map<std::string, std::string>> Client::get_device_aliases()
{
    int ret;
    std::unique_ptr<std::map<std::string, std::string>> aliases = std::make_unique<std::map<std::string, std::string>>();
    sqlite3_stmt *stmt;

    std::string s_stmt = "SELECT device, device_alias FROM devices WHERE device_alias IS NOT NULL";
    ret = sqlite3_prepare_v2(m_db,
                             s_stmt.data(),
                             s_stmt.size(),
                             &stmt,
                             NULL);
    if (SQLITE_OK != ret) {
        std::string err = "Client::devices(): Faild to prepare SELECT statement: ";
        err += sqlite3_errmsg(m_db);
        throw std::runtime_error(err);
    }

    while (SQLITE_ROW == (ret = sqlite3_step(stmt))) {
        std::string device = (const char *)sqlite3_column_text(stmt, 0);
        std::string alias = (const char *)sqlite3_column_text(stmt, 1);
        (*aliases)[device] = alias;
    }

    sqlite3_finalize(stmt);

    return aliases;
}


/*
 * get_providers()
 */
std::unique_ptr<std::vector<std::string>> Client::get_providers(const std::string &hint)
{
    int ret;
    if (std::string::npos != hint.find("/")) {
        throw std::runtime_error("HINT contains invalid characters ('/')");
    }
    std::string hint_copy = hint + "/*";
    std::pair<std::string, std::string> sql_hints = convert_hint(hint_copy);

    std::unique_ptr<std::vector<std::string>> providers = std::make_unique<std::vector<std::string>>();
    sqlite3_stmt *stmt;

    std::string s_stmt = "SELECT DISTINCT provider "
                         "FROM devices "
                         "WHERE provider IS NOT NULL "
                         "AND provider LIKE '" + sql_hints.first + "' ESCAPE '\\' ";
    ret = sqlite3_prepare_v2(m_db,
                             s_stmt.data(),
                             s_stmt.size(),
                             &stmt,
                             NULL);
    if (SQLITE_OK != ret) {
        std::string err = "Client::get_providers(): Faild to prepare SELECT statement: ";
        err += sqlite3_errmsg(m_db);
        throw std::runtime_error(err);
    }

    while (SQLITE_ROW == (ret = sqlite3_step(stmt))) {
        std::string provider = (const char *)sqlite3_column_text(stmt, 0);
        providers->push_back(provider);
    }

    sqlite3_finalize(stmt);

    return providers;
}


/*
 * set_provider_alias()
 */
bool Client::set_provider_alias(const std::string &provider_hint, const std::string &alias)
{
    auto provider_list = get_providers(provider_hint);
    if (!provider_list || 1 != provider_list->size()) {
        return false;
    }

    MsgAliasesSetProvider msg;
    msg.set_provider_alias(alias);
    std::vector<char> msg_buf;
    msg.encode(msg_buf);
    std::string topic = m_conf.mqtt_prefix() + "/aliases/" + provider_list->front() + "/set";
    m_mqtt.publish(topic, msg_buf);
    return true;
}


/*
 * set_device_alias()
 */
bool Client::set_device_alias(const std::string &device_hint, const std::string &alias)
{
    auto devices = this->devices(device_hint, false);
    if (!devices || 1 != devices->size()) {
        return false;
    }
    MsgAliasesSet msg;
    msg.set_device_name(devices->front().name);
    msg.set_device_alias(alias);
    std::vector<char> msg_buf;
    msg.encode(msg_buf);
    std::string topic = m_conf.mqtt_prefix() + "/aliases/" + devices->front().provider + "/set";
    m_mqtt.publish(topic, msg_buf);
    return true;
}


/*
 * get_sensor_readings()
 */
std::unique_ptr<std::map<std::string, std::vector<Client::SensorReading>>> Client::sensor_readings(const std::string &device_hint)
{
    std::unique_ptr<std::map<std::string, std::vector<SensorReading>>> sr = std::make_unique<std::map<std::string, std::vector<SensorReading>>>();
    sqlite3_stmt *stmt;
    int ret;

    std::pair<std::string, std::string> sql_hints = convert_hint(device_hint);

    std::string s_stmt =  "SELECT sr.provider, a.alias, sr.device, d.device_alias, sr.sensor_name, sr.current_value, sr.set_point, sr.unit "
                          "FROM sensor_readings AS sr "
                          "LEFT JOIN devices AS d ON sr.provider = d.provider AND sr.device = d.device "
                          "LEFT JOIN provider_alias AS a ON sr.provider = a.provider "
                          "WHERE d.state!=0 ";
    if (m_conf.resolve_aliases()) {
        s_stmt += "AND (   d.device_alias LIKE '" + sql_hints.second + "' ESCAPE '\\' "
                  "       OR (d.device_alias IS NULL AND d.device LIKE '" + sql_hints.second + "' ESCAPE '\\')) "
                  "AND (   a.alias LIKE '" + sql_hints.first + "' ESCAPE '\\' "
                  "     OR (a.alias IS NULL AND d.provider LIKE '" + sql_hints.first + "' ESCAPE '\\' )) ";
    } else {
        s_stmt += "AND d.device LIKE '" + sql_hints.second + "' ESCAPE '\\' "
                  "AND d.provider LIKE '" + sql_hints.first + "' ESCAPE '\\' ";
    }
    s_stmt += "ORDER BY d.device_alias, d.device, a.alias, d.provider, sr.sensor_name;";

    ret = sqlite3_prepare_v2(m_db,
                             s_stmt.data(),
                             s_stmt.size(),
                             &stmt,
                             NULL);
    if (SQLITE_OK != ret) {
        std::string err = "Client::sensor_readings(): Faild to prepare SELECT statement: ";
        err += sqlite3_errmsg(m_db);
        throw std::runtime_error(err);
    }

    while (SQLITE_ROW == (ret = sqlite3_step(stmt))) {
        std::string device;
        if (m_conf.resolve_aliases() && sqlite3_column_text(stmt, 1)) {
            device =  (const char *)sqlite3_column_text(stmt, 1);
        } else {
            device  = (const char *)sqlite3_column_text(stmt, 0);
        }
        device += "/";
        if (m_conf.resolve_aliases() && sqlite3_column_text(stmt, 3)) {
            device += (const char *)sqlite3_column_text(stmt, 3);
        } else {
            device += (const char *)sqlite3_column_text(stmt, 2);
        }
        SensorReading value;
        value.sensor_name =(const char *)sqlite3_column_text(stmt, 4);
        value.current_value = sqlite3_column_double(stmt, 5);
        if (SQLITE_FLOAT == sqlite3_column_type(stmt, 6)) {
            value.set_point = sqlite3_column_double(stmt, 6);
        } else {
            value.set_point.reset();
        }
        if (SQLITE_TEXT == sqlite3_column_type(stmt, 7)) {
            value.unit = (const char *)sqlite3_column_text(stmt, 7);
        } else {
            value.unit.reset();
        }
        (*sr)[device].push_back(value);
    }

    return sr;
}


/*
 * convert_hint()
 */
std::pair<std::string, std::string> Client::convert_hint(const std::string &hint) const
{
    if (   hint.find("'") != std::string::npos
        || hint.find("\"") != std::string::npos) {
        throw std::runtime_error("HINT contains invalid characters (' or \")");
    }
    std::string::size_type pos = hint.find("/");
    std::string provider_hint;
    std::string device_hint;
    if (pos == std::string::npos) {
        provider_hint = "*";
        device_hint = hint;
    } else {
        provider_hint = hint.substr(0, pos);
        device_hint = hint.substr(pos+1);
    }

    auto escape = [](std::string &hint, const std::string &symbol) {
        std::string::size_type pos = hint.find(symbol);
        while (pos != std::string::npos) {
            hint.insert(pos, 1, '\\');
            pos = hint.find(symbol, pos+2);
        }
    };

    escape(provider_hint, "%");
    escape(provider_hint, "_");
    escape(device_hint, "%");
    escape(device_hint, "_");

    bool last_was_bslash = false;
    auto replace_asterisk = [&last_was_bslash](char c) {
            bool ret = false;
            if (c == '*' && !last_was_bslash) {
                ret = true;
            }
            last_was_bslash = false;
            if (c == '\\') {
                last_was_bslash = true;
            }
            return ret;
    };

    std::replace_if(provider_hint.begin(), provider_hint.end(), replace_asterisk, '%');

    last_was_bslash = false;
    std::replace_if(device_hint.begin(), device_hint.end(), replace_asterisk, '%');

    return std::pair<std::string, std::string>(provider_hint, device_hint);
}
