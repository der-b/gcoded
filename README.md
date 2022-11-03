# Gcoded

Gcoded is a background daemon which detects devices connected to local machine and exposes an interface via MQTT, which allows to use these devices.
Gcoded tries to avoid MQTT topic collision.
That means, several Gcoded instances on different machines can coexist on the same MQTT broker.
Therefore, all exposed devices can be controlled with the same command line interface (CLI), even if they are connected to different machines.

One of the design goals is, that it shall have system requirements as low as possible.
At least it shall be possible to use a Raspberry Pi 1B.

## WARNING: Under construction

Currently, this implementation is only a proof of concept.
Therefore, only the elemental functionality is implemented and all interfaces are subjected to changes (e.g.: UI, API, Network Protocol).

## Motivation

The primary use case of Gcoded is to get rid of the SD card needed to provide the G-Code files to a 3D printer.
Although project like [OctoPrint](https://octoprint.org/) cover this use case, i could not use it.
I only had a Raspberry Pi 1B laying around, but [OctoPrint doesn't support them](https://octoprint.org/download/#octopi).
Unfortunately, i could not get a Raspberry Pi 4 due to the chip shortage in the beginning of 2022.

A secondary use case is the possibility to manage several 3D printer with one or more Gcoded instances in parallel.
This use case is a side effect due to the usage of MQTT as communication protocol.
The usage of topic within MQTT makes it necessary to implement topic collision avoidance.
See section [Topic Collision Avoidance](#topic-collision-avoidance) for more information.

## Getting started

### Prerequisites

Gcoded only supports Linux at the moment and need C++17.
To compile this project you need following software packages:
- [libevent](https://libevent.org/)
- [libmosquitto](https://mosquitto.org/)
- [sqlite3](https://sqlite.org/)

### Download and compiling

``` bash
git clone https://github.com/der-b/gcoded.git
cd gcoded
mkdir build
cd build
cmake ..
make
```

### Start the Gcoded daemon

Make sure, that you have a running MQTT broker on localhost.
If the broker is on a different machine, than use the CLI option '-b' (see: './gcode --help').

``` bash
cd gcoded/build
./gcoded
```

### Using the client

The following examples assume, that the MQTT broker is running on localhost.
If this is not the case, than use the '-b' option (see: './gcode --help' or './gcoded --help').

List all detected devices:

``` bash
cd gcoded/build
./gcode list
```

Send a G-code file to **_all_** available printers:

``` bash
cd gcoded/bild
./gcode send path/to/gcode_file.gcode
```

Send a G-code file to a specific device:

``` bash
cd gcoded/bild
./gcode send path/to/gcode_file.gcode DeviceName
```

## Topic Collision Avoidance

TODO: Write section

## Supported Devices

- Prusa i3 MK3S+ (probably all Prusa devices, but i have only one for testing)
