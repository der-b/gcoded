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
    PrusaDetector::get(conf).register_on_new_device(this);
    if (conf.load_dummy()) {
        DummyDetector::get().register_on_new_device(this);
    }
}


/*
 * ~Detector()
 */
Detector::~Detector()
{
    std::list<std::shared_ptr<Device>> local_devices;
    {
        const std::lock_guard<std::mutex> guard(m_mutex);
        for (const auto &dev: m_devices) {
            local_devices.push_back(dev);
        }
    }

    while (!local_devices.empty()) {
        local_devices.front()->shutdown();
        local_devices.pop_front();
    }
}


/*
 * on_new_prusa_device()
 */
void Detector::on_new_prusa_device(const std::shared_ptr<Device> &device) 
{
    const std::lock_guard<std::mutex> guard(m_mutex);
    std::cout << "New prusa device: " << device->name() << "\n";

    if (device->is_valid()) {
        device->register_listener(this);
        m_devices.push_back(device);
        for (auto &listener: m_listeners) {
            listener->on_new_device(device);
        }
        std::string name = device->name();
        device->m_on_listener_unregister = [this, name](size_t count) {
            if (count == 0) {
                std::cout << "Remove Prusa Device: " << name << "\n";
                const std::lock_guard<std::mutex> guard(m_mutex);
                m_devices.remove_if([this, name](const std::shared_ptr<Device> &dev) {
                    if (dev->name() == name) {
                        return true;
                    }
                    return false;
                });
            }
        };
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
 * on_state_change()
 */
void Detector::on_state_change(Device &device, enum Device::State new_state)
{
    if (device.is_valid()) {
        return;
    }
    const std::lock_guard<std::mutex> guard(m_mutex);
    device.unregister_listener(this);
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
