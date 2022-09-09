#ifndef __DUMMY_DETECTOR_HH__
#define __DUMMY_DETECTOR_HH__

#include "DummyDevice.hh"
#include <mutex>
#include <memory>
#include <list>

class DummyDetector : public Device::Listener {
    public:
        class Listener {
            public:
                virtual void on_new_dummy_device(const std::shared_ptr<Device> &device) = 0;
        };

        /**
         * A detector is implemented as singelton and this method is used
         * to get this singleton.
         */
        static DummyDetector &get()
        {
            static DummyDetector s;
            return s;
        }

        DummyDetector(const DummyDetector &) = delete;
        DummyDetector(DummyDetector &&) = delete;
        DummyDetector &operator=(const DummyDetector &) = delete;
        ~DummyDetector();

        /**
         * Listener will be called for all existing devices and all devices which will be detected in 
         * the future.
         * 
         * Each Listener can only be registered once. If listener was already registered,
         * this call has no effect.
         */
        void register_on_new_device(Listener *list);
        virtual void on_state_change(Device &device, enum Device::State new_state) override;

    private:
        DummyDetector();

        std::mutex m_mutex;
        std::set<Listener *> m_listeners;
        std::list<std::shared_ptr<Device>> m_devices;
};

#endif
