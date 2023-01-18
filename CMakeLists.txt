cmake_minimum_required(VERSION 3.10)
project(gcoded VERSION 0.1)

# enable generator expressions in install(CODE) and install(SCRIPT)
cmake_policy(SET CMP0087 NEW)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

enable_testing()

include_directories("${PROJECT_SOURCE_DIR}/src")

add_custom_target(check COMMAND ${CMAKE_CTEST_COMMAND})

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR})

#### check for libmosquitto
find_library(MOSQUITTO_LIBRARY mosquitto REQUIRED)
find_file(MOSQUITTO_HEADER "mosquitto.h")
if(NOT MOSQUITTO_HEADER) 
    message(FATAL_ERROR "Couldn't find header file 'mosquitto.h'")
endif()

#### check for libevent
find_library(EVENT_LIBRARY event REQUIRED)
find_file(EVENT_HEADER "event2/event.h")
if(NOT EVENT_HEADER) 
    message(FATAL_ERROR "Couldn't find header file 'event2/event.h'")
endif()

find_package(SQLite3 REQUIRED)

configure_file("conf/gcoded.conf" "${CMAKE_BINARY_DIR}/gcoded.conf" COPYONLY)

add_subdirectory(src)
add_subdirectory(conf)
add_subdirectory(test/mqtt_messages)
