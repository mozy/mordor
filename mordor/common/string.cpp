// Copyright (c) 2009 - Decho Corp.

#include "common/string.h"

#include <cassert>

std::string
base64decode(std::string src)
{
    std::string result;
    result.resize(src.size() * 3 / 4);
    char *writeBuf = &result[0];

    const char* ptr = src.c_str();
    const char* end = ptr + src.size();

    while(ptr < end) {
        int i = 0;
        int padding = 0;
        int packed = 0;
        for(; i < 4 && ptr < end; ++i, ++ptr) {
            if(*ptr == '=') {
                ++padding;
                packed <<= 6;
                continue;
            }

            int val = 0;
            if(*ptr >= 'A' && *ptr <= 'Z')
                val = *ptr - 'A';
            else if(*ptr >= 'a' && *ptr <= 'z')
                val = *ptr - 'a' + 26;
            else if(*ptr >= '0' && *ptr <= '9')
                val = *ptr - '0' + 52;
            else if(*ptr == '+')
                val = 62;
            else if(*ptr == '/')
                val = 63;
            else
                return "";

            packed = (packed << 6) | val;
        }
        if (i != 4)
            assert(false);
        if (padding > 0 && ptr != end)
            assert(false);
        if (padding > 2)
            assert(false);

        *writeBuf++ = (char)((packed >> 16) & 0xff);
        if(padding != 2)
            *writeBuf++ = (char)((packed >> 8) & 0xff);
        if(padding == 0)
            *writeBuf++ = (char)(packed & 0xff);
    }

    result.resize(writeBuf - result.c_str());
    return result;
}

std::string
base64encode(const std::string& src)
{
    return base64encode(src.c_str(), src.size());
}

std::string
base64encode(const void* src, size_t len)
{
    const char* base64 =
        "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

    std::string ret;
    ret.reserve(len * 4 / 3 + 2);

    const unsigned char* ptr = (unsigned char*)src;
    const unsigned char* end = ptr + len;

    while(ptr < end) {
        unsigned int packed = 0;
        int i = 0;
        int padding = 0;
        for(; i < 3 && ptr < end; ++i, ++ptr)
            packed = (packed << 8) | *ptr;
        if(i == 2)
            padding = 1;
        else if (i == 1)
            padding = 2;
        for(; i < 3; ++i)
            packed <<= 8;

        ret.append(1, base64[packed >> 18]);
        ret.append(1, base64[(packed >> 12) & 0x3f]);
        if(padding != 2)
            ret.append(1, base64[(packed >> 6) & 0x3f]);
        if(padding == 0)
            ret.append(1, base64[packed & 0x3f]);
        ret.append(padding, '=');
    }

    return ret;
}
