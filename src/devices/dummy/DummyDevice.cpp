#include "DummyDevice.hh"
#include <stdexcept>
#include <iostream>
#include <sstream>
#include <unistd.h>


/*
 * DummyDevice()
 */
DummyDevice::DummyDevice(const std::string &name)
    : m_device(name),
      m_running(true)
{
    set_state(State::OK);
    m_sensor_readings_job = std::thread([this]() {
                Device::SensorValue sv;
                sv.current_value = 1.1;
                sv.unit = "m";
                sv.set_point = 2.2;
                m_sensor_readings["first"] = sv;
                sv.current_value = 3.3;
                sv.unit = "m/s";
                sv.set_point.reset();
                m_sensor_readings["second"] = sv;
                sv.current_value = 4.4;
                sv.unit.reset();
                sv.set_point = 5.5;
                m_sensor_readings["third"] = sv;
                sv.current_value = 4.4;
                sv.unit.reset();
                sv.set_point.reset();
                m_sensor_readings["forth"] = sv;

                while(m_running) {
                    update_sensor_readings();
                    for (auto it = m_sensor_readings.begin(); it != m_sensor_readings.end(); it++) {
                        it->second.current_value *= 1.01;
                        if (it->second.set_point) {
                            (*it->second.set_point) *= 1.01;
                        }
                    }
                    sleep(1);
                }
            });
}


/*
 * ~DummyDevice()
 */
DummyDevice::~DummyDevice()
{
    m_running = false;
    if (m_print_job.joinable()) {
        m_print_job.join();
    }
    if (m_sensor_readings_job.joinable()) {
        m_sensor_readings_job.join();
    }
}


/*
 * print_file()
 */
Device::PrintResult DummyDevice::print_file(const std::string &file_path)
{
    throw std::runtime_error("DummyDevice::print_file() not yet implemented!");
}


/*
 * print()
 */
Device::PrintResult DummyDevice::print(const std::string &gcode)
{
    if (state() != Device::State::OK) {
        return PrintResult::ERR_INVALID_STATE;
    }
    const std::lock_guard<std::mutex> guard(m_mutex);
    if (!m_curr_print.empty()) {
        return PrintResult::ERR_PRINTING;
    }
    std::istringstream gcode_stream(gcode);

    auto trim = [](std::string &s) {
        s.erase(0, s.find_first_not_of(" \n\r\t\f\v"));
        s.erase(s.find_last_not_of(" \n\r\t\f\v") + 1);
    };

    while (!gcode_stream.eof()) {
        std::string line;
        std::getline(gcode_stream, line);
        std::string::size_type pos = line.find(';');
        if (std::string::npos != pos) {
            line = line.substr(0, pos);
        }
        if (0 == line.size()) {
            continue;
        }
        trim(line);
        if (0 == line.size()) {
            continue;
        }
        m_curr_print.push_back(line);
    }

    set_state(Device::State::PRINTING);
    m_commands = m_curr_print.size();
    m_progress = 0;
    m_remaining_time = m_curr_print.size() * 10 / 1000 / 60;
    update_progress(m_progress, m_remaining_time);

    if (m_print_job.joinable()) {
        m_print_job.join();
    }

    m_print_job = std::thread([this](){
            while(1) {
                {
                    const std::lock_guard<std::mutex> guard(m_mutex);
                    if (!m_running) {
                        return;
                    }
                    if (m_curr_print.empty()) {
                        break;
                    }
                    m_curr_print.pop_front();
                    int new_progress = 100 - (m_curr_print.size() * 100) / m_commands;
                    int remaining_time = m_curr_print.size() * 10 / 1000 / 60;
                    if (   new_progress != m_progress
                        || remaining_time != m_remaining_time) {
                        m_progress = new_progress;
                        m_remaining_time = remaining_time;
                        update_progress(m_progress, m_remaining_time);
                    }
                }
                std::this_thread::sleep_for(std::chrono::milliseconds(10));
            }
            {
                const std::lock_guard<std::mutex> guard(m_mutex);
                update_progress(100, 0);
                set_state(Device::State::OK);
            }
    });

    return Device::PrintResult::OK;
}
