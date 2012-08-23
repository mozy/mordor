#include "zero.h"

#include <string.h>

ZeroStream ZeroStream::s_zeroStream;

size_t
ZeroStream::read(void *buffer, size_t length)
{
    memset(buffer, 0, length);
    return length;
}
