#include "DummyDetector.hh"
#include <iostream>

/*
 * DummyDetector()
 */
DummyDetector::DummyDetector()
{
    std::cout << "DummyDetector::" << __func__ << "()\n";
    std::shared_ptr<Device> dev = std::make_shared<DummyDevice>("StaticDummyDevice");
    dev->register_listener(this);
    m_devices.push_back(dev);
    dev = std::make_shared<DummyDevice>("StaticDummyDevice2");
    dev->register_listener(this);
    m_devices.push_back(dev);
}


/*
 * ~DummyDetector()
 */
DummyDetector::~DummyDetector()
{
    const std::lock_guard<std::mutex> guard(m_mutex);
    std::cout << "DummyDetector::" << __func__ << "()\n";
    for (auto &dev: m_devices) {
        dev->unregister_listener(this);
    }
}


/*
 * register_on_new_device()
 */
void DummyDetector::register_on_new_device(Listener *list)
{
    const std::lock_guard<std::mutex> guard(m_mutex);
    std::cout << "DummyDetector::" << __func__ << "()\n";
    m_listeners.insert(list);
    for (auto const &dev: m_devices) {
        list->on_new_dummy_device(dev);
    }
}


/*
 * on_state_change()
 */
void DummyDetector::on_state_change(Device &device, enum Device::State new_state)
{
    std::cout << "DummyDetector::" << __func__ << "()\n";
    if (device.is_valid()) {
        return;
    }
    const std::lock_guard<std::mutex> guard(m_mutex);
    device.unregister_listener(this);
    m_devices.remove_if([&device, this](const std::shared_ptr<Device> &dev) {
            if (dev->name() == device.name()) {
                return true;
            }
            return false;
        });
}
