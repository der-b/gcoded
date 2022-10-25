#include <iostream>
#include <fstream>
#include <chrono>
#include <thread>
#include <atomic>
#include "ConfigGcode.hh"
#include "client/Client.hh"

/*
 * send()
 */
int send(Client &client, const ConfigGcode &conf)
{
    if (!conf.command() || "send" != *conf.command()) {
        std::cerr << "Invalid command!\n";
        return 1;
    }

    const size_t c_args_size = conf.command_args().size();
    if (0 >= c_args_size) {
        std::cerr << "You have to provide a path to the gcode file. See 'gcode send --help'.\n";
        return 1;
    }

    if (2 < c_args_size) {
        std::cerr << "Too many arguments for send command. See 'gcode send --help'.\n";
        return 1;
    }

    const std::string &filename = conf.command_args()[0];
    if (!std::filesystem::exists(filename)) {
        std::cerr << "Gcode file does not exist: " << filename << "\n";
        return 1;
    }

    std::string hint = "*";
    if (2 <= c_args_size) {
        hint = conf.command_args()[1];
    }
    std::unique_ptr<std::vector<Client::DeviceInfo>> devices = client.devices(hint);
    
    if (0 == devices->size()) {
        std::cerr << "No devices found.\n";
    }

    if ( 1 < devices->size()) {
        std::cout << "Found " << devices->size() << " devices. If you want to send the gcode to all of these devicese, than enter the number of devices.\n";
        std::cout << "No. of devices: ";
        std::string user_input;
        std::getline(std::cin, user_input);
        size_t dev_count = std::strtoul(user_input.c_str(), nullptr, 0);
        if (devices->size() != dev_count) {
            return 1;
        }
    }

    std::ifstream gcode_file;
    gcode_file.open(filename);
    if (!gcode_file.is_open()) {
        std::cerr << "Failed to open file: " << filename << "\n";
        return 1;
    }
    std::string gcode((std::istreambuf_iterator<char>(gcode_file)), (std::istreambuf_iterator<char>()));

    std::atomic_int count = devices->size();
    // TODO: Print only for devices, which have an correct device state. This avoids sending gcode via network, which cant be printed anyway
    for (const auto &dev: *devices) {
        client.print(dev, gcode, [&count](const Client::DeviceInfo &dev, Device::PrintResult res) {
            std::cout << "print " << dev.provider << "/" << dev.name << " " << Device::printres_to_str(res) << "\n";
            count -= 1;
        });
    }

    while (count != 0) {
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
    
    return 0;
}


/*
 * alias()
 */
int alias(Client &client, const ConfigGcode &conf)
{
    if (!conf.command() || "alias" != *conf.command()) {
        std::cerr << "Invalid command!\n";
        return 1;
    }

    const size_t c_args_size = conf.command_args().size();
    if (0 >= c_args_size) {
        std::cerr << "You have to provide an ACTION. See 'gcode alias --help'.\n";
        return 1;
    }

    if ("list" == conf.command_args()[0]) {
        std::unique_ptr<std::map<std::string, std::string>> provider_aliases = client.get_provider_aliases();
        if (provider_aliases && 0 < provider_aliases->size()) {
            std::cout << "Provider aliases:\n";
            for (const auto &alias: *provider_aliases) {
                std::cout << "  " << alias.first << " -> " << alias.second << "\n";
            }
        }

        std::unique_ptr<std::map<std::string, std::string>> device_aliases = client.get_device_aliases();
        if (device_aliases && 0 < device_aliases->size()) {
            std::cout << "Device aliases:\n";
            for (const auto &alias: *device_aliases) {
                std::cout << "  " << alias.first << " -> " << alias.second << "\n";
            }
        }
    } else if ("set" == conf.command_args()[0]) {
        const size_t count = conf.command_args().size();
        if (3 != count && 4 != count) {
            std::cerr << "Wrong argument count: See 'gcode alias --help'.\n";
        }
        if ("provider" == conf.command_args()[1]) {
            std::string alias;
            if (count == 4) {
                alias = conf.command_args()[3];
            }
            if (!client.set_provider_alias(conf.command_args()[2], alias)) {
                auto providers = client.get_providers(conf.command_args()[2]);
                if (!providers || 0 == providers->size()) {
                    std::cerr << "No provider fond which matches: '" + conf.command_args()[2] + "'\n";
                } else {
                    std::cerr << "More than one provider fond which matches: '" + conf.command_args()[2] + "':\n";
                    for (const auto &provider: *providers) {
                        std::cerr << "  " << provider << "\n";
                    }
                }
                return 1;
            }
        } else if ("device" == conf.command_args()[1]) {
            std::string alias;
            if (count == 4) {
                alias = conf.command_args()[3];
            }
            if (!client.set_device_alias(conf.command_args()[2], alias)) {
                auto devices = client.devices(conf.command_args()[2]);
                if (!devices || 0 == devices->size()) {
                    std::cerr << "No device fond which matches: '" + conf.command_args()[2] + "'\n";
                } else {
                    std::cerr << "More than one device fond which matches: '" + conf.command_args()[2] + "':\n";
                    for (const auto &device: *devices) {
                        std::cerr << "  " << device.provider << "/" << device.name << "\n";
                    }
                }
                return 1;
            }
        } else {
            std::cerr << "Unknown alias TYPE: '" + conf.command_args()[1] + "'. (See 'gcode alias --help')\n";
            return 1;
        }
    } else {
        std::cerr << "Unknown ACTION: '" << conf.command_args()[0] << "'. See 'gcode alias --help'.\n";
        return 1;
    }

    return 0;
}


/*
 * main()
 */
int main(int argc, char **argv)
{
    ConfigGcode conf(argc, argv);

    if (conf.print_help() || !conf.command()) {
        std::cout << conf.usage();
        std::cout << conf.help();
        std::cout << "\n";
        std::cout << conf;
        std::cout << "\n";
        return 0;
    }

    Client client(conf);

    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    if ("list" == *conf.command()) {
        const size_t c_args_size = conf.command_args().size();
        if (1 < c_args_size) {
            std::cerr << "Too many arguments for list command. See 'gcode list --help'.\n";
            return 1;
        }
        std::string hint = "*";
        if (1 <= c_args_size) {
            hint = conf.command_args()[0];
        }
        std::unique_ptr<std::vector<Client::DeviceInfo>> devices = client.devices(hint);

        for (const auto &dev: *devices) {
            if (conf.resolve_aliases() && dev.provider_alias.size()) {
                std::cout << dev.provider_alias;
            } else {
                std::cout << dev.provider;
            }
            std::cout << "/";
            if (conf.resolve_aliases() && dev.device_alias.size()) {
                std::cout << dev.device_alias;
            } else {
                std::cout << dev.name;
            }
            std::cout << " " << Device::state_to_str(dev.state);
            if (dev.state == Device::State::PRINTING) {
                int hours = dev.print_remaining_time / 60;
                int min = dev.print_remaining_time % 60;
                std::cout << " (" << (int)dev.print_percentage << "%, remaining ";
                std::cout << std::setw(2) << std::setfill('0') << hours << ":";
                std::cout << std::setw(2) << std::setfill('0') << min << " [hh:mm])";
            }
            std::cout << "\n";
        }
    } else if ("send" == *conf.command()) {
        return send(client, conf);
    } else if ("alias" == *conf.command()) {
        return alias(client, conf);
    } else {
        std::cerr << "Unknown command: \"" << *conf.command() << "\".\nSee --help for more information.\n";
        return 1;
    }

    return 0;
}

