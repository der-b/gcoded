#include "MsgPrint.hh"
#include <iostream>
#include <sys/random.h>

/*
 * MsgPrint()
 */
MsgPrint::MsgPrint()
    : m_type(MsgType::Type::PRINT)
{
    m_msg.gcode_len = 0;
    constexpr size_t len = sizeof(uint64_t);
    if (   len != getrandom(&m_msg.request_code_part1, len, 0)
        || len != getrandom(&m_msg.request_code_part2, len, 0)) {
        throw std::runtime_error("Could not get random number from OS for MsgPrint message!\n");
    }
}


/*
 * MsgPrint()
 */
MsgPrint::MsgPrint(const std::string &gcode)
    : m_type(MsgType::Type::PRINT)
{ 
    // TODO: We allways copy the whole gcode ... this could be more efficient!
    m_gcode = gcode;
    m_msg.gcode_len = gcode.size();
    constexpr size_t len = sizeof(uint64_t);
    if (   len != getrandom(&m_msg.request_code_part1, len, 0)
        || len != getrandom(&m_msg.request_code_part2, len, 0)) {
        throw std::runtime_error("Could not get random number from OS for MsgPrint message!\n");
    }
}


/*
 * encode()
 */
void MsgPrint::encode(std::vector<char> &encoded_msg) const
{
    m_type.encode(encoded_msg);
    encoded_msg.insert(encoded_msg.end(), (char *)&m_msg, ((char *)&m_msg) + sizeof(m_msg));
    // TODO: Check for maximum MQTT message size!
    encoded_msg.insert(encoded_msg.end(), m_gcode.data(), m_gcode.data() + m_gcode.size());
}


/*
 * decode()
 */
size_t MsgPrint::decode(const std::vector<char> &encoded_msg)
{
    size_t pos = m_type.decode(encoded_msg);
    if (m_type.type() != MsgType::Type::PRINT) {
        throw std::runtime_error("MsgPrintResponse::decode(): Wrong message type.");
    }
    if (encoded_msg.size() - pos < sizeof(m_msg)) {
        throw std::runtime_error("MsgPrint::decode(): Invalid encoded message: message to short");
    }
    memcpy(&m_msg, encoded_msg.data() + pos, sizeof(m_msg));
    pos += sizeof(m_msg);
    // TODO: Check for maximum MQTT message size!
    if (encoded_msg.size() - pos != m_msg.gcode_len) {
        throw std::runtime_error("MsgPrint::decode(): Invalid encoded message: length fields does not add up the the exact message size.");
    }
    m_gcode.clear();
    m_gcode.append(encoded_msg.data() + pos, m_msg.gcode_len);
    pos += m_msg.gcode_len;
    return pos;
}
