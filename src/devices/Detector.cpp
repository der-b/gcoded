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
    shutdown();
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
                std::shared_ptr<Device> dev_backup;
                m_devices.remove_if([this, name, &dev_backup](const std::shared_ptr<Device> &dev) {
                    if (dev->name() == name) {
                        // we want to call de destructor right now. We need to call it at the end of the
                        // m_on_listener_unregister, therefore we need to keep a backup
                        dev_backup = dev;
                        return true;
                    }
                    return false;
                });
                if (m_devices.empty()) {
                    m_shutdown_done = true;
                    m_shutdown_cv.notify_all();
                }
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

    if (device->is_valid()) {
        device->register_listener(this);
        m_devices.push_back(device);
        for (auto &listener: m_listeners) {
            listener->on_new_device(device);
        }
        std::string name = device->name();
        device->m_on_listener_unregister = [this, name](size_t count) {
            if (count == 0) {
                std::cout << "Remove Dummy Device: " << name << "\n";
                const std::lock_guard<std::mutex> guard(m_mutex);
                std::shared_ptr<Device> dev_backup;
                m_devices.remove_if([this, name, &dev_backup](const std::shared_ptr<Device> &dev) {
                    if (dev->name() == name) {
                        // we want to call de destructor right now. We need to call it at the end of the
                        // m_on_listener_unregister, therefore we need to keep a backup
                        dev_backup = dev;
                        return true;
                    }
                    return false;
                });
                if (m_devices.empty()) {
                    m_shutdown_done = true;
                    m_shutdown_cv.notify_all();
                }
            }
        };
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


/*
 * shutdown()
 */
void Detector::shutdown()
{
    std::list<std::shared_ptr<Device>> local_devices;
    {
        const std::lock_guard<std::mutex> guard(m_mutex);
        PrusaDetector::get(m_conf).unregister_on_new_device(this);
        if (m_conf.load_dummy()) {
            DummyDetector::get().unregister_on_new_device(this);
        }
        for (const auto &dev: m_devices) {
            local_devices.push_back(dev);
        }
        if (!local_devices.empty()) {
            m_shutdown_done = false;
        }
    }

    while (!local_devices.empty()) {
        local_devices.front()->shutdown();
        local_devices.pop_front();
    }

    // block until all devices are successfull unlocked
    std::unique_lock ul(m_mutex);
    m_shutdown_cv.wait(ul, [this]{return m_shutdown_done;});
}
