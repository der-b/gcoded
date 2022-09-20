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
         * command()
         */
        const std::optional<std::string> &command() const {
            return m_command;
        }

        /**
         * command_args()
         */
        const std::vector<std::string> &command_args() const {
            return m_command_args;
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


    private:
        std::optional<std::filesystem::path> m_conf_file;
        std::string m_mqtt_broker;
        uint16_t m_mqtt_port;
        std::string m_mqtt_prefix;
        std::string m_mqtt_client_id;
        bool m_load_dummy;
        bool m_print_help;
        bool m_verbose;
        std::optional<std::string> m_command;
        std::vector<std::string> m_command_args;
};

std::ostream& operator<<(std::ostream& out, const ConfigGcode &conf);

#endif
