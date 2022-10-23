#ifndef __MSG_ALIASES_SET_HH__
#define __MSG_ALIASES_SET_HH__

#include <string>
#include <map>
#include "Msg.hh"
#include "MsgType.hh"

class MsgAliasesSet : public Msg {
    public:
        MsgAliasesSet();
        virtual ~MsgAliasesSet() {};

        struct header_msg {
            uint8_t device_name_len;
            uint8_t device_alias_len;
        };

        bool operator==(const MsgAliasesSet &b)
        {
            return    m_type == b.m_type
                   && 0 == memcmp(&m_msg, &b.m_msg, sizeof(m_msg))
                   && m_device_name == b.m_device_name
                   && m_device_alias == b.m_device_alias;
        }

        bool operator!=(const MsgAliasesSet &b)
        {
            return !(*this == b);
        }

        void encode(std::vector<char> &encoded_msg) const override;
        size_t decode(const std::vector<char> &encoded_msg) override;

        void set_device_alias(const std::string &alias)
        {
            m_device_alias = alias;
            m_msg.device_alias_len = m_device_alias.size();
        }

        const std::string &device_alias() const
        {
            return m_device_alias;
        }

        void set_device_name(const std::string &name)
        {
            m_device_name = name;
            m_msg.device_name_len = m_device_name.size();
        }

        const std::string &device_name() const
        {
            return m_device_name;
        }

    private:
        MsgType m_type;
        struct header_msg m_msg;
        std::string m_device_name;
        std::string m_device_alias;
};

#endif
