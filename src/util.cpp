#include "util.hpp"
#include <string.h>

int8_t unsigned_to_signed(uint8_t x)
{
    int8_t ret;
    memcpy(&ret, &x, 1);
    return ret;
}
