#include "test_header.hh"
#include <iostream>
#include <mqtt_messages/MsgDeviceState.hh>

int main(int argc, char **argv)
{
    {
        MsgDeviceState orig;
        std::vector<char> msg;
        orig.encode(msg);
        if (msg.size() != 2) {
            return FAIL;
        }
        if (MsgType::Type::DEVICE_STATE != (MsgType::Type)msg[0]) {
            return FAIL;
        }
        if (Device::State::UNINITIALIZED != (Device::State)msg[1]) {
            return FAIL;
        }
        MsgDeviceState copy;
        copy.decode(msg);

        if (orig != copy) {
            return FAIL;
        }
    }

    {
        MsgDeviceState orig;
        std::vector<char> msg;
        orig.encode(msg);
        if (msg.size() != 2) {
            return FAIL;
        }
        if (MsgType::Type::DEVICE_STATE != (MsgType::Type)msg[0]) {
            return FAIL;
        }
        if (Device::State::UNINITIALIZED != (Device::State)msg[1]) {
            return FAIL;
        }
        msg[1] = 255;
        MsgDeviceState copy;
        bool got_exception = false;
        try {
            copy.decode(msg);
        } catch (const std::runtime_error &e) {
            got_exception = true;
        }
        if (!got_exception) {
            return FAIL;
        }


    }
    return SUCCESS;
}
