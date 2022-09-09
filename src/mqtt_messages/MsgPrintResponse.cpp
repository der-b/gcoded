#include "MsgPrintResponse.hh"
#include <iostream>
#include <sys/random.h>

/*
 * MsgPrintResponse()
 */
MsgPrintResponse::MsgPrintResponse()
    : m_type(MsgType::Type::PRINT_RESPONSE)
{
    m_msg.print_result = static_cast<uint8_t>(Device::PrintResult::INVALID);
}


/*
 * MsgPrintResponse()
 */
MsgPrintResponse::MsgPrintResponse(const MsgPrint &print_msg, Device::PrintResult print_result)
    : m_type(MsgType::Type::PRINT_RESPONSE)
{ 
    m_msg.request_code_part1 = print_msg.request_code_part1();
    m_msg.request_code_part2 = print_msg.request_code_part2();
    m_msg.print_result = static_cast<uint8_t>(print_result);
}


/*
 * encode()
 */
void MsgPrintResponse::encode(std::vector<char> &encoded_msg) const
{
    m_type.encode(encoded_msg);
    encoded_msg.insert(encoded_msg.end(), (char *)&m_msg, ((char *)&m_msg) + sizeof(m_msg));
}


/*
 * decode()
 */
size_t MsgPrintResponse::decode(const std::vector<char> &encoded_msg)
{
    size_t pos = m_type.decode(encoded_msg);
    if (m_type.type() != MsgType::Type::PRINT_RESPONSE) {
        throw std::runtime_error("MsgPrintResponse::decode(): Wrong message type.");
    }
    if (encoded_msg.size() - pos < sizeof(m_msg)) {
        throw std::runtime_error("MsgPrintResponse::decode(): Invalid encoded message: message to short");
    }
    memcpy(&m_msg, encoded_msg.data() + pos, sizeof(m_msg));
    pos += sizeof(m_msg);
    if (m_msg.print_result >= static_cast<uint8_t>(Device::PrintResult::__LAST_ENTRY)) {
        throw std::runtime_error("MsgPrintResponse::decode(): Invalid encoded message: Invalid PrintResult");
    }
    return pos;
}
