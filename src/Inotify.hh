#ifndef __INOTIFY_HH__
#define __INOTIFY_HH__

#include <string>
#include <map>
#include <set>
#include <optional>
#include <mutex>
#include <sys/inotify.h>
#include "EventLoop.hh"

class Inotify {
    public:
        // Event types
        static constexpr uint32_t CREATE = IN_CREATE;
        static constexpr uint32_t DELETE = IN_DELETE;
        static constexpr uint32_t IS_DIR = IN_ISDIR;
        static constexpr uint32_t ATTRIB = IN_ATTRIB;
        static constexpr uint32_t DELETE_SELF = IN_DELETE_SELF;

        class Listener {
            public:
                virtual void on_fs_event(const std::string &path, uint32_t event_type, const std::optional<std::string> &name) = 0;
        };

        Inotify(const Inotify &) = delete;
        Inotify(const Inotify &&) = delete;
        Inotify &operator=(const Inotify&) = delete;

        static Inotify &get()
        {
            static Inotify notify;
            return notify;
        }

        virtual ~Inotify();

        void register_listener(const std::string &path, uint32_t event_type, Listener *list);
        void unregister_listener(const std::optional<std::string> path, Listener *list);

    public:
        struct listener_data {
            uint32_t event;
            std::string path;
            std::set<Listener *> listeners;

            listener_data(uint32_t _event, const std::string &_path)
                : event(_event),
                  path(_path)
            {}
        };
        struct callback_data {
            std::recursive_mutex mutex;
            std::map<int, struct listener_data> listeners;
        };

    private:
        Inotify();

    private:
        EventLoop &m_ev;
        int m_in_fd;
        struct callback_data m_cb_data;
};

#endif
