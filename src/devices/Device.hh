#ifndef __DEVICE_HH__
#define __DEVICE_HH__

#include <optional>
#include <string>
#include <functional>
#include <set>
#include <queue>
#include <stdexcept>
#include <mutex>
#include "../EventLoop.hh"

class Detector;

/**
 * This is an abstract representation of a device which accepts gcode for
 * controlling.
 */
class Device : public EventLoop::UserListener {
    public:
        /**
         * Device states.
         */
        enum class State {
            UNINITIALIZED = 0,
            // device is used by another program
            BUSY = 1,
            // device is ready to use
            OK = 2,
            // Device is printing
            PRINTING = 3,
            // Some error occurred
            ERROR = 4,
            // The device is vanished. (i.e. The USB-cable was unplugged or the device was turned off)
            DISCONNECTED = 5,
            // Connected to device and waiting for device boot.
            INIT_DEVICE = 6,
            // Device is disabled and removed by calling Device::shutdown().
            SHUTDOWN = 7,
            __LAST_ENTRY
        };

        /**
         * Possible results from printing attempt.
         */
        enum class PrintResult {
            // this is not intended to be returned by any function
            INVALID = 0,
            // successful execution
            OK = 1,
            // the device state does not allow to accept print jobs (see: is_valid())
            ERR_INVALID_STATE = 2,
            // the device is already printing, therefore new print jobs can't be accepted
            ERR_PRINTING = 3,
            // the device does not exist (this not returned by devices, but is used to communicate to gcoded clients)
            NET_ERR_NO_DEVICE = 4,
            // network request response timed out
            NET_ERR_TIMEOUT = 5,
            __LAST_ENTRY
        };

        /**
         * Listener which can be registered to get status and print process updates.
         */
        class Listener {
            public:
                /**
                 * Will be called, every time the device state changes.
                 * It is guaranteed that this method is called for each state change and in the correct order.
                 * It is not guaranteed that the method is called timely. That means that the device might be already
                 * in another state while calling. This is due to the fact, that the device might run on a realtime
                 * thread but we don't want to run the listeners on a realtime thread.
                 *
                 * @param device Handle to the device which is calling this method.
                 * @param new_state The new state of the device.
                 */
                virtual void on_state_change(Device &device, enum State new_state) = 0;

                /**
                 * Is called while printing and informs the callee about the current printing state.
                 *
                 * How it works:
                 * If the device is printing, this method is called when the gcoded receives a response on the
                 * G-code M73 from the printer.
                 * See: https://www.reprap.org/wiki/G-code#M73:_Set.2FGet_build_percentage
                 *
                 * @param device Handle to the device which is calling this method.
                 * @param percentage of the current print
                 * @patam remaining_time Estimated remaining time in minutes until print is finished.
                 */
                virtual void on_build_progress_change(Device &device, unsigned percentage, unsigned remaining_time)
                {}

                virtual ~Listener() {};
        };

        /**
         * Represents a sensor value of a device.
         */
        struct SensorValue {
            // The current value from the sensor
            double current_value;
            // The set point, if this value is currently controlled by the device
            std::optional<double> set_point;
        };

        /**
         * Converts a device state into a printable string.
         */
        static const std::string &state_to_str(enum State state);

        /**
         * Converts a printing result into a printable string.
         */
        static inline const std::string &printres_to_str(PrintResult res);

        /**
         * Returns the current state of the device
         */
        State state() const { return m_state; }

        /**
         * Returns true, if the device is an operational state.
         * That means, interaction with the device is possible.
         */
        inline bool is_valid() const;

        /**
         * Allows to turn of the software control of the device.
         * After calling this, the device will unregister the device and it will not
         * be available until the device is physically reconnected.
         */
        void shutdown() { set_state(State::SHUTDOWN); }

        /**
         * Returns an unique name of the device.
         * This name has to be unique for a provider.
         * Furthermore, the name has to be permanent even after reboots.
         * It is a good praxis to base this name on a serial number.
         * See the section "Topic Collision Avoidance" in the README.md
         */
        virtual const std::string &name() const = 0;

        /**
         * Loads the G-Code file and send it to the printer.
         */
        virtual PrintResult print_file(const std::string &file_path) = 0;

        /**
         * Interprets the string as G-Code and send it to the printer.
         */
        virtual PrintResult print(const std::string &gcode) = 0;

        /**
         * Registers a listener. On registration Listener::on_state_change() will
         * be called to inform the listener on the actual state.
         */
        void register_listener(Listener *list);

        /**
         * Unregisters a listener. If the given listener is not registered, than the call does nothing.
         * Note, that the unregistering might not take place immediately. Another thread might currently
         * call the listener. The unregistering happens, when it is made sure, that the Listener is not
         * in use.
         */
        void unregister_listener(Listener *list);

        /**
         * Destructor()
         */
        inline virtual ~Device();

        /**
         * This method checks, whether state changes happened or a printing process was changes.
         * If so, than all listeners are called.
         *
         * You don't have to call this method. It is made sure that it is called internally.
         * But it is safe to call it.
         */
        bool onTrigger(void) override;

    protected:
        Device()
            : m_state(State::UNINITIALIZED)
        {
            m_user_event = EventLoop::get_event_loop().create_user_event(this);
        }

        /**
         * Changes the device state and triggers all listeners in a thread safe manner.
         */
        virtual void set_state(enum State new_state);

        /**
         * Triggers all listeners in a thread safe manner about a printing progress update.
         */
        void update_progress(unsigned percentage, unsigned remaining_time);

    private:
        /**
         * Tries to clean up listeners. If the needed mutex is locked, than no listener is clean up.
         * Using this in Device::unregister_listener() allows to unregister listeners from inside of a
         * listener method while being thread safe and without deadlocks.
         */
        void try_clean_up_listeners();

        /**
         * this function unregisters all listeners which are in the unregister queue.
         * WARNING: This function assumes, that Device::m_mutex is already locked when called.
         */
        void clean_up_listeners();

        std::mutex m_mutex;
        State m_state;
        std::set<Listener *> m_listeners;
        std::function<void(size_t)> m_on_listener_unregister;

        std::mutex m_unregister_mutex;
        std::queue<Listener *> m_unregister_listeners;
        std::shared_ptr<EventLoop::UserEvent> m_user_event;
        std::list<enum State> m_state_list;
        std::optional<std::pair<unsigned, unsigned>> m_progress_update;

        friend Detector;
};


/*
 * state_to_str()
 */
inline const std::string &Device::state_to_str(enum Device::State state)
{
    static const std::string states[] = { "UNINITIALIZED",
                                          "BUSY",
                                          "OK",
                                          "PRINTING",
                                          "ERROR",
                                          "DISCONNECTED",
                                          "INIT_DEVICE",
                                          "SHUTDOWN",
                                          "<UNKNOWN_STATE>" };
    if (state > State::__LAST_ENTRY) {
        state = State::__LAST_ENTRY;
    }
    return states[static_cast<size_t>(state)];
}

/*
 * printres_to_str()
 */
inline const std::string &Device::printres_to_str(Device::PrintResult res)
{
    static const std::string results[] = { "INVALID",
                                           "OK",
                                           "ERR_INVALID_STATE",
                                           "ERR_PRINTING",
                                           "NET_ERR_NO_DEVICE",
                                           "NET_ERR_TIMEOUT",
                                           "<UNKNOWN_STATE>" };
    if (res > PrintResult::__LAST_ENTRY) {
        res = PrintResult::__LAST_ENTRY;
    }
    return results[static_cast<size_t>(res)];
}

/*
 * is_valid()
 */
inline bool Device::is_valid() const {
    return    m_state != State::UNINITIALIZED
           && m_state != State::ERROR
           && m_state != State::DISCONNECTED
           && m_state != State::SHUTDOWN;
}


/*
 * ~Device()
 */
inline Device::~Device()
{
    if (m_user_event) {
        m_user_event->disable();
    }
    m_user_event = nullptr;
}
#endif
