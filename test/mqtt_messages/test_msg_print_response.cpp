#include "test_header.hh"
#include <iostream>
#include <mqtt_messages/MsgPrintResponse.hh>

int main(int argc, char **argv)
{
    MsgPrint print_msg("Holla the Woodfary!");
    {
        MsgPrintResponse orig(print_msg, Device::PrintResult::OK);
        std::vector<char> msg;
        orig.encode(msg);
        if (MsgType::Type::PRINT_RESPONSE != (MsgType::Type)msg[0]) {
            return FAIL;
        }
        MsgPrintResponse copy;
        copy.decode(msg);

        if (orig != copy) {
            return FAIL;
        }
    }

    {
        MsgPrintResponse orig(print_msg, Device::PrintResult::OK);
        std::vector<char> msg;
        orig.encode(msg);
        if (MsgType::Type::PRINT_RESPONSE != (MsgType::Type)msg[0]) {
            return FAIL;
        }
        MsgPrintResponse::header_msg *msg_view = (MsgPrintResponse::header_msg *)(msg.data() + sizeof(MsgType::header_msg));
        msg_view->print_result = 255;
        MsgPrintResponse copy;
        bool got_exception = false;
        try {
            copy.decode(msg);
        } catch (const std::runtime_error &e) {
            got_exception = true;
        }
        if (!got_exception) {
            return FAIL;
        }
    }
    return SUCCESS;
}
