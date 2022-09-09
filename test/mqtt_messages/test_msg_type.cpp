#include "test_header.hh"
#include <iostream>
#include <mqtt_messages/MsgType.hh>

int main(int argc, char **argv)
{
    {
        MsgType orig;
        std::vector<char> msg;
        orig.encode(msg);
        if (msg.size() != 1) {
            return FAIL;
        }
        if (0 != msg[0]) {
            return FAIL;
        }
        MsgType copy;
        copy.decode(msg);

        if (orig != copy) {
            return FAIL;
        }
    }
    {
        MsgType orig;
        std::vector<char> msg;
        orig.encode(msg);
        if (msg.size() != 1) {
            return FAIL;
        }
        if (0 != msg[0]) {
            return FAIL;
        }
        msg[0] = 255;
        MsgType copy;
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
