#ifndef __DEVICE_HH__
#define __DEVICE_HH__

#include <optional>
#include <string>
#include <functional>
#include <set>
#include <stdexcept>
#include <mutex>
#include <thread>

/**
 * This is an abstract representation of a device which accepts gcode for 
 * controlling.
 */
class Device {
    public:
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
            __LAST_ENTRY
        };

        static const std::string &state_to_str(enum State state)
        {
            static const std::string states[] = { "UNINITIALIZED", "BUSY", "OK", "PRINTING", "ERROR", "DISCONNECTED", "INIT_DEVICE", "<UNKNOWN_STATE>" };
            if (state > State::__LAST_ENTRY) {
                state = State::__LAST_ENTRY;
            }
            return states[static_cast<size_t>(state)];
        }

        enum class PrintResult {
            // this is not intended to be returned by any funktion
            INVALID = 0,
            // successfull execution
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

        static const std::string &printres_to_str(PrintResult res)
        {
            static const std::string results[] = { "INVALID", "OK", "ERR_INVLID_STATE", "ERR_PRINTING", "NET_ERR_NO_DEVICE", "NET_ERR_TIMEOUT", "<UNKNOWN_STATE>" };
            if (res > PrintResult::__LAST_ENTRY) {
                res = PrintResult::__LAST_ENTRY;
            }
            return results[static_cast<size_t>(res)];
        }

        class Listener {
            public:
                virtual void on_state_change(Device &device, enum State new_state) = 0;

                /**
                 * If the device is printing, this method is called when gcode M73 is transmitted to the pritner.
                 * See: https://www.reprap.org/wiki/G-code#M73:_Set.2FGet_build_percentage
                 *
                 * The unit of remaining_time is minutes.
                 */
                virtual void on_build_progress_change(Device &device, unsigned percentage, unsigned remaining_time)
                {}

                virtual ~Listener() {};
        };

        struct value {
            double actual_value;
            std::optional<double> set_point;
        };

        State state() const {
            return m_state;
        }

        bool is_valid() const {
            return m_state != State::ERROR && m_state != State::DISCONNECTED;
        }

        /*
         * Returns an unique name of the device.
         */
        virtual const std::string &name() const = 0;

        /*
         * loads the gcode-file and send it to the printer.
         */
        virtual PrintResult print_file(const std::string &file_path) = 0;

        /*
         * interpretes the string as gcode and send it to the printer.
         */
        virtual PrintResult print(const std::string &gcode) = 0;

        void register_listener(Listener *list)
        {
            const std::lock_guard<std::recursive_mutex> guard(m_mutex);
            m_listeners.insert(list);
        }

        void unregister_listener(Listener *list)
        {
            const std::lock_guard<std::recursive_mutex> guard(m_mutex);
            m_listeners.erase(list);
        }

        virtual ~Device() {};

    protected:
        Device()
            : m_state(State::UNINITIALIZED)
        {};

        void set_state(enum State new_state)
        {
            const std::lock_guard<std::recursive_mutex> guard(m_mutex);
            m_state = new_state;
            for (auto const &list: m_listeners) {
                // TODO: If the device runs on realtime thread, than the callbacks also runs on the same thread
                // TODO: Uncouple this, so that the callbacks runs on a normal thread
                list->on_state_change(*this, m_state);
            }
        }

        void update_progress(unsigned percentage, unsigned remaining_time) {
            const std::lock_guard<std::recursive_mutex> guard(m_mutex);
            for (auto const &list: m_listeners) {
                // TODO: If the device runs on realtime thread, than the callbacks also runs on the same thread
                // TODO: Uncouple this, so that the callbacks runs on a normal thread
                list->on_build_progress_change(*this, percentage, remaining_time);
            }
        }
    private:
        std::recursive_mutex m_mutex;
        State m_state;
        std::set<Listener *> m_listeners;
};

#endif
