#ifndef __CONFIG_HH__
#define __CONFIG_HH__

#include <optional>
#include <filesystem>
#include "MQTTConfig.hh"

class Config : public MQTTConfig {
    public:
        Config() = delete;
        Config(int argc, char **argv);
        virtual ~Config() {};

        /**
         * returns the configuration file, which was loaded, if any file was loaded.
         */
        const std::optional<std::filesystem::path> &conf_file() const {
            return m_conf_file;
        }


        /**
         * returns the id file, which was loaded, if any file was loaded.
         */
        const std::optional<std::filesystem::path> &id_file() const {
            return m_id_file;
        }


        /**
         * returns the filename of the alias database
         */
        const std::filesystem::path &aliases_file() const {
            return m_aliases_file;
        }


        /**
         * returns the client ID used for MQTT.
         * This id is a 128bit random number.
         * This ID is persistent even over reboots. This ID is normally stored in "/var/lib/gcoded/id".
         * If this file does't exist, than this file is created.
         * If this file does not exist, than a temporary id is created, which has prefix "temp-".
         */
        virtual const std::string &mqtt_client_id() const override {
            return m_mqtt_client_id;
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


        /**
         * Returns the prefix under which the interface for the 3d printers will be
         * exposed.
         */
        virtual const std::string &mqtt_prefix() const override {
            return m_mqtt_prefix;
        }


        /**
         * Connection retries before raising an exception.
         * Infenite retries if not set.
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
         * Returns the certificate of the certificate authority for validating the
         * certificate of the MQTT broker.
         */
        virtual const std::optional<std::string> &mqtt_cafile() const override
        {
            return m_mqtt_cafile;
        }


        /**
         * Returns the path to a directory wich contains certificates of the certificate authorities
         * for validating the certificate of the MQTT broker.
         *
         * Be aware, that the every time you copy a new certificate in this directory, you have to
         * execute "openssl rehash <direcory>". In order that this command works correctly, the
         * certificats needs one of the following filename endings: ".pem", ".crt", ".cer" or ".crl"
         */
        virtual const std::optional<std::string> &mqtt_capath() const override
        {
            return m_mqtt_capath;
        }


        /**
         * Returns the path to the certificate file for the client authentification.
         */
        virtual const std::optional<std::string> &mqtt_certfile() const override
        {
            return m_mqtt_certfile;
        }


        /**
         * Returns the path to the file containing the private key which belong the certificate
         * in the certfile.
         */
        virtual const std::optional<std::string> &mqtt_keyfile() const override
        {
            return m_mqtt_keyfile;
        }


        /**
         * Returns true if the domain name in the broker certificate will not be validated.
         * Setting this to true is intended for testing purposes!
         */
        virtual const bool mqtt_tls_insecure() const override
        {
            return m_mqtt_tls_insecure;
        }


        /**
         * Normally the thread which controls the 3d printer is running on a realtime scheduler.
         * That means, this thread thread has always the priority over normal thread. This has the
         * advantage, that other processes/threads which consume a lot CPU time does never disturb
         * the printing process. The disadvantage is, that if bug in the realtime thread (i.e.
         * infinity loop) might halt the whole operating system, since no other thread will get
         * any computing time. This option allows to disable the usage of the realtime scheduler.
         * This is intended to ease the debugging.
         *
         * Setting it to false, disables the usage of the realtime scheduler.
         *
         * Using the realtime scheduler needs superuser rights. If you don't want to start the
         * Gcoded daemon with this rights, than you have to disable the realtime scheduler.
         */
        const bool use_realtime_scheduler() const
        {
            return m_use_realtime_scheduler;
        }


        /**
         * returns true if dummy devices shall be loaded
         */
        const bool load_dummy() const {
            return m_load_dummy;
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

    private:
        /**
         * sets the default configuration, which is compiled into the program.
         */
        void set_default();

        /**
         * Tries to load a client id from '/var/lib/gcoded/id'.
         * If this file contains an invalid id, than throws an runtime exception.
         * If file does not exist, than this method will attempt to create an this file
         * with proper content.
         * If this file can't be created, than a temporary id will be generated. Temporary IDs
         * always start with 'temp-'.
         */
        void load_mqtt_client_id();

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

        /**
         * Checks the config on logical errors. Throws an exception if an error was found.
         */
        void validate_config();

        std::optional<uint16_t> parse_mqtt_port_value(const std::string &value) const;
        std::optional<uint32_t> parse_mqtt_connect_retries_value(const std::string &value) const;
        std::optional<std::pair<std::string, std::string>> parse_mqtt_psk(const std::string &value) const;


    private:
        std::optional<std::filesystem::path> m_conf_file;
        std::optional<std::filesystem::path> m_id_file;
        std::filesystem::path m_aliases_file;
        std::string m_mqtt_client_id;
        std::string m_mqtt_broker;
        uint16_t m_mqtt_port;
        std::optional<std::string> m_mqtt_user;
        std::optional<std::string> m_mqtt_password;
        std::string m_mqtt_prefix;
        std::optional<uint32_t> m_mqtt_connect_retries;
        std::optional<std::string> m_mqtt_psk;
        std::optional<std::string> m_mqtt_identity;
        std::optional<std::string> m_mqtt_cafile;
        std::optional<std::string> m_mqtt_capath;
        std::optional<std::string> m_mqtt_certfile;
        std::optional<std::string> m_mqtt_keyfile;
        bool m_mqtt_tls_insecure;
        bool m_use_realtime_scheduler;
        bool m_load_dummy;
        bool m_print_help;
        bool m_verbose;
};

std::ostream& operator<<(std::ostream& out, const Config &conf);

#endif
