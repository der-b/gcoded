#ifndef __DUMMY_DEVICE_HH__
#define __DUMMY_DEVICE_HH__

#include "../Device.hh"
#include <mutex>
#include <list>
#include <thread>

class DummyDevice : public Device {
    public:
        DummyDevice() = delete;
        DummyDevice(const DummyDevice &) = delete;
        DummyDevice(DummyDevice &&) = delete;
        DummyDevice &operator=(const DummyDevice &) = delete;

        DummyDevice(const std::string &name);
        virtual ~DummyDevice();

        virtual PrintResult print_file(const std::string &file_path) override;
        virtual PrintResult print(const std::string &gcode) override;
        virtual const std::string &name() const override 
        {
            return m_device;
        }

        virtual std::map<std::string, struct Device::SensorValue> sensor_readings() override
        {
            return m_sensor_readings;
        }
    private:
        std::string m_device;
        std::mutex m_mutex;
        std::list<std::string> m_curr_print;
        std::thread m_print_job;
        std::thread m_sensor_readings_job;
        std::map<std::string, Device::SensorValue> m_sensor_readings;
        size_t m_commands;
        int m_progress;
        int m_remaining_time;
        bool m_running;
};

#endif
