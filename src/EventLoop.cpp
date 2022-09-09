#include "EventLoop.hh"

#include <stdexcept>
#include <iostream>
#include <event2/thread.h>
#include <unistd.h>

struct ReadCBHelperStruct {
    void *arg;
    bool (*onRead)(int fd, void *arg);
    struct event *event;
};


struct WriteCBHelperStruct {
    void *arg;
    bool (*onWrite)(int fd, void *arg);
    struct event *event;
};

void read_callback(evutil_socket_t fd, short what, void *arg) {
    ReadCBHelperStruct *helper = static_cast<ReadCBHelperStruct *>(arg);
    if (what & (EV_READ | EV_TIMEOUT)) {
        if (helper->onRead(fd, helper->arg)) {
            struct timeval timeout;
            timeout.tv_sec = 1;
            timeout.tv_usec = 0;
            event_add(helper->event, &timeout);
        }
    } else {
        throw std::runtime_error("Unknown event in callback()\n");
    }
}


void write_callback(evutil_socket_t fd, short what, void *arg) {
    WriteCBHelperStruct *helper = static_cast<WriteCBHelperStruct *>(arg);
    if (what & (EV_WRITE | EV_TIMEOUT)) {
        if (helper->onWrite(fd, helper->arg)) {
            struct timeval timeout;
            timeout.tv_sec = 1;
            timeout.tv_usec = 0;
            event_add(helper->event, &timeout);
        }
    } else {
        throw std::runtime_error("Unknown event in callback()\n");
    }
}

/*
 * constructor()
 */
EventLoop::EventLoop() 
{
    evthread_use_pthreads();
    m_eb = event_base_new();
    if (!m_eb) {
        throw std::runtime_error("Could not create an libevent event base!\n");
    }
    
    m_worker = std::thread([this]() {
            event_base_loop(m_eb, EVLOOP_NO_EXIT_ON_EMPTY);
    });
}


/*
 * destructor()
 */
EventLoop::~EventLoop()
{
    for(auto &event: m_write_events) {
        ReadCBHelperStruct *helper;
        event_get_assignment(event.second, NULL, NULL, NULL, NULL, (void **)&helper);
        event_free(helper->event);
        delete helper;
        close(event.first);
    }
    for(auto &event: m_read_events) {
        ReadCBHelperStruct *helper;
        event_get_assignment(event.second, NULL, NULL, NULL, NULL, (void **)&helper);
        event_free(helper->event);
        delete helper;
        close(event.first);
    }
    if (event_base_loopexit(m_eb, NULL)) {
        std::cerr << "ERROR: event_base_loopexit() failed!\n";
    }
    if (m_worker.joinable()) {
        m_worker.join();
    }
    if (m_eb) {
        event_base_free(m_eb);
        m_eb = NULL;
    }
}


/*
 * register_read_cb()
 */
void EventLoop::register_read_cb(int fd, bool (*onRead)(int fd, void *arg), void *arg)
{
    ReadCBHelperStruct *helper;
    if (m_read_events.end() != m_read_events.find(fd)) {
        event_get_assignment(m_read_events[fd], NULL, NULL, NULL, NULL, (void **)&helper);
        event_free(helper->event);
        helper->event = m_read_events[fd] = event_new(m_eb, fd, EV_READ, read_callback, helper);
    } else {
        helper = new ReadCBHelperStruct();
        helper->event = m_read_events[fd] = event_new(m_eb, fd, EV_READ, read_callback, helper);
    }
    helper->arg = arg;
    helper->onRead = onRead;
    evutil_make_socket_nonblocking(fd);
    struct timeval timeout;
    timeout.tv_sec = 1;
    timeout.tv_usec = 0;
    event_add(helper->event, &timeout);
}


/*
 * register_write_cb()
 */
void EventLoop::register_write_cb(int fd, bool (*onWrite)(int fd, void *arg), void *arg)
{
    WriteCBHelperStruct *helper;
    if (m_write_events.end() != m_write_events.find(fd)) {
        event_get_assignment(m_write_events[fd], NULL, NULL, NULL, NULL, (void **)&helper);
        event_free(helper->event);
        helper->event = m_write_events[fd] = event_new(m_eb, fd, EV_WRITE, write_callback, helper);
    } else {
        helper = new WriteCBHelperStruct();
        helper->event = m_write_events[fd] = event_new(m_eb, fd, EV_WRITE, write_callback, helper);
    }
    helper->arg = arg;
    helper->onWrite = onWrite;
    evutil_make_socket_nonblocking(fd);
    struct timeval timeout;
    timeout.tv_sec = 1;
    timeout.tv_usec = 0;
    event_add(helper->event, &timeout);
}


/*
 * unregeister_read_cb()
 */
void EventLoop::unregister_read_cb(int fd)
{
    auto fd_entry = m_read_events.find(fd);
    if (fd_entry != m_read_events.end()) {
        m_read_events.erase(fd_entry);
    }
}


/*
 * unregeister_write_cb()
 */
void EventLoop::unregister_write_cb(int fd)
{
    auto fd_entry = m_write_events.find(fd);
    if (fd_entry != m_write_events.end()) {
        m_write_events.erase(fd_entry);
    }
}


/*
 * trigger_read_cb()
 */
void EventLoop::trigger_read_cb(int fd)
{
    auto event = m_read_events.find(fd);
    if (m_read_events.end() == event) {
        return;
    }

    event_active(event->second, EV_READ, 0);
}


/*
 * trigger_write_cb()
 */
void EventLoop::trigger_write_cb(int fd)
{
    auto event = m_write_events.find(fd);
    if (m_write_events.end() == event) {
        return;
    }

    event_active(event->second, EV_WRITE, 0);
}
