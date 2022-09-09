#!/bin/sh

while [ 1 -eq 1 ]
do
    echo "pub"
    mosquitto_pub -t "test" -m "hello Mosquitto"
    sleep 1
done
