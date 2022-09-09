#ifndef __MSG_PRINT_RESPONSE_HH__
#define __MSG_PRINT_RESPONSE_HH__

#include <string>
#include "Msg.hh"
#include "MsgType.hh"
#include "MsgPrint.hh"
#include "devices/Device.hh"

class MsgPrintResponse : public Msg {
    public:
        MsgPrintResponse();
        MsgPrintResponse(const MsgPrint &print_msg, Device::PrintResult print_result);

        struct header_msg {
            // 128 bit response identifier copied from the MsgPrint message
            uint64_t request_code_part1;
            uint64_t request_code_part2;
            uint8_t print_result;
        };

        bool operator==(const MsgPrintResponse &b)
        {
            
            return    m_type == b.m_type
                   && 0 == memcmp(&m_msg, &b.m_msg, sizeof(m_msg));
        }

        bool operator!=(const MsgPrintResponse &b)
        {
            return !(*this == b);
        }

        void encode(std::vector<char> &encoded_msg) const override;
        size_t decode(const std::vector<char> &encoded_msg) override;

        Device::PrintResult print_result() const {
            return static_cast<Device::PrintResult>(m_msg.print_result);
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
