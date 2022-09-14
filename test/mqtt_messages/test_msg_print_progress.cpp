#include "test_header.hh"
#include <iostream>
#include <mqtt_messages/MsgPrintProgress.hh>

int main(int argc, char **argv)
{
    {
        MsgPrintProgress orig(13,37);
        std::vector<char> msg;
        orig.encode(msg);
        if (MsgType::Type::PRINT_PROGRESS != (MsgType::Type)msg[0]) {
            return FAIL;
        }
        MsgPrintProgress copy;
        copy.decode(msg);

        if (orig != copy) {
            return FAIL;
        }
    }

    {
        MsgPrintProgress orig(13,37);
        std::vector<char> msg;
        orig.encode(msg);
        if (MsgType::Type::PRINT_PROGRESS != (MsgType::Type)msg[0]) {
            return FAIL;
        }
        msg[1] = 255;
        MsgPrintProgress copy;
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
