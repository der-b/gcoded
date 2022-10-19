#ifndef __MSG_ALIASES_SET_PROVIDER_HH__
#define __MSG_ALIASES_SET_PROVIDER_HH__

#include <string>
#include <map>
#include "Msg.hh"
#include "MsgType.hh"

class MsgAliasesSetProvider : public Msg {
    public:
        MsgAliasesSetProvider();
        virtual ~MsgAliasesSetProvider() {};

        struct header_msg {
            uint8_t provider_alias_len;
        };

        bool operator==(const MsgAliasesSetProvider &b)
        {
            return    m_type == b.m_type
                   && 0 == memcmp(&m_msg, &b.m_msg, sizeof(m_msg))
                   && m_provider_alias == b.m_provider_alias;
        }

        bool operator!=(const MsgAliasesSetProvider &b)
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

    private:
        MsgType m_type;
        struct header_msg m_msg;
        std::string m_provider_alias;
};

#endif
