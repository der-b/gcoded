###
### gcoded
###
add_executable(gcoded
               Config.cpp
               gcoded.cpp
               EventLoop.cpp
               devices/Device.cpp
               devices/Detector.cpp
               devices/prusa/PrusaDevice.cpp
               devices/prusa/PrusaDetector.cpp
               devices/dummy/DummyDetector.cpp
               devices/dummy/DummyDevice.cpp
               MQTT.cpp
               Inotify.cpp
               Interface.cpp
               Aliases.cpp
               mqtt_messages/MsgDeviceState.cpp
               mqtt_messages/MsgPrint.cpp
               mqtt_messages/MsgPrintResponse.cpp
               mqtt_messages/MsgPrintProgress.cpp
               mqtt_messages/MsgAliases.cpp
               mqtt_messages/MsgAliasesSet.cpp
               mqtt_messages/MsgAliasesSetProvider.cpp
               mqtt_messages/MsgSensorReadings.cpp
               mqtt_messages/MsgType.cpp)

target_link_libraries(gcoded
                      event_core
                      event_pthreads
                      pthread
                      stdc++fs
                      mosquitto
                      ${SQLite3_LIBRARY})

###
### gcode
###
add_executable(gcode
               ConfigGcode.cpp
               client/Client.cpp
               MQTT.cpp
               mqtt_messages/MsgDeviceState.cpp
               mqtt_messages/MsgPrint.cpp
               mqtt_messages/MsgPrintResponse.cpp
               mqtt_messages/MsgPrintProgress.cpp
               mqtt_messages/MsgAliases.cpp
               mqtt_messages/MsgAliasesSet.cpp
               mqtt_messages/MsgAliasesSetProvider.cpp
               mqtt_messages/MsgType.cpp
               gcode.cpp)

target_link_libraries(gcode
                      mosquitto
                      pthread
                      stdc++fs
                      ${SQLite3_LIBRARY})
