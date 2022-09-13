#ifndef __DETECTOR_HH__
#define __DETECTOR_HH__

#include "Device.hh"
#include "prusa/PrusaDetector.hh"
#include "dummy/DummyDetector.hh"
#include "../Config.hh"
#include <memory>
#include <list>
#include <functional>

/**
 * This class detects known devices
 */
class Detector : public PrusaDetector::Listener, public Device::Listener, public DummyDetector::Listener {
    public:
        class Listener {
            public:
                virtual void on_new_device(const std::shared_ptr<Device> &dev) = 0;
                virtual ~Listener() {};
        };

        /**
         * A detector is implemented as singelton and this method is used
         * to get this singleton.
         */
        static Detector &get(const Config &conf)
        {
            static Detector s(conf);
            return s;
        }

        Detector() = delete;
        Detector(const Detector &) = delete;
        Detector(Detector &&) = delete;
        Detector &operator=(const Detector &) = delete;
        virtual ~Detector();

        void for_each_device(std::function<void(const std::shared_ptr<Device> &)> fun)
        {
            const std::lock_guard<std::mutex> guard(m_mutex);
            for (const auto &dev: m_devices) {
                fun(dev);
            }
        }

        virtual void on_new_prusa_device(const std::shared_ptr<Device> &device) override;
        virtual void on_new_dummy_device(const std::shared_ptr<Device> &device) override;
        virtual void on_state_change(Device &device, enum Device::State new_state) override;

        void register_on_new_device(Listener *listener);
        void unregister_on_new_device(Listener *listener);

    private:
        Detector(const Config &conf);
        void detect();
        
    private:
        std::mutex m_mutex;
        std::list<std::shared_ptr<Device>> m_devices;
        const Config m_conf;
        std::set<Listener *> m_listeners;
};

#endif
