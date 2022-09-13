#ifndef __MSG_HH__
#define __MSG_HH__

#include <vector>
#include <cstdint>
#include <cstring>
#include <cstddef>
#include <typeinfo>

class Msg {
    public:

        /**
         * Encodes the current into the provides vector.
         * This method does only attach to the end of the vector! 
         * The caller has to make sure, that the vector is cleard before.
         */
        virtual void encode(std::vector<char> &encoded_msg) const = 0;

        /**
         * Consumes the first bytes of the byte vector to decode the header_msg
         * and sets the private variables of the current object accordingly.
         * Returns the number of consumed bytes.
         */
        virtual size_t decode(const std::vector<char> &encoded_msg) = 0;

        virtual ~Msg() {};
};

#endif
