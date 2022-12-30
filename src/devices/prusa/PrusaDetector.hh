#ifndef __PRUSA_DETECTOR_HH__
#define __PRUSA_DETECTOR_HH__

#include "PrusaDevice.hh"
#include "../Device.hh"
#include "../../Inotify.hh"
#include "../../Config.hh"
#include <mutex>
#include <set>
#include <list>
#include <memory>

class PrusaDetector : public Inotify::Listener, public Device::Listener {
    public:
        static constexpr unsigned int USB_VENDOR_ID = 0x2c99;
        static constexpr unsigned int USB_PRODUCT_ID = 0x2;

        class Listener {
            public:
                virtual void on_new_prusa_device(const std::shared_ptr<Device> &device) = 0;
                virtual ~Listener() {};
        };

        /**
         * A detector is implemented as singelton and this method is used
         * to get this singleton.
         */
        static PrusaDetector &get(const Config &conf)
        {
            static PrusaDetector s(conf);
            return s;
        }

        PrusaDetector(const PrusaDetector &) = delete;
        PrusaDetector(PrusaDetector &&) = delete;
        PrusaDetector &operator=(const PrusaDetector &) = delete;
        virtual ~PrusaDetector();

        /**
         * Listener will be called for all existing devices and all devices which will be detected in 
         * the future.
         * 
         * Each Listener can only be registered once. If listener was already registered,
         * this call has no effect.
         */
        void register_on_new_device(Listener *list);
        void unregister_on_new_device(Listener *list);

        virtual void on_fs_event(const std::string &path, uint32_t event_type, const std::optional<std::string> &name) override;
        virtual void on_state_change(Device &device, enum Device::State new_state) override;

    private:
        PrusaDetector(const Config &conf);

        /**
         * Adds detected devices to the device list, if they are not in the list already.
         * Returns true if a new device was added to the list.
         */
        void detect(std::list<std::shared_ptr<Device>> &devices);
        void check_candidate(const std::string &filename);

        std::mutex m_mutex;
        std::set<Listener *> m_listeners;
        std::list<std::shared_ptr<Device>> m_devices;
        const Config &m_conf;
};

#endif
