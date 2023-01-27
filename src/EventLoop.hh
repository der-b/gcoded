#ifndef __EVENTLOOP_HH__
#define __EVENTLOOP_HH__

#include <event2/event.h>
#include <thread>
#include <map>
#include <list>
#include <memory>
#include <mutex>
#include <cstring>

class EventLoop {
    public:
        class UserEvent {
            public:
                UserEvent() = delete;
                UserEvent(const UserEvent &) = delete;
                UserEvent(UserEvent &&) = delete;
                UserEvent &operator=(const EventLoop &) = delete;

                UserEvent(struct event* ev, EventLoop *el)
                {
                    m_ev = ev;
                    m_el = el;
                }

                virtual ~UserEvent()
                {
                    disable();
                }

                void trigger()
                {
                    const std::lock_guard<std::mutex> guard(m_mutex);
                    if (m_ev) {
                        event_active(m_ev, 0, 0);
                    }
                }

                void disable()
                {
                    struct event* ev = NULL;
                    EventLoop *el = NULL;
                    {
                        const std::lock_guard<std::mutex> guard(m_mutex);
                        if (m_el)
                        {
                            ev = m_ev;
                            el = m_el;
                            m_ev = NULL;
                            m_el = NULL;
                        }
                        if (ev) {
                            el->unregister_user_event(ev);
                        }
                    }

                }
            private:
                std::mutex m_mutex;
                struct event *m_ev;
                struct EventLoop *m_el;
        };

        class UserListener {
            public:
                virtual bool onTrigger(void) = 0;
                virtual ~UserListener() {};
        };

    public:
        EventLoop() = delete;
        EventLoop(const EventLoop &) = delete;
        EventLoop(EventLoop &&) = delete;
        EventLoop &operator=(const EventLoop &) = delete;

        /**
         * Returns an instance of the event loop whit an thread, which is executed by
         * a realtime scheduler. If the parameter realtime is set to false, the function
         * returns an event loop without a realtiem thread.
         */
        static EventLoop &get_realtime_event_loop(bool realtime = true)
        {
            if (!realtime) {
                return get_event_loop();
            }
            static EventLoop el(true);
            return el;
        }

        static EventLoop &get_event_loop()
        {
            static EventLoop el(false);
            return el;
        }

        void register_read_cb(int fd, bool (*onRead)(int fd, void *arg), void *arg);
        void register_write_cb(int fd, bool (*onWrite)(int fd, void *arg), void *arg);
        void unregister_read_cb(int fd);
        void unregister_write_cb(int fd);

        std::shared_ptr<UserEvent> create_user_event(UserListener *listener);

        void trigger_read_cb(int fd);
        void trigger_write_cb(int fd);

    private:
        EventLoop(bool realtime = false);
        ~EventLoop();

        void unregister_user_event(struct event *ev);
        friend UserEvent;

    private:
        std::recursive_mutex m_mutex;
        struct event_base *m_eb;
        std::thread        m_worker;
        std::map<int, struct event*> m_read_events;
        std::map<int, struct event*> m_write_events;
        std::list<struct event*> m_user_events;
};

#endif
