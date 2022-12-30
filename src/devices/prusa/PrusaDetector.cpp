#include "PrusaDetector.hh"
#include <iostream>
#include <filesystem>
#include <fstream>
#include <limits.h>
#include <stdlib.h>
#include <cstring>
#include <fcntl.h>
#include <unistd.h>

/*
 * PrusaDetector()
 */
PrusaDetector::PrusaDetector(const Config &conf)
    : m_conf(conf)
{
    Inotify::get().register_listener("/dev/", Inotify::CREATE | Inotify::ATTRIB, this);
    detect(m_devices);
}


/*
 * PrusaDetector()
 */
PrusaDetector::~PrusaDetector()
{
    std::lock_guard<std::mutex> guard(m_mutex);
    for (auto &dev: m_devices) {
        dev->unregister_listener(this);
    }
}


/*
 * detect()
 */
void PrusaDetector::detect(std::list<std::shared_ptr<Device>> &devices)
{
    using std::filesystem::directory_iterator;

    std::lock_guard<std::mutex> guard(m_mutex);
    // iterate over all ttys
    for (const auto &dir: directory_iterator("/sys/class/tty/")) {
        std::filesystem::path candidate = dir;
        check_candidate(candidate.filename());
    }
}


/*
 * check_candidate()
 */
void PrusaDetector::check_candidate(const std::string &filename)
{
    std::filesystem::path candidate = "/sys/class/tty/" + filename;
    candidate+="/../../uevent";
    // resolve symbolic links, to get the real device
    char *real = realpath(candidate.c_str(), NULL);
    if (real) {
        // if the path doesn't exist, than it is not an USB device,
        // but we are looking for serial over USB.
        if (!std::filesystem::exists(real)) {
            free(real);
            real = NULL;
            return;
        }
        // parse the uevent file to figure out the USB vendor id and product id
        std::ifstream uevent(real);
        free(real);
        real = NULL;
        std::string line;
        std::string product;
        bool correct_devtype = false;
        bool correct_driver = false;
        while (!uevent.eof()) {
            std::getline(uevent, line);
            if ("DEVTYPE=usb_interface" == line) {
                correct_devtype = true;
                continue;
            }
            if ("DRIVER=cdc_acm" == line) {
                correct_driver = true;
                continue;
            }
            if (0 == line.compare(0, std::strlen("PRODUCT"), "PRODUCT")) {
                product = line.substr(std::strlen("PRODUCT="));
            }
        }
        if (!correct_devtype || !correct_driver) {
            return;
        }
        size_t seperator1 = product.find("/");
        size_t seperator2 = product.find("/", seperator1+1);
        int vendor_id = std::stoul(product.substr(0, seperator1), NULL, 16);
        int product_id = std::stoul(product.substr(seperator1+1, seperator2 - seperator1 - 1), NULL, 16);
        
        if (USB_VENDOR_ID != vendor_id) {
            return;
        }
        if (USB_PRODUCT_ID != product_id) {
            return;
        }

        std::filesystem::path serial_file = "/sys/class/tty/" + filename;
        serial_file+="/../../../serial";
        char *real_serial = realpath(serial_file.c_str(), NULL);
        if (!real_serial) {
            std::cerr << "no serial file for '" << filename << "' found\n";
            return;
        }
        if (!std::filesystem::exists(real_serial)) {
            free(real_serial);
            real_serial = NULL;
            std::cerr << "no serial file for '" << filename << "' found\n";
            return;
        }
        std::ifstream serial(real_serial);
        free(real_serial);
        real_serial = NULL;
        std::getline(serial, line);
        if (0 == line.size()) {
            std::cerr << "No serial number found for PrusaDevice\n";
            return;
        }

        const std::string device_name = "prusa-" + line;
        const std::string device_file = "/dev/" + filename;

        auto dev = std::find_if(m_devices.begin(), m_devices.end(), [&device_name](const std::shared_ptr<Device> &d) { return device_name == d->name(); });
        if (dev == m_devices.end()) {
            int fd = open(device_name.c_str(), O_RDWR | O_SYNC | O_NOCTTY | O_NONBLOCK);
            if (fd < 0 && errno == EACCES) {
                close(fd);
                return;
            }
            close(fd);

            std::shared_ptr<Device> new_dev = std::make_shared<PrusaDevice>(device_file, device_name, m_conf);

            new_dev->register_listener(this);

            if (new_dev->is_valid()) {
                m_devices.push_back(new_dev);
                for (auto &listener: m_listeners) {
                    listener->on_new_prusa_device(new_dev);
                }
            }

            return;
        }
    }
}


/*
 * register_on_new_device()
 */
void PrusaDetector::register_on_new_device(Listener *list)
{
    const std::lock_guard<std::mutex> guard(m_mutex);
    m_listeners.insert(list);
    for (const auto &dev: m_devices) {
        list->on_new_prusa_device(dev);
    }
}


/*
 * unregister_on_new_device()
 */
void PrusaDetector::unregister_on_new_device(Listener *list)
{
    const std::lock_guard<std::mutex> guard(m_mutex);
    m_listeners.erase(list);
}


/*
 * on_fs_event()
 */
void PrusaDetector::on_fs_event(const std::string &path, uint32_t event_type, const std::optional<std::string> &name)
{
    if (   !(Inotify::CREATE & event_type)
        && !(Inotify::ATTRIB & event_type)) {
        return;
    }
    if (Inotify::IS_DIR & event_type) {
        return;
    }

    if (!name) {
        return;
    }

    const std::lock_guard<std::mutex> guard(m_mutex);
    check_candidate(*name);
}


/*
 * on_state_change()
 */
void PrusaDetector::on_state_change(Device &device, enum Device::State new_state)
{
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
