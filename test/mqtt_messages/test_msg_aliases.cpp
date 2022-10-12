#include "test_header.hh"
#include <iostream>
#include <mqtt_messages/MsgAliases.hh>

int main(int argc, char **argv)
{
    {
        MsgAliases orig;
        orig.set_provider_alias("ZXY");
        orig.add_alias("abc", "zxy");
        orig.add_alias("def", "wvu");
        std::cout << "ORIGINAL:\n";
        std::cout << orig.to_str() << "\n";
        std::vector<char> msg;
        orig.encode(msg);
        if (MsgType::Type::ALIASES != (MsgType::Type)msg[0]) {
            return FAIL;
        }
        MsgAliases copy;
        copy.decode(msg);
        std::cout << "COPY:\n";
        std::cout << copy.to_str() << "\n";

        if (orig != copy) {
            return FAIL;
        }
    }

    return SUCCESS;
}
