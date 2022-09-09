#include "DummyDevice.hh"
#include <stdexcept>
#include <iostream>


/*
 * DummyDevice()
 */
DummyDevice::DummyDevice(const std::string &name)
    : m_device(name)
{
    set_state(State::OK);
}


/*
 * ~DummyDevice()
 */
DummyDevice::~DummyDevice() 
{
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
    std::cout << "TODO: DummyDevice::print() not yet implemented!\n";
    return Device::PrintResult::OK;
}
