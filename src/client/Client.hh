#ifndef __CLIENT_HH__
#define __CLIENT_HH__

#include <memory>
#include <sqlite3.h>
#include <map>
#include <chrono>
#include <thread>
#include "../devices/Device.hh"
#include "../ConfigGcode.hh"
#include "../MQTT.hh"

class Client : public MQTT::Listener {
    public:
        struct DeviceInfo {
            std::string provider;
            std::string name;
            Device::State state;
        };

        Client() = delete;
        Client(const ConfigGcode &conf);
        ~Client();

        virtual void on_message(const char *topic, const char *payload, size_t payload_len) override;
        std::unique_ptr<std::vector<DeviceInfo>> devices(const std::string &hint = "*");
        void print(const DeviceInfo &dev, const std::string &gcode, std::function<void(const DeviceInfo &, Device::PrintResult)> callback);

    private:
        /**
         * Converts a hint to an expression to expressions, which can be used in SQL statements.
         * The first string of the result pair is the like expression for the provider and the
         * second string is the like expression for the device.
         */
        std::pair<std::string, std::string> convert_hint(const std::string &hint) const;

    private:
        const ConfigGcode &m_conf;
        MQTT m_mqtt;
        sqlite3 *m_db;

        std::thread m_timeout_task;
        bool m_running;

        std::mutex m_mutex;
        struct print_callback_helper {
            std::chrono::time_point<std::chrono::steady_clock> timeout;
            std::function<void(const DeviceInfo, Device::PrintResult)>  callback;
            const DeviceInfo *device;

            print_callback_helper(std::chrono::time_point<std::chrono::steady_clock> _timeout,
                                  std::function<void(const DeviceInfo, Device::PrintResult)> _callback,
                                  const DeviceInfo &_device)
                : timeout(_timeout),
                  callback(_callback),
                  device(&_device)
            {}
        };
        std::map<std::pair<uint64_t, uint64_t>, struct print_callback_helper> m_print_callbacks;
};

#endif
