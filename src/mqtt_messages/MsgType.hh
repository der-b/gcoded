#ifndef __MSG_TYPE_HH__
#define __MSG_TYPE_HH__

#include "Msg.hh"

class MsgType : public Msg {
    public:
        enum Type {
            UNKNOWN = 0,
            DEVICE_STATE = 1,
            PRINT = 2,
            PRINT_RESPONSE = 3,
            PRINT_PROGRESS = 4,
            ALIASES = 5,
            ALIASES_SET = 6,
            ALIASES_SET_PROVIDER = 7,
            SENSOR_READINGS = 8,
            // this entry needs to be the last element and needs a number which is higher
            // by one compared to the previous enty
            __LAST_ENTRY = 9
        };

        struct header_msg {
            uint8_t type;
        };

        MsgType() 
        {
            m_msg.type = UNKNOWN;
        }

        MsgType(enum Type type) 
        {
            m_msg.type = (uint8_t)type;
        }

        virtual ~MsgType() {};

        enum Type type() const
        {
            return (enum Type)m_msg.type;
        }

        bool operator==(const MsgType &b)
        {
            return 0 == memcmp(&m_msg, &b.m_msg, sizeof(m_msg));
        }

        bool operator!=(const MsgType &b)
        {
            return !(*this == b);
        }

        void encode(std::vector<char> &encoded_msg) const override;
        size_t decode(const std::vector<char> &encoded_msg) override;

    private:
        void set_type(enum Type type)
        {
            m_msg.type = type;
        }
        struct header_msg m_msg;
};

#endif
