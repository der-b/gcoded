#include "MsgSensorReadings.hh"
#include <iostream>
#include <sys/random.h>
#include <functional>
#include <bitset>

/*
 * MsgSensorReadings()
 */
MsgSensorReadings::MsgSensorReadings()
    : m_type(MsgType::Type::SENSOR_READINGS)
{
    m_msg.count = 0;
}

#define SRHEADER_FIELDS_SET_POINT 1

struct SRHeader {
    // flags to indicate wich optional fields are present.
    uint8_t fields;
    uint8_t size_name;
    uint8_t size_unit;
} __attribute__((packed));

/*
 * encode()
 */
void MsgSensorReadings::encode(std::vector<char> &encoded_msg) const
{
    SRHeader head;
    m_type.encode(encoded_msg);
    encoded_msg.insert(encoded_msg.end(), (char *)&m_msg, ((char *)&m_msg) + sizeof(m_msg));
    for (const auto &value: m_readings) {
        head.size_name = value.first.size();
        if (value.first.size() > 0xff) {
                throw std::runtime_error("Sensor name is longer than the allowed maximum of 0xff.");
        }
        head.size_unit = 0;
        if (value.second.unit) {
            if (value.second.unit->size() > 0xff) {
                throw std::runtime_error("Unit filed is longer than the allowed maximum of 0xff.");
            }
            head.size_unit = value.second.unit->size();
        }
        head.fields = 0;
        if (value.second.set_point) {
            head.fields |= SRHEADER_FIELDS_SET_POINT;
        }
        encoded_msg.insert(encoded_msg.end(), (char *)&head, ((char *)&head) + sizeof(head));
        encoded_msg.insert(encoded_msg.end(),
                           (char *)&value.second.current_value,
                           ((char *)&value.second.current_value) + sizeof(value.second.current_value));
        if (value.second.set_point) {
            encoded_msg.insert(encoded_msg.end(),
                               (char *)&*value.second.set_point,
                               ((char *)&*value.second.set_point) + sizeof(*value.second.set_point));
        }
        encoded_msg.insert(encoded_msg.end(), (char *)value.first.data(), ((char *)value.first.data()) + value.first.size());
        if (value.second.unit) {
            encoded_msg.insert(encoded_msg.end(),
                               (char *)value.second.unit->data(),
                               (char *)(value.second.unit->data()) + value.second.unit->size());
        }
    }
}


/*
 * decode()
 */
size_t MsgSensorReadings::decode(const std::vector<char> &encoded_msg)
{
    size_t pos = m_type.decode(encoded_msg);
    size_t tmp_size;
    std::function<void(size_t)> check_size = [&](size_t needed) {
            if (encoded_msg.size() - pos < needed) {
                throw std::runtime_error("MsgSensorReadings::decode(): Invalid encoded message: message to short");
            }
        };

    if (m_type.type() != MsgType::Type::SENSOR_READINGS) {
        throw std::runtime_error("MsgSensorReadingsResponse::decode(): Wrong message type.");
    }

    tmp_size = sizeof(m_msg);
    check_size(tmp_size);
    memcpy(&m_msg, encoded_msg.data() + pos, tmp_size);
    pos += tmp_size;

    for (uint32_t i = 0; i < m_msg.count; ++i) {

        tmp_size = sizeof(SRHeader);
        check_size(tmp_size);
        SRHeader *head = (SRHeader *)(encoded_msg.data() + pos);
        Device::SensorValue value;
        pos += tmp_size;

        tmp_size = sizeof(value.current_value);
        check_size(tmp_size);
        memcpy((char *)&value.current_value, encoded_msg.data() + pos, tmp_size);
        pos += tmp_size;

        if (head->fields & SRHEADER_FIELDS_SET_POINT) {
            double set_point;
            tmp_size = sizeof(set_point);
            check_size(tmp_size);
            memcpy((char *)&set_point, encoded_msg.data() + pos, tmp_size);
            value.set_point = set_point;
            pos += tmp_size;
        }

        tmp_size = head->size_name;
        check_size(tmp_size);
        std::string name(encoded_msg.data() + pos, tmp_size);
        for (const char &ch: name) {
            if (!std::isprint(ch)) {
                throw std::runtime_error("Sensor name contains nonprintable characters! (mqtt message)");
            }
        }
        pos += tmp_size;

        if (head->size_unit) {
            tmp_size = head->size_unit;
            check_size(tmp_size);
            std::string unit(encoded_msg.data() + pos, tmp_size);
            for (const char &ch: unit) {
                if (!std::isprint(ch)) {
                    throw std::runtime_error("Unit contains nonprintable characters! (mqtt message)");
                }
            }
            value.unit = unit;
            pos += tmp_size;
        }

        m_readings[name] = value;
    }
    return pos;
}


/*
 * add_sensor_reading()
 */
void MsgSensorReadings::add_sensor_reading(const std::string &sensor_name, const Device::SensorValue &value)
{
    m_readings[sensor_name] = value;
    if (m_readings.size() > 0xff) {
        throw std::runtime_error("Too many sensor readings for mqtt message.");
    }
    m_msg.count = m_readings.size();
}
