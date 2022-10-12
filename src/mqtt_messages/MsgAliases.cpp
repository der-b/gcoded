#include "MsgAliases.hh"
#include <iostream>
#include <sys/random.h>

/*
 * MsgAliases()
 */
MsgAliases::MsgAliases()
    : m_type(MsgType::Type::ALIASES)
{
    m_msg.provider_alias_len = 0;
}


/*
 * encode()
 */
void MsgAliases::encode(std::vector<char> &encoded_msg) const
{
    m_type.encode(encoded_msg);
    encoded_msg.insert(encoded_msg.end(), (char *)&m_msg, ((char *)&m_msg) + sizeof(m_msg));
    encoded_msg.insert(encoded_msg.end(), m_provider_alias.begin(), m_provider_alias.end());
    for (const auto &alias: m_aliases) {
        struct header_device_alias header;
        header.device_name_len = alias.first.size();
        header.device_alias_len = alias.second.size();
        encoded_msg.insert(encoded_msg.end(), (char *)&header, ((char *)&header) + sizeof(header));
        encoded_msg.insert(encoded_msg.end(), alias.first.begin(), alias.first.end());
        encoded_msg.insert(encoded_msg.end(), alias.second.begin(), alias.second.end());
    }
}


/*
 * decode()
 */
size_t MsgAliases::decode(const std::vector<char> &encoded_msg)
{
    size_t pos = m_type.decode(encoded_msg);
    if (m_type.type() != MsgType::Type::ALIASES) {
        throw std::runtime_error("MsgAliasesResponse::decode(): Wrong message type.");
    }
    if (encoded_msg.size() - pos < sizeof(m_msg)) {
        throw std::runtime_error("MsgAliases::decode(): Invalid encoded message: message to short");
    }
    memcpy(&m_msg, encoded_msg.data() + pos, sizeof(m_msg));
    pos += sizeof(m_msg);

    if (encoded_msg.size() - pos < m_msg.provider_alias_len) {
        throw std::runtime_error("MsgAliases::decode(): Invalid encoded message: message to short");
    }
    m_provider_alias = std::string(encoded_msg.data() + pos, encoded_msg.data() + pos + m_msg.provider_alias_len);
    pos += m_msg.provider_alias_len;

    m_aliases.clear();
    while (encoded_msg.size() > pos) {
        struct header_device_alias *header = (struct header_device_alias *)(encoded_msg.data() + pos);
        pos += sizeof(*header);
        if (encoded_msg.size() - pos < header->device_name_len) {
            throw std::runtime_error("MsgAliases::decode(): Invalid encoded message: message to short");
        }
        std::string device(encoded_msg.data() + pos, encoded_msg.data() + pos + header->device_name_len);
        pos += header->device_name_len;
        if (encoded_msg.size() - pos < header->device_alias_len) {
            throw std::runtime_error("MsgAliases::decode(): Invalid encoded message: message to short");
        }
        std::string alias(encoded_msg.data() + pos, encoded_msg.data() + pos + header->device_alias_len);
        pos += header->device_alias_len;
        m_aliases[device] = alias;
    }

    return pos;
}


/*
 * to_str()
 */
std::string MsgAliases::to_str() const
{
    std::string out;
    out += "provider_alias: '" + m_provider_alias + "' (" + std::to_string(m_provider_alias.size()) + ")\n";
    for (const auto &alias: m_aliases) {
        out += "    '" + alias.first + "' --> '" + alias.second + "'\n";
    }

    return out;
}
