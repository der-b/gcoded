#include "test_header.hh"
#include <iostream>
#include <mqtt_messages/MsgAliasesSetProvider.hh>

int main(int argc, char **argv)
{
    {
        MsgAliasesSetProvider orig;
        orig.set_provider_alias("ZXY");
        std::cout << "ORIGINAL:\n";
        std::cout << orig.provider_alias() << "\n";
        std::vector<char> msg;
        orig.encode(msg);
        if (MsgType::Type::ALIASES_SET_PROVIDER != (MsgType::Type)msg[0]) {
            return FAIL;
        }
        MsgAliasesSetProvider copy;
        copy.decode(msg);
        std::cout << "COPY:\n";
        std::cout << copy.provider_alias() << "\n";

        if (orig != copy) {
            return FAIL;
        }
    }

    return SUCCESS;
}
