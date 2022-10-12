add_executable(test_msg_type EXCLUDE_FROM_ALL
    test_msg_type.cpp
    ../../src/mqtt_messages/MsgType.cpp)
add_dependencies(check test_msg_type)
add_test(NAME test_msg_type COMMAND test_msg_type)

add_executable(test_msg_device_state EXCLUDE_FROM_ALL
    test_msg_device_state.cpp
    ../../src/mqtt_messages/MsgDeviceState.cpp
    ../../src/mqtt_messages/MsgType.cpp)
add_dependencies(check test_msg_device_state)
add_test(NAME test_msg_device_state COMMAND test_msg_device_state)

add_executable(test_msg_print EXCLUDE_FROM_ALL
    test_msg_print.cpp
    ../../src/mqtt_messages/MsgPrint.cpp
    ../../src/mqtt_messages/MsgType.cpp)
add_dependencies(check test_msg_print)
add_test(NAME test_msg_print COMMAND test_msg_print)

add_executable(test_msg_print_response EXCLUDE_FROM_ALL
    test_msg_print_response.cpp
    ../../src/mqtt_messages/MsgPrintResponse.cpp
    ../../src/mqtt_messages/MsgPrint.cpp
    ../../src/mqtt_messages/MsgType.cpp)
add_dependencies(check test_msg_print_response)
add_test(NAME test_msg_print_response COMMAND test_msg_print_response)

add_executable(test_msg_print_progress EXCLUDE_FROM_ALL
    test_msg_print_progress.cpp
    ../../src/mqtt_messages/MsgPrintProgress.cpp
    ../../src/mqtt_messages/MsgType.cpp)
add_dependencies(check test_msg_print_progress)
add_test(NAME test_msg_print_progress COMMAND test_msg_print_progress)

add_executable(test_msg_aliases EXCLUDE_FROM_ALL
    test_msg_aliases.cpp
    ../../src/mqtt_messages/MsgAliases.cpp
    ../../src/mqtt_messages/MsgType.cpp)
add_dependencies(check test_msg_aliases)
add_test(NAME test_msg_aliases COMMAND test_msg_aliases)
