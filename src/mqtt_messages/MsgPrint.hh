#ifndef __MSG_PRINT_HH__
#define __MSG_PRINT_HH__

#include <string>
#include "Msg.hh"
#include "MsgType.hh"

class MsgPrint : public Msg {
    public:
        MsgPrint();
        MsgPrint(const std::string &gcode);
        virtual ~MsgPrint() {};

        struct header_msg {
            // 128 bit random number, which will be copied to the respons message
            // See MsgPrintResponse
            // this number is generated on message construction, to generate a new 
            // request code, it is necessary to create a new message!
            uint64_t request_code_part1;
            uint64_t request_code_part2;
            size_t gcode_len;
        };

        bool operator==(const MsgPrint &b)
        {
            
            return    m_type == b.m_type
                   && 0 == memcmp(&m_msg, &b.m_msg, sizeof(m_msg))
                   && m_gcode == b.m_gcode;
        }

        bool operator!=(const MsgPrint &b)
        {
            return !(*this == b);
        }

        void encode(std::vector<char> &encoded_msg) const override;
        size_t decode(const std::vector<char> &encoded_msg) override;

        const std::string &gcode() const {
            return m_gcode;
        }

        uint64_t request_code_part1() const {
            return m_msg.request_code_part1;
        }

        uint64_t request_code_part2() const {
            return m_msg.request_code_part1;
        }

    private:
        MsgType m_type;
        struct header_msg m_msg;
        std::string m_gcode;
};

#endif
