#ifndef __MQTTCONFIG_HH__
#define __MQTTCONFIG_HH__

#include <string>
#include <optional>

class MQTTConfig {
    public:
        virtual const std::string &mqtt_broker() const = 0;
        virtual const uint16_t mqtt_port() const = 0;
        virtual const std::optional<std::string> &mqtt_user() const = 0;
        virtual const std::optional<std::string> &mqtt_password() const = 0;
        virtual const std::string &mqtt_prefix() const = 0;
        virtual const std::string &mqtt_client_id() const = 0;
        virtual const bool verbose() const = 0;
        virtual ~MQTTConfig() {};
};

#endif
