#include "ConfigGcode.hh"
#include <getopt.h>
#include <iostream>
#include <fstream>
#include <regex>
#include <sys/random.h>

const struct option long_options_config[] = {
    { "config",      required_argument, 0, 'c' },
    { "mqtt-broker", required_argument, 0, 'b' },
    { "mqtt-port",   required_argument, 0, 'p' },
    { "mqtt-prefix", required_argument, 0, 'e' },
    { "real-names",  no_argument,       0, 'r' },
    { "verbose",     no_argument,       0, 'v' },
    { "help",        no_argument,       0, 'h' },
    { 0, 0, 0, 0 }
};

const char short_options_config[] = "-c:b:p:e:rvh";

const char usage_message[] = "gcode [OPTIONS] [COMMAND]\n";
const char help_message[] = 
"This program controls is a command line interface to the gcoded daemons registered to an MQTT broker.\n"
"\n"
"OPTIONS:\n"
"-c, --config=file            Configuration file to load.\n"
"-b, --mqtt-broker=hostname   Hostname or IP of the MQTT broker.\n"
"-p, --mqtt-port=port         Port of the MQTT broker.\n"
"-e, --mqtt-prefix=prefix     MQTT topic under which gcoded will expose the interface.\n"
"-r, --real-names             Do not use aliases for device and provider. Use real names.\n"
"-v, --verbose                Enable debug output.\n"
"-h, --help                   Print help message and configuration.\n"
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
    m_mqtt_client_id = "";
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
                err += ": invalid variable value '";
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
    out << "resolve_aliases: " << ((conf.resolve_aliases())?("true"):("false")) << "\n";
    out << "verbose: " << ((conf.verbose())?("true"):("false")) << "\n";
    return out;
}
