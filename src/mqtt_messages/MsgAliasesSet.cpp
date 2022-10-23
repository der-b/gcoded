#include "MsgAliasesSet.hh"
#include <iostream>
#include <sys/random.h>

/*
 * MsgAliasesSet()
 */
MsgAliasesSet::MsgAliasesSet()
    : m_type(MsgType::Type::ALIASES_SET)
{
    m_msg.device_name_len = 0;
    m_msg.device_alias_len = 0;
}


/*
 * encode()
 */
void MsgAliasesSet::encode(std::vector<char> &encoded_msg) const
{
    m_type.encode(encoded_msg);
    encoded_msg.insert(encoded_msg.end(), (char *)&m_msg, ((char *)&m_msg) + sizeof(m_msg));
    encoded_msg.insert(encoded_msg.end(), m_device_name.begin(), m_device_name.end());
    encoded_msg.insert(encoded_msg.end(), m_device_alias.begin(), m_device_alias.end());
}


/*
 * decode()
 */
size_t MsgAliasesSet::decode(const std::vector<char> &encoded_msg)
{
    size_t pos = m_type.decode(encoded_msg);
    if (m_type.type() != MsgType::Type::ALIASES_SET) {
        throw std::runtime_error("MsgAliasesSetResponse::decode(): Wrong message type.");
    }
    if (encoded_msg.size() - pos < sizeof(m_msg)) {
        throw std::runtime_error("MsgAliasesSet::decode(): Invalid encoded message: message to short");
    }
    memcpy(&m_msg, encoded_msg.data() + pos, sizeof(m_msg));
    pos += sizeof(m_msg);

    if (encoded_msg.size() - pos < m_msg.device_name_len) {
        throw std::runtime_error("MsgAliasesSet::decode(): Invalid encoded message: message to short");
    }
    m_device_name = std::string(encoded_msg.data() + pos, encoded_msg.data() + pos + m_msg.device_name_len);
    pos += m_msg.device_name_len;

    if (encoded_msg.size() - pos < m_msg.device_alias_len) {
        throw std::runtime_error("MsgAliasesSet::decode(): Invalid encoded message: message to short");
    }
    m_device_alias = std::string(encoded_msg.data() + pos, encoded_msg.data() + pos + m_msg.device_alias_len);
    pos += m_msg.device_alias_len;

    return pos;
}

