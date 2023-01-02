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
The usage of topic within MQTT makes it necessary to implement a topic collision avoidance algorithm.
See section [Topic Collision Avoidance](#topic-collision-avoidance) for more information.

And this is a good opportunity to play around with the realtime scheduler which was introduces in the Linux kernel 2.6.26 (see 'man 7 sched').

## Getting started

### Prerequisites

Gcoded only supports Linux at the moment and need C++17 compiler.
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

### MQTT Broker

You need a running MQTT Broker. Gcoded was is tested with [Mosquitto](https://mosquitto.org/). For the examples below, we assume
that the MQTT broker runs on localhost on the default port 1883. Depending on the used Linux Distribution you can start Mosquitto
as root with:

``` bash
systemctl start mosquitto
```

### Start the Gcoded daemon

Make sure, that you have a running MQTT broker on localhost.
If the broker is on a different machine, than use the CLI option '-b' (see: './gcode --help').

``` bash
cd gcoded/build
./gcoded
```

### Using the client

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

The Topic Collision Avoidance is based on the [MQTT ClientID](https://docs.oasis-open.org/mqtt/mqtt/v5.0/os/mqtt-v5.0-os.html#_Toc3901059) and the requirement that the MQTT Broker must ensure that each clients has a unique ClientID.
Furthermore, it does not make sense to have two gcoded daemons running on the same host, since each daemon can handle several 3D printers.

During the start, gcoded checks whether the file '/var/lib/gcoded/id' does exists. If so, it reads the ID, an hexadecimal random number, from this file and uses it as ClientID.
If the file does not exist, it generates a new ClientID and tries write it to '/var/lib/gcoded/id'.
On success it, uses the new ID as ClientID.
If the file is not writeable, than gcoded creates a temporary ClientID, with a prefix 'temp-' following an hexadecimal random number.

From this follows, that each gcoded daemon has a permanent MQTT ClientID even after a reboot.
If gcoded has not the necessary permission to read/write the file '/var/lib/gcoded/id' than each restart of the daemon will generate a new ClientID.

This ClientID is also used as unique identifier in the MQTT topics dedicated to a one gcoded daemon.

**Notice:** If a second client with the same ClientID connects to mosquitto, than first client will be disconnected.
Since gcoded automatically reconnects after a connection loss, both clients will alternately connect and disconnect.
If this happens, gcoded will generate a warning: *"WARNING: MQTT session takeover. Another gcoded daemon with the same MQTT ClientID connected to the MQTT broker."*
Unfortunately, prior to Mosquitto version 2.0.15 there was a [bug](https://github.com/eclipse/mosquitto/issues/2607), which retuned a wrong reason coded in case of a session takeover.
Therefor, if you use a version before 2.0.15, this warning will not be generated.

## Supported Devices

- Prusa i3 MK3S+ (probably all Prusa devices, but i have only one for testing)
