#ifndef __INTERFACE_HH__
#define __INTERFACE_HH__

#include "Config.hh"
#include "MQTT.hh"
#include "devices/Detector.hh"
#include <mutex>

class Interface : public Detector::Listener, public Device::Listener, public MQTT::Listener {
    public:
        Interface() = delete;
        Interface(const Interface &) = delete;
        Interface &operator=(const Interface&) = delete;
        Interface(const Config &conf);

        ~Interface();

        virtual void on_new_device(const std::shared_ptr<Device> &dev) override;
        virtual void on_state_change(Device &device, enum Device::State new_state) override;
        virtual void on_message(const char *topic, const char *payload, size_t payload_len) override;
    
    private:
        std::mutex m_mutex;
        Config m_conf;
        MQTT m_mqtt;
        std::set<std::string> m_retain_topics;
};

#endif
