#include "MsgType.hh"
#include <stdexcept>
#include <iostream>


/*
 * encode()
 */
void MsgType::encode(std::vector<char> &encoded_msg) const
{
    encoded_msg.push_back(m_msg.type);
}


/*
 * decode()
 */
size_t MsgType::decode(const std::vector<char> &encoded_msg)
{
    if (encoded_msg.size() < 1) { 
        throw std::runtime_error("MsgType::decode(): Invalid encoded message: message to short");
    }
    m_msg.type = (enum Type)encoded_msg[0];
    if (__LAST_ENTRY <= m_msg.type) {
        throw std::runtime_error("MsgType::decode(): Invalid encoded message: invalid type");
    }
    return 1;
}
