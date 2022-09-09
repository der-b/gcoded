#include "MsgDeviceState.hh"
#include <iostream>

/*
 * MsgDeviceState()
 */
MsgDeviceState::MsgDeviceState()
    : m_type(MsgType::Type::DEVICE_STATE)
{
    m_msg.state = (char)Device::State::UNINITIALIZED;
}


/*
 * MsgDeviceState()
 */
MsgDeviceState::MsgDeviceState(enum Device::State state)
    : m_type(MsgType::Type::DEVICE_STATE)
{ 
    m_msg.state = static_cast<uint8_t>(state);
}


/*
 * encode()
 */
void MsgDeviceState::encode(std::vector<char> &encoded_msg) const
{
    m_type.encode(encoded_msg);
    encoded_msg.push_back(m_msg.state);
}


/*
 * decode()
 */
size_t MsgDeviceState::decode(const std::vector<char> &encoded_msg)
{
    size_t pos = m_type.decode(encoded_msg);
    if (encoded_msg.size() - pos != 1) {
        throw std::runtime_error("MsgDeviceState::decode(): Invalid encoded message: message to short");
    }
    m_msg.state = static_cast<uint8_t>(encoded_msg[pos]);
    if (static_cast<uint8_t>(Device::State::__LAST_ENTRY) <= m_msg.state) {
        throw std::runtime_error("MsgDeviceState::decode(): Invalid encoded message: invalid state");
    }
    return pos + 1;
}
