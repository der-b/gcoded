#include "ConfigGcode.hh"
#include <getopt.h>
#include <iostream>
#include <fstream>
#include <regex>
#include <sys/random.h>

const struct option long_options_config[] = {
    { "config",               required_argument, 0, 'c' },
    { "mqtt-broker",          required_argument, 0, 'b' },
    { "mqtt-port",            required_argument, 0, 'p' },
    { "mqtt-prefix",          required_argument, 0, 'e' },
    { "mqtt-connect-retries", required_argument, 0, 't' },
    { "mqtt-tls-insecure",    no_argument,       0, 0 },
    { "real-names",           no_argument,       0, 'r' },
    { "verbose",              no_argument,       0, 'v' },
    { "help",                 no_argument,       0, 'h' },
    { 0, 0, 0, 0 }
};

const char short_options_config[] = "-c:b:p:e:t:rvh";

const char usage_message[] = "gcode [OPTIONS] [COMMAND]\n";
const char help_message[] =
"This program controls is a command line interface to the gcoded daemons registered to an MQTT broker.\n"
"\n"
"OPTIONS:\n"
"-c, --config=file               Configuration file to load.\n"
"-b, --mqtt-broker=hostname      Hostname or IP of the MQTT broker.\n"
"-p, --mqtt-port=port            Port of the MQTT broker.\n"
"-e, --mqtt-prefix=prefix        MQTT topic under which gcoded will expose the interface.\n"
"-t, --mqtt-connect-retries=num  Number of connection retries to the MQTT broker befor giving up.\n"
"    --mqtt-tls-insecure         Do not validate domain name in the MQTT broker certificate.\n"
"                                This should be used only for testing. Allows Man-in-the-middle attacks.\n"
"-r, --real-names                Do not use aliases for device and provider. Use real names.\n"
"-v, --verbose                   Enable debug output.\n"
"-h, --help                      Print help message and configuration.\n"
"\n"
"COMMANDS: (get further details with \"-h\": e.g. \"gcode list -h\")\n"
"list         Lists all currently known devices which can process gcode.\n"
"send         Sends a gcode file to an device.\n"
"alias        Manage aliases.\n";


const char list_usage_message[] = "gcode [OPTIONS] list [DEVICE_HINT]\n";
const char list_help_message[] =
"List all currently known devices which can process gcode.\n"
"DEVICE_HINT  A hint to which device the gcode file shall be send.\n"
"             If no hint is given, than the file will be send to all known devices.\n"
"             The hint accepts '*' as a wildcard and tries to match device names.\n"
"             If you want to match all devices of one provider, than you have to provide a\n"
"             hint like 'providername/*'.\n"
"             If a hint matches for more than one device, you will be prompt whether you are sure.\n";

const char send_usage_message[] = "gcode [OPTIONS] send GCODE_FILE [DEVICE_HINT]\n";
const char send_help_message[] =
"Sends a gcode file to an device.\n"
"GCODE_FILE   Gcode file which shall be send to the device.\n"
"DEVICE_HINT  A hint to which device the gcode file shall be send.\n"
"             If no hint is given, than the file will be send to all known devices.\n"
"             The hint accepts '*' as a wildcard and tries to match device names.\n"
"             If you want to match all devices of one provider, than you have to provide a\n"
"             hint like 'providername/*'.\n"
"             If a hint matches for more than one device, you will be prompt whether you are sure.\n";

const char alias_usage_message[] = "gcode [OPTIONS] alias ACTION\n";
const char alias_help_message[] =
"Manage aliases.\n"
"ACTIONS: \n"
"list           List all aliases.\n"
"set            Set aliases for providers/devices. It expects three arguments: TYPE NAME ALIAS.\n"
"               TYPE is either 'provider' or 'device'\n"
"               NAME is the name of the provider or device\n"
"               ALIAS is the new alias of the provider or alias. This argument is optional. If it is\n"
"               omitted, than the alias is removed\n";


/*
 * constructor()
 */
ConfigGcode::ConfigGcode(int argc, char **argv)
    : m_load_dummy(false)
{
    set_default();
    parse_config(argc, argv);
    load_config();
    parse_args(argc, argv);
    validate_config();
}


/*
 * usage();
 */
const char *ConfigGcode::usage() const
{
    if (m_command) {
        if ("list" == *m_command) {
            return list_usage_message;
        } else if ("send" == *m_command) {
            return send_usage_message;
        } else if ("alias" == *m_command) {
            return alias_usage_message;
        } else {
            std::cerr << "Cant print command specific usage message: Unknown command.\n";
        }
    }
    return usage_message;
}


/*
 * help();
 */
const char *ConfigGcode::help() const
{
    if (m_command) {
        if ("list" == *m_command) {
            return list_help_message;
        } else if ("send" == *m_command) {
            return send_help_message;
        } else if ("alias" == *m_command) {
            return alias_help_message;
        } else {
            std::cerr << "Cant print command specific help message: Unknown command.\n";
        }
    }
    return help_message;
}


/*
 * set_default()
 */
void ConfigGcode::set_default()
{
    m_conf_file = "./gcoded.conf";
    if (!std::filesystem::exists(*m_conf_file)) {
        m_conf_file = "/etc/gcoded.conf";
        if (!std::filesystem::exists(*m_conf_file)) {
            m_conf_file.reset();
        }
    }

    m_mqtt_broker = "localhost";
    m_mqtt_port = 1883;
    m_mqtt_prefix = "gcoded";
    m_mqtt_connect_retries = 5;
    m_mqtt_client_id = "";
    m_mqtt_psk = std::nullopt;
    m_mqtt_identity = std::nullopt;
    m_mqtt_cafile = std::nullopt;
    m_mqtt_capath = std::nullopt;
    m_mqtt_certfile = std::nullopt;
    m_mqtt_keyfile = std::nullopt;
    m_mqtt_tls_insecure = false;
    m_print_help = false;
    m_verbose = false;
    m_resolve_aliases = true;
}


/*
 * parse_config()
 */
void ConfigGcode::parse_config(int argc, char **argv)
{
    optind = 1;
    while (1) {
        int c = getopt_long(argc, argv, short_options_config, long_options_config, NULL);

        if (-1 == c) {
            break;
        }

        switch(c) {
            case 'c':
                m_conf_file = optarg;
                break;
            default:
                // ignore everything except the config options
                break;
        }
    }
}


/*
 * load_config()
 */
void ConfigGcode::load_config()
{
    auto trim = [](std::string &s) {
        s.erase(0, s.find_first_not_of(" \n\r\t\f\v"));
        s.erase(s.find_last_not_of(" \n\r\t\f\v") + 1);
    };
    if (!m_conf_file) {
        return;
    }
    std::ifstream conf_file(*m_conf_file);
    if (!conf_file) {
        std::string err = "Could not open config file: ";
        err += *m_conf_file;
        throw std::runtime_error(err);
    }
    uint32_t line_counter = 0;

    std::regex var_name_regex("[[:alpha:]][[:alnum:]_]*");
    std::regex var_value_regex("([^[:space:]\"]+)|(\"(([^\"])|(\\\\\"))*\")");

    while (!conf_file.eof()) {
        line_counter++;

        std::string line;
        std::getline(conf_file, line);

        // remove witespaces at the beginning or the end of a line
        trim(line);

        if (0 == line.length()) {
            continue;
        }

        if ('#' == line[0]) {
            // ignore comments
            continue;
        }

        std::string::size_type pos_equals = line.find('=');
        if (std::string::npos == pos_equals) {
            std::string err = "Error on line ";
            err += std::to_string(line_counter);
            err += " in configuration file: ";
            err += *m_conf_file;
            err += ": A line needs to look like 'var_name = var_value'";
            throw std::runtime_error(err);
        }

        std::string var_name = line.substr(0, pos_equals);
        std::string var_value = line.substr(pos_equals + 1);
        trim(var_name);
        trim(var_value);

        if (0 == var_name.length()) {
            std::string err = "Error on line ";
            err += std::to_string(line_counter);
            err += " in configuration file: ";
            err += *m_conf_file;
            err += ": You have to provide a variable name.";
            throw std::runtime_error(err);
        }

        if (!std::regex_match(var_name, var_name_regex)) {
            std::string err = "Error on line ";
            err += std::to_string(line_counter);
            err += " in configuration file: ";
            err += *m_conf_file;
            err += ": invalid variable name: '";
            err += var_name;
            err += "'";
            throw std::runtime_error(err);
        }

        if (0 == var_value.size()) {
            std::string err = "Error on line ";
            err += std::to_string(line_counter);
            err += " in configuration file: ";
            err += *m_conf_file;
            err += ": You have to provide a variable value.";
            throw std::runtime_error(err);
        }

        if (!std::regex_match(var_value, var_value_regex)) {
            std::string err = "Error on line ";
            err += std::to_string(line_counter);
            err += " in configuration file: ";
            err += *m_conf_file;
            err += ": invalid variable value: '";
            err += var_value;
            err += "'";
            throw std::runtime_error(err);
        }

        if ('\"' == var_value[0]) {
            var_value = var_value.substr(1,var_value.length()-2);
        }

        if ("mqtt_broker" == var_name) {
            m_mqtt_broker = var_value;
        } else if ("mqtt_port" == var_name) {
            std::optional<uint16_t> value = parse_mqtt_port_value(var_value);
            if (!value) {
                std::string err = "Parsing error in '";
                err += *m_conf_file;
                err += "' on line ";
                err += std::to_string(line_counter);
                err += ": invalid value '";
                err += var_value;
                err += "' for variable '";
                err += var_name;
                err += "'";
                throw std::runtime_error(err);
            }
            m_mqtt_port = *value;
        } else if ("mqtt_user" == var_name) {
            m_mqtt_user = var_value;
        } else if ("mqtt_password" == var_name) {
            m_mqtt_password = var_value;
        } else if ("mqtt_prefix" == var_name) {
            m_mqtt_prefix = var_value;
        } else if ("mqtt_connect_retries" == var_name) {
            std::optional<uint32_t> value = parse_mqtt_connect_retries_value(var_value);
            if (!value) {
                std::string err = "Parsing error in '";
                err += *m_conf_file;
                err += "' on line ";
                err += std::to_string(line_counter);
                err += ": invalid value '";
                err += var_value;
                err += "' for variable '";
                err += var_name;
                err += "'";
                throw std::runtime_error(err);
            }
            m_mqtt_connect_retries = *value;
        } else if ("mqtt_psk" == var_name) {
            std::optional<std::pair<std::string, std::string>> value = parse_mqtt_psk(var_value);
            if (!value) {
                std::string err = "Parsing error in '";
                err += *m_conf_file;
                err += "' on line ";
                err += std::to_string(line_counter);
                err += ": invalid value for variable '";
                err += var_name;
                err += "'";
                throw std::runtime_error(err);
            }
            m_mqtt_psk = value->second;
            m_mqtt_identity = value->first;
        } else if ("mqtt_cafile" == var_name) {
            m_mqtt_cafile = var_value;
        } else if ("mqtt_capath" == var_name) {
            m_mqtt_capath = var_value;
        } else if ("mqtt_certfile" == var_name) {
            m_mqtt_certfile = var_value;
        } else if ("mqtt_keyfile" == var_name) {
            m_mqtt_keyfile = var_value;
        } else if ("use_realtime_scheduler") {
            // ignore: is only used for gcoded
        } else {
            std::string err = "Parsing error in '";
            err += *m_conf_file;
            err += "' on line ";
            err += std::to_string(line_counter);
            err += ": Unknown variable name '";
            err += var_name;
            err += "'";
            throw std::runtime_error(err);
        }
    }
}


/*
 * parse_args()
 */
void ConfigGcode::parse_args(int argc, char **argv)
{
    int option_index;
    optind = 1;
    while (1) {
        int c = getopt_long(argc, argv, short_options_config, long_options_config, &option_index);

        if (-1 == c) {
            break;
        }

        switch(c) {
            case 0:
                if (std::string("mqtt-tls-insecure") == long_options_config[option_index].name) {
                    m_mqtt_tls_insecure = true;
                }
                break;
            case 1:
                if (!m_command) {
                    m_command = argv[optind-1];
                } else {
                    m_command_args.emplace_back(argv[optind-1]);
                }
                break;
            case 'c':
                // ignore, we parsed this already
                break;
            case 'b':
                m_mqtt_broker = optarg;
                break;
            case 'p':
                {
                    std::optional<uint16_t> port = parse_mqtt_port_value(optarg);
                    if (!port) {
                        std::string err = "Invalid argument for option -p/--mqtt-port";
                        throw std::runtime_error(err);
                    }
                    m_mqtt_port = *port;
                }
                break;
            case 'e':
                m_mqtt_prefix = optarg;
                break;
            case 't':
                {
                    std::optional<uint32_t> connect_retries = parse_mqtt_connect_retries_value(optarg);
                    if (!connect_retries) {
                        std::string err = "Invalid argument for option -t/--mqtt-connect_retries";
                        throw std::runtime_error(err);
                    }
                    m_mqtt_connect_retries = *connect_retries;
                }
                break;
            case 'r':
                m_resolve_aliases = false;
                break;
            case 'h':
                m_print_help = true;
                break;
            case 'v':
                m_verbose = true;
                break;
            case '?':
            default:
                std::string err = "Unknown option: '";
                err += argv[optind-1];
                err += "'";
                throw std::runtime_error(err);
                break;
        }
    }
}


/*
 * validate_config()
 */
void ConfigGcode::validate_config()
{
    if ((!mqtt_certfile()) != (!mqtt_keyfile())) {
        throw std::runtime_error("If you set one of 'mqtt_certfile' and 'mqtt_keyfile' in your configuration then you also have to set the other!");
    }

    if ((mqtt_cafile() || mqtt_capath()) && mqtt_psk()) {
        throw std::runtime_error("You can configure TLS either with PSK ('mqtt_psk') or with certificates ('mqtt_cafile'/'mqtt_capath') but not both.");
    }
}


/*
 * parse_mqtt_port_value()
 */
std::optional<uint16_t> ConfigGcode::parse_mqtt_port_value(const std::string &value) const
{
    size_t end;
    int64_t port = std::stol(value, &end, 0);
    if (value.length() != end) {
        return std::nullopt;
    }
    if (0 > port) {
        return std::nullopt;
    }
    if (std::numeric_limits<uint16_t>::max() < port) {
        return std::nullopt;
    }

    return port;
}


/*
 * parse_mqtt_connect_retries_value()
 */
std::optional<uint32_t> ConfigGcode::parse_mqtt_connect_retries_value(const std::string &value) const
{
    size_t end;
    int64_t retries = std::stol(value, &end, 0);
    if (value.length() != end) {
        return std::nullopt;
    }
    if (0 > retries) {
        return std::nullopt;
    }
    if (std::numeric_limits<uint32_t>::max() < retries) {
        return std::nullopt;
    }

    return retries;
}



/*
 * parse_mqtt_psk()
 */
std::optional<std::pair<std::string, std::string>> ConfigGcode::parse_mqtt_psk(const std::string &value) const
{
    std::string::size_type pos_colon = value.find(":");
    if (std::string::npos == pos_colon) {
        return std::nullopt;
    }

    std::string identity = value.substr(0, pos_colon);
    std::string psk = value.substr(pos_colon + 1);

    if (std::string::npos != psk.find_first_not_of("0123456789abcdefABCDEF")) {
        return std::nullopt;
    }

    return std::pair<std::string, std::string>(identity, psk);
}


/*
 * operator<<()
 */
std::ostream& operator<<(std::ostream& out, const ConfigGcode &conf)
{
    out << "Current Configuration:\n";
    out << "conf_file: ";
    if (conf.conf_file()) {
        out << *conf.conf_file() << "\n";
    } else {
        out << "<none>\n";
    }
    out << "mqtt_broker: " << conf.mqtt_broker() << "\n";
    out << "mqtt_port: " << conf.mqtt_port() << "\n";
    out << "mqtt_user: ";
    if (conf.mqtt_user()) {
        out << *conf.mqtt_user() << "\n";
    } else {
        out << "<none>\n";
    }
    out << "mqtt_password: ";
    if (conf.mqtt_password()) {
        //out << *conf.mqtt_password() << "\n";
        out << "***\n";
    } else {
        out << "<none>\n";
    }
    out << "mqtt_prefix: " << conf.mqtt_prefix() << "\n";
    out << "mqtt_connect_retires: ";
    if (conf.mqtt_connect_retries()) {
        out << *conf.mqtt_connect_retries() << "\n";
    } else {
        out << "endless\n";
    }
    out << "mqtt_psk: ";
    if (conf.mqtt_psk()) {
        out << *conf.mqtt_psk() << "\n";
        //out << "***\n";
    } else {
        out << "<none>\n";
    }
    out << "mqtt_identity: ";
    if (conf.mqtt_identity()) {
        out << *conf.mqtt_identity() << "\n";
    } else {
        out << "<none>\n";
    }
    out << "mqtt_cafile: ";
    if (conf.mqtt_cafile()) {
        out << *conf.mqtt_cafile() << "\n";
    } else {
        out << "<none>\n";
    }
    out << "mqtt_capath: ";
    if (conf.mqtt_capath()) {
        out << *conf.mqtt_capath() << "\n";
    } else {
        out << "<none>\n";
    }
    out << "mqtt_certfile: ";
    if (conf.mqtt_certfile()) {
        out << *conf.mqtt_certfile() << "\n";
    } else {
        out << "<none>\n";
    }
    out << "mqtt_keyfile: ";
    if (conf.mqtt_keyfile()) {
        out << *conf.mqtt_keyfile() << "\n";
    } else {
        out << "<none>\n";
    }
    out << "mqtt_tls_insecure: " << ((conf.mqtt_tls_insecure())?("true"):("false")) << "\n";
    out << "resolve_aliases: " << ((conf.resolve_aliases())?("true"):("false")) << "\n";
    out << "verbose: " << ((conf.verbose())?("true"):("false")) << "\n";
    return out;
}
