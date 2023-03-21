#ifndef __MSG_SENSORREADINGS_HH__
#define __MSG_SENSORREADINGS_HH__

#include <string>
#include "Msg.hh"
#include "MsgType.hh"
#include "../devices/Device.hh"

class MsgSensorReadings : public Msg {
    public:
        MsgSensorReadings();
        virtual ~MsgSensorReadings() {};

        struct header_msg {
            uint8_t count;
        };

        bool operator==(const MsgSensorReadings &b)
        {
            return    m_type == b.m_type
                   && 0 == memcmp(&m_msg, &b.m_msg, sizeof(m_msg))
                   && m_readings == b.m_readings;
        }

        bool operator!=(const MsgSensorReadings &b)
        {
            return !(*this == b);
        }

        void encode(std::vector<char> &encoded_msg) const override;
        size_t decode(const std::vector<char> &encoded_msg) override;

        void add_sensor_reading(const std::string &sensor_name, const Device::SensorValue &value);

        const std::map<std::string, Device::SensorValue> &sensor_readings() const
        {
            return m_readings;
        }

    private:
        MsgType m_type;
        struct header_msg m_msg;
        std::map<std::string, Device::SensorValue> m_readings;
};

#endif
