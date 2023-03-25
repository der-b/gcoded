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
            std::string provider_alias;
            std::string device_alias;
            Device::State state;
            uint8_t print_percentage;
            uint32_t print_remaining_time;
        };

        struct SensorReading {
            std::string sensor_name;
            double current_value;
            std::optional<std::string> unit;
            std::optional<double> set_point;
        };

        Client() = delete;
        Client(const ConfigGcode &conf);
        ~Client();

        virtual void on_message(const char *topic, const char *payload, size_t payload_len) override;
        // TODO: Documentation
        std::unique_ptr<std::vector<DeviceInfo>> devices(const std::string &hint = "*");
        std::unique_ptr<std::vector<DeviceInfo>> devices(const std::string &hint, bool resolve_aliases);
        // TODO: Documentation
        void print(const DeviceInfo &dev, const std::string &gcode, std::function<void(const DeviceInfo &, Device::PrintResult)> callback);

        /**
         * Returns a map of all provider aliases. Thereby, the map key is the provider original name
         * and the map value is the alias name.
         */
        std::unique_ptr<std::map<std::string, std::string>> get_provider_aliases();

        /**
         * Returns a map of all device aliases. Thereby, the map key is the device original name
         * and the map value is the alias name.
         */
        std::unique_ptr<std::map<std::string, std::string>> get_device_aliases();

        /**
         * returns all provider.
         */
        std::unique_ptr<std::vector<std::string>> get_providers(const std::string &hint = "*");

        /**
         * Sets the alias of a provider. If alias has a size of zero, an existing alias is deleted.
         *
         * Returns false, if the given provider does not exist.
         */
        bool set_provider_alias(const std::string &provider_hint, const std::string &alias);

        /**
         * Sets the alias of a device. If alias has a size of zero, an existing alias is deleted.
         *
         * Returns false, if the given device does not exist.
         */
        bool set_device_alias(const std::string &device_hint, const std::string &alias);

        /**
         * Returns the sensor readings for the devices.
         */
        std::unique_ptr<std::map<std::string, std::vector<SensorReading>>> sensor_readings(const std::string &device_hint);

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
