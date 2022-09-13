#include <iostream>
#include <cstring>
#include <functional>
#include "Client.hh"
#include "mqtt_messages/MsgDeviceState.hh"
#include "mqtt_messages/MsgPrint.hh"
#include "mqtt_messages/MsgPrintResponse.hh"

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
        sqlite3_stmt *stmt;
        std::string s_stmt = "CREATE TABLE devices "
                             "( "
                             "provider TEXT, "
                             "device TEXT, "
                             "state INTEGER, "
                             "PRIMARY KEY (provider, device)"
                             ")";

        int ret = sqlite3_prepare_v2(m_db,
                                     s_stmt.data(),
                                     s_stmt.size(),
                                     &stmt,
                                     NULL);
        if (SQLITE_OK != ret) {
            sqlite3_finalize(stmt);
            std::string err = "Client::Client(): Failed to prepare create table statement: ";
            err += sqlite3_errmsg(m_db);
            sqlite3_close(m_db);
            m_db = nullptr;
            throw std::runtime_error(err);
        }

        ret = sqlite3_step(stmt);
        if (SQLITE_DONE != ret) {
            sqlite3_finalize(stmt);
            std::string err = "Client::Client(): Execution of the create table statment failed: ";
            err += sqlite3_errmsg(m_db);
            sqlite3_close(m_db);
            m_db = nullptr;
            throw std::runtime_error(err);
        }

        sqlite3_finalize(stmt);
    }

    m_mqtt.register_listener(this);

    std::string state_topic = conf.mqtt_prefix() + "/clients/+/+/state";
    std::string print_topic = conf.mqtt_prefix() + "/clients/+/+/print_response";
    m_mqtt.subscribe(state_topic);
    m_mqtt.subscribe(print_topic);

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
    const std::string state_postfix = "/state";
    const std::string print_postfix = "/print_response";

    ssize_t res;
    if (0 != (res = prefix.compare(0, prefix.size(), topic, prefix.size()))) {
        std::cerr << "Unexpected mqtt topic prefix\n";
        return;
    }

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
                             "ON CONFLICT DO UPDATE SET state = ?3";
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

    } else {
        std::cerr << "Unexpected mqtt topic postfix: " << topic << "\n";
        return;
    }
}


/*
 * devices()
 */
std::unique_ptr<std::vector<Client::DeviceInfo>> Client::devices(const std::string &hint)
{
    int ret;
    std::unique_ptr<std::vector<Client::DeviceInfo>> devices = std::make_unique<std::vector<Client::DeviceInfo>>();
    sqlite3_stmt *stmt;

    std::pair<std::string, std::string> sql_hints = convert_hint(hint);

    std::string s_stmt =  "SELECT provider, device, state FROM devices WHERE device LIKE '"
                        + sql_hints.second
                        + "' ESCAPE '\\' AND provider LIKE '"
                        + sql_hints.first
                        + "' ESCAPE '\\' ORDER BY device, provider;";
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
    }

    return devices;
}


/*
 * send()
 */
void Client::print(const Client::DeviceInfo &dev, const std::string &gcode, std::function<void(const DeviceInfo &dev, Device::PrintResult)> callback)
{
    using namespace std::chrono_literals;
    // TODO: Strip out comment lines and trim each line to reduce the transferred size!
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
 * convert_hint()
 */
std::pair<std::string, std::string> Client::convert_hint(const std::string &hint) const
{
    if (   hint.find("'") != std::string::npos
        || hint.find("\"") != std::string::npos) {
        throw std::runtime_error("DEVICE_HINT contains invalid characters (' or \")");
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
