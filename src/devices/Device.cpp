#include "Device.hh"


/*
 * register_listener()
 */
void Device::register_listener(Device::Listener *list)
{
    {
        const std::lock_guard<std::mutex> guard(m_mutex);
        m_listeners.insert(list);
    }
    try_clean_up_listeners();
}


/*
 * unregister_listener()
 */
void Device::unregister_listener(Device::Listener *list)
{
    {
        const std::lock_guard<std::mutex> guard(m_unregister_mutex);
        m_unregister_listeners.push(list);
    }
    try_clean_up_listeners();
}


/*
 * onTrigger()
 */
bool Device::onTrigger(void)
{
    const std::lock_guard<std::mutex> guard(m_mutex);
    bool ret = true;

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
    } else if (m_sensor_reading_update) {
        for (auto const &list: m_listeners) {
            list->on_sensor_update(*this);
        }
        m_sensor_reading_update = false;
    }

    if (!m_state_list.empty() || m_progress_update) {
        if (m_user_event) {
            m_user_event->trigger();
        }
    } else if (!is_valid()) {
        if (m_user_event) {
            m_user_event->disable();
        }
        m_user_event = nullptr;
        ret = false;
    }
    clean_up_listeners();

    return ret;
}


/*
 * set_state()
 */
void Device::set_state(enum Device::State new_state)
{
    const std::lock_guard<std::mutex> guard(m_mutex);
    if (new_state == m_state){
        return;
    }

    if (m_state == State::SHUTDOWN) {
        std::string err = "Invalid state change on device '";
        err += name();
        err += "': You are not allowed to change the state of a device which is in SHUTDOWN.";
        throw std::runtime_error(err);
    }

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


/*
 * update_progress()
 */
void Device::update_progress(unsigned percentage, unsigned remaining_time)
{
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


/*
 * update_sensor_readings()
 */
void Device::update_sensor_readings()
{
    const std::lock_guard<std::mutex> guard(m_mutex);
    m_sensor_reading_update = true;
    // We do not call directly the listeners, since the device which calls
    // this function might run on a realtime thread and we don't want to execute the listners
    // on a real time thread. Therefore we inform a normal thread that there was a progress update
    // which calls the listeners.
    if (m_user_event) {
        m_user_event->trigger();
    }
}


/*
 * try_clean_up_listeners()
 */
void Device::try_clean_up_listeners()
{
    if (m_mutex.try_lock()) {
        try {
            clean_up_listeners();
        } catch(...) {
            m_mutex.unlock();
            std::exception_ptr eptr = std::current_exception();
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


/*
 * clean_up_listeners()
 */
void Device::clean_up_listeners()
{
    const std::lock_guard<std::mutex> guard(m_unregister_mutex);
    while (!m_unregister_listeners.empty()) {
        m_listeners.erase(m_unregister_listeners.front());
        m_unregister_listeners.pop();
    }
    if (m_on_listener_unregister) {
        m_on_listener_unregister(m_listeners.size());
    }
}
