#include "MsgAliasesSetProvider.hh"
#include <iostream>
#include <sys/random.h>

/*
 * MsgAliasesSetProvider()
 */
MsgAliasesSetProvider::MsgAliasesSetProvider()
    : m_type(MsgType::Type::ALIASES_SET_PROVIDER)
{
    m_msg.provider_alias_len = 0;
}


/*
 * encode()
 */
void MsgAliasesSetProvider::encode(std::vector<char> &encoded_msg) const
{
    m_type.encode(encoded_msg);
    encoded_msg.insert(encoded_msg.end(), (char *)&m_msg, ((char *)&m_msg) + sizeof(m_msg));
    encoded_msg.insert(encoded_msg.end(), m_provider_alias.begin(), m_provider_alias.end());
}


/*
 * decode()
 */
size_t MsgAliasesSetProvider::decode(const std::vector<char> &encoded_msg)
{
    size_t pos = m_type.decode(encoded_msg);
    if (m_type.type() != MsgType::Type::ALIASES_SET_PROVIDER) {
        throw std::runtime_error("MsgAliasesSetProviderResponse::decode(): Wrong message type.");
    }
    if (encoded_msg.size() - pos < sizeof(m_msg)) {
        throw std::runtime_error("MsgAliasesSetProvider::decode(): Invalid encoded message: message to short");
    }
    memcpy(&m_msg, encoded_msg.data() + pos, sizeof(m_msg));
    pos += sizeof(m_msg);

    if (encoded_msg.size() - pos < m_msg.provider_alias_len) {
        throw std::runtime_error("MsgAliasesSetProvider::decode(): Invalid encoded message: message to short");
    }
    m_provider_alias = std::string(encoded_msg.data() + pos, encoded_msg.data() + pos + m_msg.provider_alias_len);
    pos += m_msg.provider_alias_len;

    return pos;
}

