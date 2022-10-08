#ifndef __ALIASES_HH__
#define __ALIASES_HH__

#include "Config.hh"
#include <optional>
#include <map>
#include <set>
#include <mutex>
#include <sqlite3.h>
#include "Inotify.hh"

class Aliases : public Inotify::Listener {
    public:
        /**
         * Represets the current state of the aliases
         */
        enum class State {
            /// Special placeholder, should not be used
            UNKNOWN = 0,
            /// During initialisation if the calss
            INIT = 1,
            /// Aliases can be used as intended
            OK = 2,
            /// Could not open aliases database, therefore aliases are disabled.
            ERR_FILE = 3,
            /// Could open aliases database only in read only mode, therefor existing aliases are used. No changes possible.
            READONLY = 4,
            __LAST_ENTRY
        };

        class Listener {
            public:
                virtual void on_alias_change() = 0;
        };

        Aliases(const Config &config);
        virtual ~Aliases();

        State state() const
        {
            return m_state;
        }

        /**
         * returns the provider alias
         */
        std::optional<std::string> provider_alias();

        /**
         * sets the provider alias
         */
        bool set_provider_alias(const std::string &alias);

        /**
         * sets an alias
         */
        bool set_alias(const std::string &device, const std::string &alias);

        /**
         * remove an alias
         */
        bool rm_alias(const std::string &device);

        /**
         * fills all aliases into the map
         */
        void get_aliases(std::map<std::string, std::string> &aliases);

        virtual void on_fs_event(const std::string &path, uint32_t event_type, const std::optional<std::string> &name);

        void register_listener(Listener *list)
        {
            const std::lock_guard<std::mutex> guard(m_listener_mutex);
            m_listeners.insert(list);
        }

        void unregister_listener(Listener *list)
        {
            const std::lock_guard<std::mutex> guard(m_listener_mutex);
            m_listeners.erase(list);
        }

    private:
        /**
         * This class represents an transaction, which is roles back all SQL statements if the object is
         * destroyed. To commit the statements it is necessary to call commit() on the object.
         */
        class Transaction {
            public:
                Transaction(sqlite3 *db);

                /**
                 * Roles back all SQL statements, if commit() was not called before.
                 */
                ~Transaction() noexcept(false);

                /**
                 * Commits a SQL transaction.
                 */
                void commit();

            private:
                bool m_committed;
                sqlite3 *m_db;
        };


        /**
         * returns the count of aliases for the provider.
         * if the value is grater than one, than there is something wrong.
         */
        uint64_t provider_alias_count();

    private:
        sqlite3 *m_db;
        const Config &m_config;
        State m_state;
        std::mutex m_listener_mutex;
        std::set<Listener *> m_listeners;
};


/**
 * converts a Aliases::State to a string.
 */
const std::string &to_str(enum Aliases::State state);

#endif
