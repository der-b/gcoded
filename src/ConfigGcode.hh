#ifndef __CONFIGGCODE_HH__
#define __CONFIGGCODE_HH__

#include <vector>
#include <optional>
#include <filesystem>
#include "MQTTConfig.hh"

class ConfigGcode : public MQTTConfig {
    public:
        ConfigGcode() = delete;
        ConfigGcode(int argc, char **argv);
        virtual ~ConfigGcode() {};

        /**
         * returns the configuration file, which was loaded, if any file was loaded.
         */
        const std::optional<std::filesystem::path> &conf_file() const {
            return m_conf_file;
        }


        /**
         * returns the hostname or IP of the MQTT broker
         */
        virtual const std::string &mqtt_broker() const override {
            return m_mqtt_broker;
        }


        /**
         * returns the port of the MQTT broker
         */
        virtual const uint16_t mqtt_port() const override {
            return m_mqtt_port;
        }


        /**
         * returns the used username for the MQTT connection
         */
        virtual const std::optional<std::string> &mqtt_user() const override {
            return m_mqtt_user;
        }


        /**
         * returns the used password for the MQTT connection
         */
        virtual const std::optional<std::string> &mqtt_password() const override {
            return m_mqtt_password;
        }


        virtual const std::string &mqtt_client_id() const override {
            return m_mqtt_client_id;
        }


        /**
         * Returns the prefix under which the interface for the 3d printers will be
         * exposed.
         */
        virtual const std::string &mqtt_prefix() const override {
            return m_mqtt_prefix;
        }


        /**
         * Connection retries before raising an exception.
         * Infinite retries if not set.
         */
        virtual const std::optional<uint32_t> &mqtt_connect_retries() const override {
            return m_mqtt_connect_retries;
        }


        /**
         * Returns the PSK, used while connecting to a MQTT broker.
         * The identity to the PSK can be retrieved with mqtt_identity()
         */
        virtual const std::optional<std::string> &mqtt_psk() const override
        {
            return m_mqtt_psk;
        }


        /**
         * Returns the identity corresponding to the PSK.
         */
        virtual const std::optional<std::string> &mqtt_identity() const override
        {
            return m_mqtt_identity;
        }


        /**
         * if true, than the help message shall be printed.
         */
        const bool print_help() const {
            return m_print_help;
        }


        /**
         * if true, prints debug output
         */
        virtual const bool verbose() const override {
            return m_verbose;
        }


        /**
         * returns the usage string.
         */
        const char *usage() const;

        /**
         * returns the help message.
         */
        const char *help() const;

        /**
         * TODO: Improve documentation
         * command()
         */
        const std::optional<std::string> &command() const {
            return m_command;
        }

        /**
         * TODO: Improve documentation
         * command_args()
         */
        const std::vector<std::string> &command_args() const {
            return m_command_args;
        }

        /**
         * Returns false, if aliases shall not be used and resolved.
         * That means, the real device names and provider names are used, if it returns false.
         */
        bool resolve_aliases() const {
            return m_resolve_aliases;
        }

    private:
        /**
         * sets the default configuration, which is compiled into the program.
         */
        void set_default();


        /**
         * parses only the configuration file argument.
         */
        void parse_config(int argc, char **argv);

        /**
         * loads a the configuration file in the path m_conf_file.
         * throws an exceptin, if m_conf_file was set but does not exist.
         */
        void load_config();

        /**
         * parses all program arguments except the configuration file argument.
         */
        void parse_args(int argc, char **argv);

        std::optional<uint16_t> parse_mqtt_port_value(const std::string &value) const;
        std::optional<uint32_t> parse_mqtt_connect_retries_value(const std::string &value) const;
        std::optional<std::pair<std::string, std::string>> parse_mqtt_psk(const std::string &value) const;


    private:
        std::optional<std::filesystem::path> m_conf_file;
        std::string m_mqtt_broker;
        uint16_t m_mqtt_port;
        std::optional<std::string> m_mqtt_user;
        std::optional<std::string> m_mqtt_password;
        std::string m_mqtt_prefix;
        std::optional<uint32_t> m_mqtt_connect_retries;
        std::string m_mqtt_client_id;
        std::optional<std::string> m_mqtt_psk;
        std::optional<std::string> m_mqtt_identity;
        bool m_load_dummy;
        bool m_print_help;
        bool m_verbose;
        std::optional<std::string> m_command;
        std::vector<std::string> m_command_args;
        bool m_resolve_aliases;
};

std::ostream& operator<<(std::ostream& out, const ConfigGcode &conf);

#endif
