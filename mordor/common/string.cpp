// Copyright (c) 2009 - Decho Corp.

#include "mordor/common/pch.h"

#include "mordor/common/string.h"

#include "assert.h"
#include "exception.h"

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
            ASSERT(false);
        if (padding > 0 && ptr != end)
            ASSERT(false);
        if (padding > 2)
            ASSERT(false);

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

void
hexstringFromData(const void *data, size_t len, char *hex)
{
    const unsigned char *buf = (const unsigned char *)data;
    size_t i, j;
    for (i = j = 0; i < len; ++i) {
        char c;
        c = (buf[i] >> 4) & 0xf;
        c = (c > 9) ? c + 'a' - 10 : c + '0';
        hex[j++] = c;
        c = (buf[i] & 0xf);
        c = (c > 9) ? c + 'a' - 10 : c + '0';
        hex[j++] = c;
    }
    hex[j] = '\0';
}

std::string
hexstringFromData(const void *data, size_t len)
{
    std::string result;
    result.resize(len * 2);
    hexstringFromData(data, len, (char *)result.data());
    return result;
}

void
replace(std::string &str, char find, char replaceWith)
{
    size_t index = str.find(find);
    while (index != std::string::npos) {
        str[index] = replaceWith;
        index = str.find(find, index + 1);
    }
}

#ifdef WINDOWS
std::string toUtf8(const wchar_t *str, size_t len)
{
    if (len == (size_t)~0)
        len = wcslen(str);
    ASSERT(len < 0x80000000u);
    std::string result;
    if (len == 0)
        return result;
    int ret = WideCharToMultiByte(CP_UTF8, 0, str, (int)len, NULL, 0, NULL, NULL);
    if (ret == 0)
        throwExceptionFromLastError();
    result.resize(ret);
    ret = WideCharToMultiByte(CP_UTF8, 0, str, (int)len, &result[0], ret, NULL, NULL);
    if (ret == 0)
        throwExceptionFromLastError();
    ASSERT(ret == result.size());

    return result;
}

std::string toUtf8(const std::wstring &str)
{
    ASSERT(str.size() < 0x80000000u);
    return toUtf8(str.c_str(), (int)str.size());
}
#endif
