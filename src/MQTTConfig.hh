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
        virtual const std::optional<uint32_t> &mqtt_connect_retries() const = 0;
        virtual const std::string &mqtt_prefix() const = 0;
        virtual const std::string &mqtt_client_id() const = 0;
        virtual const std::optional<std::string> &mqtt_psk() const = 0;
        virtual const std::optional<std::string> &mqtt_identity() const = 0;
        virtual const std::optional<std::string> &mqtt_cafile() const = 0;
        virtual const std::optional<std::string> &mqtt_capath() const = 0;
        virtual const std::optional<std::string> &mqtt_certfile() const = 0;
        virtual const std::optional<std::string> &mqtt_keyfile() const = 0;
        virtual const bool mqtt_tls_insecure() const = 0;
        virtual const bool verbose() const = 0;
        virtual ~MQTTConfig() {};
};

#endif
