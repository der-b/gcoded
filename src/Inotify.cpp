#include "Inotify.hh"
#include <algorithm>
#include <iostream>
#include <unistd.h>
#include <sys/inotify.h>
#include <limits.h>
#include <string.h>

bool on_read(int fd, void *arg) 
{
    Inotify::callback_data *cb_data = (Inotify::callback_data *)arg;
    Inotify::callback_data &m_cb_data = *cb_data;

    char buf[sizeof(struct inotify_event) + NAME_MAX + 1];
    const struct inotify_event *event;
    ssize_t len;
    
    const std::lock_guard<std::recursive_mutex> guard(m_cb_data.mutex);

    while (0 < (len = read(fd, buf, sizeof(buf)))) {
        char *ptr = buf;
        event = (const struct inotify_event *)ptr;
        for (; ptr < buf + len
             ; ptr += sizeof(struct inotify_event) + event->len,
               event = (const struct inotify_event *)ptr) {

            /*
            if (event->mask & IN_CREATE) {
                std::cout << "CREATE: ";
            } else if (event->mask & IN_DELETE) {
                std::cout << "DELETE: ";
            } else {
                std::cout << "UNKNOWN: ";
            }

            if (event->len > 0) {
                std::cout << event->name;
            }

            std::cout << "\n";
            */
            //std::cout << "miau!" << event->wd << "\n";
            if (event->mask & IN_IGNORED) {
                m_cb_data.listeners.erase(event->wd);
                continue;
            }

            std::string path = m_cb_data.listeners.at(event->wd).path;
            for (const auto &listener: m_cb_data.listeners.at(event->wd).listeners) {
                std::optional<std::string> name;
                if (event->len) {
                    name = event->name;
                }
                listener->on_fs_event(path, event->mask, name);
            }

        }
    }

    if (0 == len) {
        // TODO: Check what to do in this case
        std::cerr << "Inotify::on_read(): reached EOF with read().";
        return false;
    }

    if (-1 == len && errno != EAGAIN) {
        throw std::runtime_error("Inotify::on_read(): read() failed.");
    }

    return true;
}


/*
 * Inotify()
 */
Inotify::Inotify()
    : m_ev(EventLoop::get_event_loop())
{
    m_in_fd = inotify_init1(IN_NONBLOCK);
    if (-1 == m_in_fd) {
        throw std::runtime_error("inotify_init1() faild!");
    }

    m_ev.register_read_cb(m_in_fd, on_read , &m_cb_data);
}


/*
 * ~Inotify()
 */
Inotify::~Inotify()
{
    close(m_in_fd);
}


/*
 * register_listener()
 */
void Inotify::register_listener(const std::string &path, uint32_t event_type, Listener *list)
{
    const std::lock_guard<std::recursive_mutex> guard(m_cb_data.mutex);
    int wd = inotify_add_watch(m_in_fd, path.c_str(), event_type | IN_MASK_ADD);
    if (-1 == wd) {
        std::string err = "inotify_add_watch() failed: ";
        err += strerror(errno);
        throw std::runtime_error(err);
    }
    auto it = m_cb_data.listeners.emplace(std::piecewise_construct,
                                          std::forward_as_tuple(wd),
                                          std::forward_as_tuple(event_type, path));
    it.first->second.listeners.insert(list);
}


/*
 * unregister_listener()
 */
void Inotify::unregister_listener(const std::optional<std::string> path, Listener *list)
{
    const std::lock_guard<std::recursive_mutex> guard(m_cb_data.mutex);
    if (path) {
        auto it = std::find_if(m_cb_data.listeners.begin(),
                               m_cb_data.listeners.end(),
                               [&path](const std::pair<int, struct listener_data> &list_data) {
                                   return list_data.second.path == *path;
                               });
        if (m_cb_data.listeners.end() != it) {
            it->second.listeners.erase(list);
            if (0 == it->second.listeners.size()) {
                if (inotify_rm_watch(m_in_fd, it->first)) {
                    throw std::runtime_error("inotify_rm_watch() failed.");
                }
            }
        }
    } else {
        auto it = m_cb_data.listeners.begin();
        while (it != m_cb_data.listeners.end()) {
            it->second.listeners.erase(list);
            if (0 == it->second.listeners.size()) {
                auto it2 = it;
                it++;
                if (inotify_rm_watch(m_in_fd, it2->first)) {
                    throw std::runtime_error("inotify_rm_watch() failed.");
                }
            } else {
                it++;
            }
        }
    }
}

