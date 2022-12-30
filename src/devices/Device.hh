#ifndef __DEVICE_HH__
#define __DEVICE_HH__

#include <optional>
#include <string>
#include <functional>
#include <set>
#include <queue>
#include <stdexcept>
#include <mutex>
#include <thread>
#include "../EventLoop.hh"
#include <iostream>

class Detector;

/**
 * This is an abstract representation of a device which accepts gcode for 
 * controlling.
 */
class Device : public EventLoop::UserListener {
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
            // Device is disabled and removed by calling Device::shutdown().
            SHUTDOWN = 7,
            __LAST_ENTRY
        };

        static const std::string &state_to_str(enum State state)
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
                 * If the device is printing, this method is called when the gcoded receives a respnse on the
                 * G-code M73 from the pritner.
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
            return    m_state != State::ERROR
                   && m_state != State::DISCONNECTED
                   && m_state != State::SHUTDOWN;
        }

        void shutdown() {
            set_state(State::SHUTDOWN);
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
            {
                const std::lock_guard<std::mutex> guard(m_mutex);
                m_listeners.insert(list);
            }
            try_clean_up_listeners();
        }

        void unregister_listener(Listener *list)
        {
            {
                const std::lock_guard<std::mutex> guard(m_unregister_mutex);
                m_unregister_listeners.push(list);
            }
            try_clean_up_listeners();
        }

        virtual ~Device()
        {
            if (m_user_event) {
                m_user_event->disable();
            }
            m_user_event = nullptr;
        }

        virtual bool onTrigger(void) override
        {
            const std::lock_guard<std::mutex> guard(m_mutex);

            if (!m_state_list.empty()) {
                enum State new_state = m_state_list.front();
                m_state_list.pop_front();
                for (auto const &list: m_listeners) {
                    list->on_state_change(*this, new_state);
                }
            } else if (m_progress_update) {
                for (auto const &list: m_listeners) {
                    list->on_build_progress_change(*this, m_progress_update->first, m_progress_update->second);
                }
                m_progress_update.reset();
            }

            if (!m_state_list.empty() || m_progress_update) {
                if (m_user_event) {
                    m_user_event->trigger();
                }
            } else if (State::SHUTDOWN == m_state) {
                if (m_user_event) {
                    m_user_event->disable();
                }
                m_user_event = nullptr;
            }
            clean_up_listeners();

            return true;
        }

    protected:
        Device()
            : m_state(State::UNINITIALIZED)
        {
            m_user_event = EventLoop::get_event_loop().create_user_event(this);
        }

        virtual void set_state(enum State new_state)
        {
            const std::lock_guard<std::mutex> guard(m_mutex);
            m_state = new_state;
            m_state_list.push_back(new_state);
            // We do not call directly the listeners for state changes, since the device which calls
            // this function might run on a realtime thread and we don't want to execute the listners
            // on a real time thread. Therefore we inform a normal thread that there was a state change
            // which calls the listeners.
            if (m_user_event) {
                m_user_event->trigger();
            }
        }

        void update_progress(unsigned percentage, unsigned remaining_time) {
            const std::lock_guard<std::mutex> guard(m_mutex);
            m_progress_update = std::pair<unsigned, unsigned>(percentage, remaining_time);
            // We do not call directly the listeners for progress updates, since the device which calls
            // this function might run on a realtime thread and we don't want to execute the listners
            // on a real time thread. Therefore we inform a normal thread that there was a progress update
            // which calls the listeners.
            if (m_user_event) {
                m_user_event->trigger();
            }
        }

    private:
        void try_clean_up_listeners() {
            if (m_mutex.try_lock()) {
                try {
                    clean_up_listeners();
                } catch(...) {
                    m_mutex.unlock();
                    std::exception_ptr eptr = std::current_exception(); // capture
                    try {
                        std::rethrow_exception(eptr);
                    } catch(const std::exception& e) {
                        std::string error = "Caught exception: \"";
                        error += e.what();
                        error += "\"\n";
                        throw std::runtime_error(error);
                    }
                }
                m_mutex.unlock();
            }
        }

        // WARNING: This fuction assumes, that m_mutex is already locked
        void clean_up_listeners() {
            const std::lock_guard<std::mutex> guard(m_unregister_mutex);
            while (!m_unregister_listeners.empty()) {
                m_listeners.erase(m_unregister_listeners.front());
                m_unregister_listeners.pop();
            }
            if (m_on_listener_unregister) {
                m_on_listener_unregister(m_listeners.size());
            }
        }

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

#endif
