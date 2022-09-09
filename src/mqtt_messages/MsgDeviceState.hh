#ifndef __MSG_DEVICE_STATE_HH__
#define __MSG_DEVICE_STATE_HH__

#include "Msg.hh"
#include "MsgType.hh"
#include "../devices/Device.hh"

class MsgDeviceState : public Msg {
    public:
        MsgDeviceState();
        MsgDeviceState(enum Device::State state);

        struct header_msg {
            uint8_t state;
        };

        bool operator==(const MsgDeviceState &b)
        {
            
            return    m_type == b.m_type
                   && 0 == memcmp(&m_msg, &b.m_msg, sizeof(m_msg));
        }

        bool operator!=(const MsgDeviceState &b)
        {
            return !(*this == b);
        }

        void encode(std::vector<char> &encoded_msg) const override;
        size_t decode(const std::vector<char> &encoded_msg) override;

        enum Device::State device_state() const {
            return (enum Device::State)m_msg.state;
        }

    private:
        MsgType m_type;
        struct header_msg m_msg;
};

#endif
