#include "DummyDevice.hh"
#include <stdexcept>
#include <iostream>
#include <sstream>


/*
 * DummyDevice()
 */
DummyDevice::DummyDevice(const std::string &name)
    : m_device(name),
      m_running(true)
{
    set_state(State::OK);
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
    set_state(State::DISCONNECTED);
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
    if (!is_valid()) {
        return PrintResult::ERR_INVALID_STATE;
    }
    const std::lock_guard<std::mutex> guard(m_mutex);
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
