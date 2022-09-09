#ifndef __EVENTLOOP_HH__
#define __EVENTLOOP_HH__

#include <event2/event.h>
#include <thread>
#include <map>

class EventLoop {
    public:
        EventLoop(const EventLoop &) = delete;
        EventLoop(EventLoop &&) = delete;
        EventLoop &operator=(const EventLoop &) = delete;

        static EventLoop &get_realtime_event_loop()
        {
            // TODO: add parameter, which starts the thread as realtime thread
            static EventLoop el;
            return el;
        }

        static EventLoop &get_event_loop()
        {
            static EventLoop el;
            return el;
        }

        void register_read_cb(int fd, bool (*onRead)(int fd, void *arg), void *arg);
        void register_write_cb(int fd, bool (*onWrite)(int fd, void *arg), void *arg);
        void unregister_read_cb(int fd);
        void unregister_write_cb(int fd);

        void trigger_read_cb(int fd);
        void trigger_write_cb(int fd);

    private:
        EventLoop();
        ~EventLoop();

    private:
        struct event_base *m_eb;
        std::thread        m_worker;
        std::map<int, struct event*> m_read_events;
        std::map<int, struct event*> m_write_events;
};

#endif
