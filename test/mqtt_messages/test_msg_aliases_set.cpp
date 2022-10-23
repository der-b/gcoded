#include "test_header.hh"
#include <iostream>
#include <mqtt_messages/MsgAliasesSet.hh>

int main(int argc, char **argv)
{
    {
        MsgAliasesSet orig;
        orig.set_device_name("abc");
        orig.set_device_alias("def");
        std::cout << "ORIGINAL:\n";
        std::cout << orig.device_name() << "\n";
        std::cout << orig.device_alias() << "\n";
        std::vector<char> msg;
        orig.encode(msg);
        if (MsgType::Type::ALIASES_SET != (MsgType::Type)msg[0]) {
            return FAIL;
        }
        MsgAliasesSet copy;
        copy.decode(msg);
        std::cout << "COPY:\n";
        std::cout << copy.device_name() << "\n";
        std::cout << copy.device_alias() << "\n";

        if (orig != copy) {
            return FAIL;
        }
    }

    return SUCCESS;
}
