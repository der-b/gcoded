#include "test_header.hh"
#include <iostream>
#include <mqtt_messages/MsgPrint.hh>

int main(int argc, char **argv)
{
    {
        MsgPrint orig("Holla the Woodfary!");
        std::vector<char> msg;
        orig.encode(msg);
        if (MsgType::Type::PRINT != (MsgType::Type)msg[0]) {
            return FAIL;
        }
        MsgPrint copy;
        copy.decode(msg);

        if (orig != copy) {
            return FAIL;
        }

        std::cout << "Gcode: " << copy.gcode() << "\n";
    }

    {
        MsgPrint orig("Holla the Woodfary!");
        std::vector<char> msg;
        orig.encode(msg);
        if (MsgType::Type::PRINT != (MsgType::Type)msg[0]) {
            return FAIL;
        }
        MsgPrint::header_msg *msg_view = (MsgPrint::header_msg *)(msg.data() + sizeof(MsgType::header_msg));
        msg_view->gcode_len = 255;
        MsgPrint copy;
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
