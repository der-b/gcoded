#include "MsgPrintProgress.hh"
#include <iostream>
#include <sys/random.h>

/*
 * MsgPrintProgress()
 */
MsgPrintProgress::MsgPrintProgress()
    : m_type(MsgType::Type::PRINT_PROGRESS)
{
    m_msg.percentage = 0;
    m_msg.remaining_time = 0;
}


/*
 * MsgPrintProgress()
 */
MsgPrintProgress::MsgPrintProgress(uint8_t percentage, uint32_t remaining_time)
    : m_type(MsgType::Type::PRINT_PROGRESS)
{
    m_msg.percentage = percentage;
    m_msg.remaining_time = remaining_time;
}


/*
 * encode()
 */
void MsgPrintProgress::encode(std::vector<char> &encoded_msg) const
{
    m_type.encode(encoded_msg);
    encoded_msg.insert(encoded_msg.end(), (char *)&m_msg, ((char *)&m_msg) + sizeof(m_msg));
}


/*
 * decode()
 */
size_t MsgPrintProgress::decode(const std::vector<char> &encoded_msg)
{
    size_t pos = m_type.decode(encoded_msg);
    if (m_type.type() != MsgType::Type::PRINT_PROGRESS) {
        throw std::runtime_error("MsgPrintProgressResponse::decode(): Wrong message type.");
    }
    if (encoded_msg.size() - pos < sizeof(m_msg)) {
        throw std::runtime_error("MsgPrintProgress::decode(): Invalid encoded message: message to short");
    }
    memcpy(&m_msg, encoded_msg.data() + pos, sizeof(m_msg));
    pos += sizeof(m_msg);
    if (m_msg.percentage > 100) {
        throw std::runtime_error("MsgPrintProgress::decode(): percentage in received message it >100.");
    }
    return pos;
}
