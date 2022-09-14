#ifndef __MSG_PRINTPROGRESS_HH__
#define __MSG_PRINTPROGRESS_HH__

#include <string>
#include "Msg.hh"
#include "MsgType.hh"

class MsgPrintProgress : public Msg {
    public:
        MsgPrintProgress();
        MsgPrintProgress(uint8_t percentage, uint32_t remaining_time);
        virtual ~MsgPrintProgress() {};

        struct header_msg {
            uint8_t percentage;
            uint32_t remaining_time;
        };

        bool operator==(const MsgPrintProgress &b)
        {

            return    m_type == b.m_type
                   && 0 == memcmp(&m_msg, &b.m_msg, sizeof(m_msg));
        }

        bool operator!=(const MsgPrintProgress &b)
        {
            return !(*this == b);
        }

        void encode(std::vector<char> &encoded_msg) const override;
        size_t decode(const std::vector<char> &encoded_msg) override;

        uint32_t percentage() const {
            return m_msg.percentage;
        }

        uint32_t remaining_time() const {
            return m_msg.remaining_time;
        }

    private:
        MsgType m_type;
        struct header_msg m_msg;
};

#endif
