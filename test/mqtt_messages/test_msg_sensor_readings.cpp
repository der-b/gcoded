#include "test_header.hh"
#include <iostream>
#include <mqtt_messages/MsgSensorReadings.hh>
#include <devices/Device.hh>

int main(int argc, char **argv)
{
    std::map<std::string, Device::SensorValue> readings;

    {
        MsgSensorReadings orig;
        std::vector<char> msg;
        orig.encode(msg);
        if (MsgType::Type::SENSOR_READINGS != (MsgType::Type)msg[0]) {
            return FAIL;
        }
        MsgSensorReadings copy;
        copy.decode(msg);

        if (orig != copy) {
            return FAIL;
        }
    }

    Device::SensorValue sv;
    sv.current_value = 1.1;
    sv.unit = "m";
    sv.set_point = 2.2;
    readings["first"] = sv;
    sv.current_value = 3.3;
    sv.unit = "m/s";
    sv.set_point.reset();
    readings["second"] = sv;
    sv.current_value = 4.4;
    sv.unit.reset();
    sv.set_point = 5.5;
    readings["third"] = sv;
    sv.current_value = 4.4;
    sv.unit.reset();
    sv.set_point.reset();
    readings["forth"] = sv;

    {
        MsgSensorReadings orig;
        std::vector<char> msg;
        for (const auto value: readings) {
            orig.add_sensor_reading(value.first, value.second);
        }
        orig.encode(msg);
        if (MsgType::Type::SENSOR_READINGS != (MsgType::Type)msg[0]) {
            return FAIL;
        }
        MsgSensorReadings copy;
        copy.decode(msg);

        if (orig != copy) {
            return FAIL;
        }
    }

    {
        MsgSensorReadings orig;
        std::vector<char> msg;
        orig.encode(msg);
        if (MsgType::Type::SENSOR_READINGS != (MsgType::Type)msg[0]) {
            return FAIL;
        }
        msg[1] = 255;
        MsgSensorReadings copy;
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
