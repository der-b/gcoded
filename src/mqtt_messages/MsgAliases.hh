#ifndef __MSG_ALIASES_HH__
#define __MSG_ALIASES_HH__

#include <string>
#include <map>
#include "Msg.hh"
#include "MsgType.hh"

class MsgAliases : public Msg {
    public:
        MsgAliases();
        virtual ~MsgAliases() {};

        struct header_msg {
            uint8_t provider_alias_len;
        };

        struct header_device_alias {
            uint8_t device_name_len;
            uint8_t device_alias_len;
        };

        bool operator==(const MsgAliases &b)
        {
            return    m_type == b.m_type
                   && 0 == memcmp(&m_msg, &b.m_msg, sizeof(m_msg))
                   && m_provider_alias == b.m_provider_alias
                   && m_aliases == b.m_aliases;
        }

        bool operator!=(const MsgAliases &b)
        {
            return !(*this == b);
        }

        void encode(std::vector<char> &encoded_msg) const override;
        size_t decode(const std::vector<char> &encoded_msg) override;

        void set_provider_alias(const std::string &alias)
        {
            m_provider_alias = alias;
            m_msg.provider_alias_len = m_provider_alias.size();
        }

        const std::string &provider_alias() const
        {
            return m_provider_alias;
        }

        void add_alias(const std::string &device, const std::string &alias)
        {
            m_aliases[device] = alias;
        }

        std::string to_str() const;

        const std::map<std::string, std::string> &aliases() const
        {
            return m_aliases;
        }

    private:
        MsgType m_type;
        struct header_msg m_msg;
        std::string m_provider_alias;
        // device -> alias
        std::map<std::string, std::string> m_aliases;
};

#endif
