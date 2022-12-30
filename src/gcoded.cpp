#include <iostream>
#include "Config.hh"
#include "devices/Detector.hh"
#include "MQTT.hh"
#include "Interface.hh"
#include "Inotify.hh"
#include "Aliases.hh"
#include <unistd.h>
#include <signal.h>

bool running = true;

void sig_handler(int)
{
    std::cout << "shutdown\n";
    running = false;
}

int main(int argc, char **argv) 
{
    signal(SIGINT, sig_handler);
    Config conf(argc, argv);

    if (conf.print_help()) {
        std::cout << conf.usage();
        std::cout << conf.help();
        std::cout << "\n";
        std::cout << conf;
        std::cout << "\n";
        return 0;
    }

    Detector &detec = Detector::get(conf);
    {
        Inotify &bla = Inotify::get();
        Aliases aliases(conf);
        Interface interface(conf, aliases);

        while(running) {
            sleep(1);
        }
    }

    detec.shutdown();

    return 0;
}

