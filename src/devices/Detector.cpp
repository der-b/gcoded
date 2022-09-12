#include "Detector.hh"
#include "prusa/PrusaDetector.hh"
#include "dummy/DummyDetector.hh"
#include <iostream>

/*
 * Detector()
 */
Detector::Detector(const Config &conf)
    : m_conf(conf)
{
    PrusaDetector::get().register_on_new_device(this);
    if (conf.load_dummy()) {
        DummyDetector::get().register_on_new_device(this);
    }
}


/*
 * ~Detector()
 */
Detector::~Detector()
{
    const std::lock_guard<std::mutex> guard(m_mutex);
    for (auto &dev: m_devices) {
        dev->unregister_listener(this);
    }
}


/*
 * on_new_prusa_device()
 */
void Detector::on_new_prusa_device(const std::shared_ptr<Device> &device) 
{
    const std::lock_guard<std::mutex> guard(m_mutex);
    std::cout << "New prusa device: " << device->name() << "\n";
    device->register_listener(this);

    if (device->is_valid()) {
        m_devices.push_back(device);
        for (auto &listener: m_listeners) {
            listener->on_new_device(device);
        }
    }
}


/*
 * on_new_dummy_device()
 */
void Detector::on_new_dummy_device(const std::shared_ptr<Device> &device) 
{
    const std::lock_guard<std::mutex> guard(m_mutex);
    std::cout << "New dummy device: " << device->name() << "\n";
    device->register_listener(this);

    if (device->is_valid()) {
        m_devices.push_back(device);
        for (auto &listener: m_listeners) {
            listener->on_new_device(device);
        }
    }
}


/*
 * on_stat_change()
 */
void Detector::on_state_change(Device &device, enum Device::State new_state)
{
    if (   new_state != Device::State::ERROR
        && new_state != Device::State::DISCONNECTED) {
        return;
    }
    const std::lock_guard<std::mutex> guard(m_mutex);
    std::cout << "Remove Prusa Device: " << device.name() << "\n";
    m_devices.remove_if([&device, this](const std::shared_ptr<Device> &dev) {
            if (dev->name() == device.name()) {
                return true;
            }
            return false;
        });
}

/*
 * register_on_new_device()
 */
void Detector::register_on_new_device(Listener *listener)
{
    const std::lock_guard<std::mutex> guard(m_mutex);
    m_listeners.insert(listener);
    for (auto const &dev: m_devices) {
        listener->on_new_device(dev);
    }
}


/*
 * unregister_on_new_device()
 */
void Detector::unregister_on_new_device(Listener *listener)
{
    const std::lock_guard<std::mutex> guard(m_mutex);
    m_listeners.erase(listener);
}
