// Copyright (c) 2009 - Mozy, Inc.

#include <algorithm>

#include <string.h>

#include <openssl/md5.h>
#include <openssl/sha.h>

#ifdef HAVE_CONFIG_H
#include "../config.h"
#ifdef HAVE_ICONV
#include <iconv.h>
#endif
#endif

#include "mordor/string.h"
#include "mordor/util.h"

#include "assert.h"
#include "exception.h"

#ifdef MSVC
#pragma comment(lib, "libeay32")
#endif

namespace Mordor {

std::string
base64decode(const std::string &src)
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

            // padding with "=" only
            if (padding > 0)
                return "";

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
                return ""; // invalid character

            packed = (packed << 6) | val;
        }
        if (i != 4)
            return "";
        if (padding > 0 && ptr != end)
            return "";
        if (padding > 2)
            return "";

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
base64encode(const std::string& data)
{
    return base64encode(data.c_str(), data.size());
}

std::string
base64encode(const void* data, size_t len)
{
    const char* base64 =
        "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

    std::string ret;
    ret.reserve(len * 4 / 3 + 2);

    const unsigned char* ptr = (const unsigned char*)data;
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

std::string
md5(const std::string &data)
{
    return hexstringFromData(md5sum(data).c_str(), MD5_DIGEST_LENGTH);
}

std::string
sha1(const std::string &data)
{
    return hexstringFromData(sha1sum(data).c_str(), SHA_DIGEST_LENGTH);
}

std::string
md5sum(const void *data, size_t len)
{
    MD5_CTX ctx;
    MD5_Init(&ctx);
    MD5_Update(&ctx, data, len);
    std::string result;
    result.resize(MD5_DIGEST_LENGTH);
    MD5_Final((unsigned char*)&result[0], &ctx);
    return result;
}

std::string
md5sum(const std::string &data)
{
    return md5sum(data.c_str(), data.size());
}

std::string
sha0sum(const void *data, size_t len)
{
    SHA_CTX ctx;
    SHA_Init(&ctx);
    SHA_Update(&ctx, data, len);
    std::string result;
    result.resize(SHA_DIGEST_LENGTH);
    SHA_Final((unsigned char*)&result[0], &ctx);
    return result;
}

std::string
sha0sum(const std::string & data)
{
    return sha0sum(data.c_str(), data.length());
}

std::string
sha1sum(const void *data, size_t len)
{
    SHA_CTX ctx;
    SHA1_Init(&ctx);
    SHA1_Update(&ctx, data, len);
    std::string result;
    result.resize(SHA_DIGEST_LENGTH);
    SHA1_Final((unsigned char*)&result[0], &ctx);
    return result;
}

std::string
sha1sum(const std::string &data)
{
    return sha1sum(data.c_str(), data.size());
}

struct xorStruct
{
    xorStruct(char value) : m_value(value) {}
    char m_value;
    char operator()(char in) const { return in ^ m_value; }
};

template <class CTX,
    int (*Init)(CTX *),
    int (*Update)(CTX *, const void *, size_t),
    int (*Final)(unsigned char *, CTX *),
    unsigned int B, unsigned int L>
std::string
hmac(const std::string &text, const std::string &key)
{
    std::string keyLocal = key;
    CTX ctx;
    if (keyLocal.size() > B) {
        Init(&ctx);
        Update(&ctx, keyLocal.c_str(), keyLocal.size());
        keyLocal.resize(L);
        Final((unsigned char *)&keyLocal[0], &ctx);
    }
    keyLocal.append(B - keyLocal.size(), '\0');
    std::string ipad = keyLocal, opad = keyLocal;
    std::transform(ipad.begin(), ipad.end(), ipad.begin(), xorStruct(0x36));
    std::transform(opad.begin(), opad.end(), opad.begin(), xorStruct(0x5c));
    Init(&ctx);
    Update(&ctx, ipad.c_str(), B);
    Update(&ctx, text.c_str(), text.size());
    std::string result;
    result.resize(L);
    Final((unsigned char *)&result[0], &ctx);
    Init(&ctx);
    Update(&ctx, opad.c_str(), B);
    Update(&ctx, result.c_str(), L);
    Final((unsigned char *)&result[0], &ctx);
    return result;
}

std::string
hmacMd5(const std::string &text, const std::string &key)
{
    return hmac<MD5_CTX,
        &MD5_Init,
        &MD5_Update,
        &MD5_Final,
        MD5_CBLOCK, MD5_DIGEST_LENGTH>
        (text, key);
}

std::string
hmacSha1(const std::string &text, const std::string &key)
{
    return hmac<SHA_CTX,
        &SHA1_Init,
        &SHA1_Update,
        &SHA1_Final,
        SHA_CBLOCK, SHA_DIGEST_LENGTH>
        (text, key);
}

std::string
hmacSha256(const std::string &text, const std::string &key)
{
    return hmac<SHA256_CTX,
        &SHA256_Init,
        &SHA256_Update,
        &SHA256_Final,
        SHA256_CBLOCK, SHA256_DIGEST_LENGTH>
        (text, key);
}

void
hexstringFromData(const void *data, size_t len, char *output)
{
    const unsigned char *buf = (const unsigned char *)data;
    size_t i, j;
    for (i = j = 0; i < len; ++i) {
        char c;
        c = (buf[i] >> 4) & 0xf;
        c = (c > 9) ? c + 'a' - 10 : c + '0';
        output[j++] = c;
        c = (buf[i] & 0xf);
        c = (c > 9) ? c + 'a' - 10 : c + '0';
        output[j++] = c;
    }
}

std::string
hexstringFromData(const void *data, size_t len)
{
    if (len == 0)
        return std::string();
    std::string result;
    result.resize(len * 2);
    hexstringFromData(data, len, &result[0]);
    return result;
}

std::string
hexstringFromData(const std::string &data)
{
    return hexstringFromData(data.c_str(), data.size());
}

void
dataFromHexstring(const char *hexstring, size_t length, void *output)
{
    unsigned char *buf = (unsigned char *)output;
    unsigned char byte;
    if (length % 2 != 0)
        MORDOR_THROW_EXCEPTION(std::invalid_argument("length"));
    for (size_t i = 0; i < length; ++i) {
        switch (hexstring[i]) {
            case 'a':
            case 'b':
            case 'c':
            case 'd':
            case 'e':
            case 'f':
                byte = (hexstring[i] - 'a' + 10) << 4;
                break;
            case 'A':
            case 'B':
            case 'C':
            case 'D':
            case 'E':
            case 'F':
                byte = (hexstring[i] - 'A' + 10) << 4;
                break;
            case '0':
            case '1':
            case '2':
            case '3':
            case '4':
            case '5':
            case '6':
            case '7':
            case '8':
            case '9':
                byte = (hexstring[i] - '0') << 4;
                break;
            default:
                MORDOR_THROW_EXCEPTION(std::invalid_argument("hexstring"));
        }
        ++i;
        switch (hexstring[i]) {
            case 'a':
            case 'b':
            case 'c':
            case 'd':
            case 'e':
            case 'f':
                byte |= hexstring[i] - 'a' + 10;
                break;
            case 'A':
            case 'B':
            case 'C':
            case 'D':
            case 'E':
            case 'F':
                byte |= hexstring[i] - 'A' + 10;
                break;
            case '0':
            case '1':
            case '2':
            case '3':
            case '4':
            case '5':
            case '6':
            case '7':
            case '8':
            case '9':
                byte |= hexstring[i] - '0';
                break;
            default:
                MORDOR_THROW_EXCEPTION(std::invalid_argument("hexstring"));
        }
        *buf++ = byte;
    }
}

std::string
dataFromHexstring(const char *hexstring, size_t length)
{
    if (length % 2 != 0)
        MORDOR_THROW_EXCEPTION(std::invalid_argument("length"));
    if (length == 0)
        return std::string();
    std::string result;
    result.resize(length / 2);
    dataFromHexstring(hexstring, length, &result[0]);
    return result;
}

std::string
dataFromHexstring(const std::string &hexstring)
{
    return dataFromHexstring(hexstring.c_str(), hexstring.size());
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

void
replace(std::string &str, char find, const std::string &replaceWith)
{
    size_t index = str.find(find);
    while (index != std::string::npos) {
        str = str.substr(0, index) + replaceWith + str.substr(index + 1);
        index = str.find(find, index + replaceWith.size());
    }
}

void
replace(std::string &str, const std::string &find, const std::string &replaceWith)
{
    size_t index = str.find(find);
    while (index != std::string::npos) {
        str = str.substr(0, index) + replaceWith + str.substr(index + find.size());
        index = str.find(find, index + replaceWith.size());
    }
}

std::vector<std::string>
split(const std::string &str, char delim, size_t max)
{
    MORDOR_ASSERT(max > 1);
    std::vector<std::string> result;
    if (str.empty())
        return result;

    size_t last = 0;
    size_t pos = str.find(delim);
    while (pos != std::string::npos) {
        result.push_back(str.substr(last, pos - last));
        last = pos + 1;
        if (--max == 1)
            break;
        pos = str.find(delim, last);
    }
    result.push_back(str.substr(last));
    return result;
}

std::vector<std::string>
split(const std::string &str, const char *delims, size_t max)
{
    MORDOR_ASSERT(max > 1);
    std::vector<std::string> result;
    if (str.empty())
        return result;

    size_t last = 0;
    size_t pos = str.find_first_of(delims);
    while (pos != std::string::npos) {
        result.push_back(str.substr(last, pos - last));
        last = pos + 1;
        if (--max == 1)
            break;
        pos = str.find_first_of(delims, last);
    }
    result.push_back(str.substr(last));
    return result;
}

static bool endsWith(const std::string &string, const std::string &suffix)
{
    return string.size() >= suffix.size() &&
        strnicmp(string.c_str() + string.size() - suffix.size(),
            suffix.c_str(), suffix.size()) == 0;
}

namespace {
struct Suffix
{
    std::string suffix;
    unsigned long long multiplier;
};
}

unsigned long long stringToMicroseconds(const std::string &string)
{
    static const Suffix suffixes[] = {
        { "microseconds", 1ull },
        { "us", 1ull },
        { "milliseconds", 1000ull },
        { "ms", 1000ull },
        { "seconds", 1000000ull },
        { "minutes", 60 * 1000000ull },
        { "m", 60 * 1000000ull },
        { "hours", 60 * 60 * 1000000ull },
        { "h", 60 * 60 * 1000000ull },
        { "days", 24 * 60 * 60 * 1000000ull },
        { "d", 24 * 60 * 60 * 1000000ull },
        // s needs to go at the bottom since we're just suffix matching, and it
        // would give a false positive for "minutes", etc.
        { "s", 1000000ull }
    };

    std::string copy(string);
    unsigned long long multiplier = 1ull;

    // Strip leading whitespace
    while (copy.size() > 1 && copy[0] == ' ')
        copy = copy.substr(1);
    // Strip trailing whitespace
    while (copy.size() > 1 && copy[copy.size() -1] == ' ')
        copy.resize(copy.size() - 1);

    for (size_t i = 0; i < sizeof(suffixes)/sizeof(suffixes[0]); ++i) {
        if (endsWith(copy, suffixes[i].suffix)) {
            multiplier = suffixes[i].multiplier;
            copy.resize(copy.size() - suffixes[i].suffix.size());
            break;
        }
    }

    // Strip whitespace between the number and the units
    while (copy.size() > 1 && copy[copy.size() -1] == ' ')
        copy.resize(copy.size() - 1);

    // If there's a decimal point, use floating point arithmetic
    if (copy.find('.') != std::string::npos)
        return (unsigned long long)(multiplier *
            boost::lexical_cast<double>(copy));
    else
        return multiplier * boost::lexical_cast<unsigned long long>(copy);
}

#ifdef WINDOWS
static DWORD g_wcFlags = WC_ERR_INVALID_CHARS;
static DWORD g_mbFlags = MB_ERR_INVALID_CHARS;

std::string
toUtf8(const utf16char *str, size_t len)
{
    if (len == (size_t)~0)
        len = wcslen(str);
    MORDOR_ASSERT(len < 0x80000000u);
    std::string result;
    if (len == 0)
        return result;
    int ret = WideCharToMultiByte(CP_UTF8, g_wcFlags, str, (int)len, NULL, 0, NULL, NULL);
    MORDOR_ASSERT(ret >= 0);
    if (ret == 0) {
        if (lastError() == ERROR_INVALID_FLAGS) {
            g_wcFlags = 0;
            ret = WideCharToMultiByte(CP_UTF8, g_wcFlags, str, (int)len, NULL, 0, NULL, NULL);
            MORDOR_ASSERT(ret >= 0);
        }
        if (ret == 0)
            MORDOR_THROW_EXCEPTION_FROM_LAST_ERROR_API("WideCharToMultiByte");
    }
    result.resize(ret);
    ret = WideCharToMultiByte(CP_UTF8, g_wcFlags, str, (int)len, &result[0], ret, NULL, NULL);
    MORDOR_ASSERT(ret >= 0);
    if (ret == 0)
        MORDOR_THROW_EXCEPTION_FROM_LAST_ERROR_API("WideCharToMultiByte");
    MORDOR_ASSERT(ret == result.size());

    return result;
}

std::string
toUtf8(const std::wstring &str)
{
    MORDOR_ASSERT(str.size() < 0x80000000u);
    return toUtf8(str.c_str(), str.size());
}

utf16string
toUtf16(const char *str, size_t len)
{
    if (len == (size_t)~0)
        len = strlen(str);
    MORDOR_ASSERT(len < 0x80000000u);
    utf16string result;
    if (len == 0)
        return result;
    int ret = MultiByteToWideChar(CP_UTF8, g_mbFlags, str, (int)len, NULL, 0);
    MORDOR_ASSERT(ret >= 0);
    if (ret == 0) {
        if (lastError() == ERROR_INVALID_FLAGS) {
            g_mbFlags = 0;
            ret = MultiByteToWideChar(CP_UTF8, g_mbFlags, str, (int)len, NULL, 0);
            MORDOR_ASSERT(ret >= 0);
        }
        if (ret == 0)
            MORDOR_THROW_EXCEPTION_FROM_LAST_ERROR_API("MultiByteToWideChar");
    }
    result.resize(ret);
    ret = MultiByteToWideChar(CP_UTF8, g_mbFlags, str, (int)len, &result[0], ret);
    if (ret == 0)
        MORDOR_THROW_EXCEPTION_FROM_LAST_ERROR_API("MultiByteToWideChar");
    MORDOR_ASSERT(ret == result.size());

    return result;
}

utf16string
toUtf16(const std::string &str)
{
    MORDOR_ASSERT(str.size() < 0x80000000u);
    return toUtf16(str.c_str(), str.size());
}
#elif defined (OSX)

std::string
toUtf8(CFStringRef string)
{
    const char *bytes = CFStringGetCStringPtr(string, kCFStringEncodingUTF8);
    if (bytes)
        return bytes;
    std::string result;
    CFIndex length = CFStringGetLength(string);
    length = CFStringGetMaximumSizeForEncoding(length, kCFStringEncodingUTF8);
    result.resize(length);
    if (!CFStringGetCString(string, &result[0], length, kCFStringEncodingUTF8)) {
        MORDOR_NOTREACHED();
    }
    result.resize(strlen(result.c_str()));
    return result;
}

utf16string
toUtf16(const char * str, size_t length)
{
    utf16string result;
    if (length == 0u)
        return result;
    ScopedCFRef<CFStringRef> cfUtf8Str = CFStringCreateWithBytesNoCopy(NULL,
        (const UInt8 *)str, (CFIndex)length, kCFStringEncodingUTF8, false,
        kCFAllocatorNull);
    if (!cfUtf8Str)
        MORDOR_THROW_EXCEPTION(InvalidUnicodeException());
#if MORDOR_BYTE_ORDER == MORDOR_LITTLE_ENDIAN
    ScopedCFRef<CFDataRef> cfUtf16Data = CFStringCreateExternalRepresentation(
        NULL, cfUtf8Str, kCFStringEncodingUTF16LE, 0);
#elif MORDOR_BYTE_ORDER == MORDOR_BIG_ENDIAN
    ScopedCFRef<CFDataRef> cfUtf16Data = CFStringCreateExternalRepresentation(
        NULL, cfUtf8Str, kCFStringEncodingUTF16BE, 0);
#endif
    MORDOR_ASSERT(cfUtf16Data);
    MORDOR_ASSERT(CFDataGetLength(cfUtf16Data) % sizeof(utf16char) == 0);
    result.resize(CFDataGetLength(cfUtf16Data) / sizeof(utf16char));
    CFDataGetBytes(cfUtf16Data, CFRangeMake(0,CFDataGetLength(cfUtf16Data)),
        (UInt8 *)&result[0]);
    return result;
}

utf16string
toUtf16(const std::string &str)
{
    return toUtf16(str.c_str(), str.size());
}

#elif defined(HAVE_ICONV)

namespace {

class Iconv {
    iconv_t m_iconv;
public:
    Iconv(const char* from, const char* to)
        : m_iconv(iconv_open(to, from))
    {
        MORDOR_ASSERT(m_iconv != (iconv_t)-1);
    }
    ~Iconv() {
        iconv_close(m_iconv);
    }
    size_t operator()(char** inbuf, size_t* inlen, char** outbuf, size_t* outlen) {
        return iconv(m_iconv, inbuf, inlen, outbuf, outlen);
    }
};
}

utf16string
toUtf16(const char *str, size_t len)
{
    utf16string result;
    if (len == 0u)
        return result;
    result.resize(len);        // way enough (paired surrogate also)
    size_t out_left = len * sizeof(utf16string::value_type);
    char *out_buf = (char *)&result[0];
    Iconv conv("UTF-8", "UTF-16LE");
    size_t n = conv((char **)&str, &len, &out_buf, &out_left);
    if (n == (size_t)-1) {
        MORDOR_ASSERT(errno != E2BIG);
        MORDOR_THROW_EXCEPTION(InvalidUnicodeException());
    }
    MORDOR_ASSERT(out_left % sizeof(utf16string::value_type) == 0);
    result.resize(result.size() - out_left/sizeof(utf16string::value_type));
    return result;
}

utf16string
toUtf16(const std::string &str)
{
    return toUtf16(str.data(), str.size());
}

#endif

std::string
toUtf8(utf16char character)
{
    return toUtf8((utf32char)character);
}

std::string
toUtf8(utf32char character)
{
    MORDOR_ASSERT(character <= 0x10ffff);
    std::string result;
    if (character <= 0x7f) {
        result.append(1, (char)character);
    } else if (character <= 0x7ff) {
        result.resize(2);
        result[0] = 0xc0 | ((character >> 6) & 0x1f);
        result[1] = 0x80 | (character & 0x3f);
    } else if (character <= 0xffff) {
        result.resize(3);
        result[0] = 0xe0 | ((character >> 12) & 0xf);
        result[1] = 0x80 | ((character >> 6) & 0x3f);
        result[2] = 0x80 | (character & 0x3f);
    } else {
        result.resize(4);
        result[0] = 0xf0 | ((character >> 18) & 0x7);
        result[1] = 0x80 | ((character >> 12) & 0x3f);
        result[2] = 0x80 | ((character >> 6) & 0x3f);
        result[3] = 0x80 | (character & 0x3f);
    }
    return result;
}

utf32char
toUtf32(utf16char highSurrogate, utf16char lowSurrogate)
{
    MORDOR_ASSERT(isHighSurrogate(highSurrogate));
    MORDOR_ASSERT(isLowSurrogate(lowSurrogate));
    return ((((utf32char)highSurrogate - 0xd800) << 10) | ((utf32char)lowSurrogate - 0xdc00)) + 0x10000;
}

std::string
toUtf8(utf16char highSurrogate, utf16char lowSurrogate)
{
    return toUtf8(toUtf32(highSurrogate, lowSurrogate));
}

bool isHighSurrogate(utf16char character)
{
    return character >= 0xd800 && character <= 0xdbff;
}

bool isLowSurrogate(utf16char character)
{
    return character >= 0xdc00 && character <= 0xdfff;
}

bool
caseinsensitiveless::operator ()(const std::string &lhs, const std::string &rhs) const
{
    return stricmp(lhs.c_str(), rhs.c_str()) < 0;
}

std::ostream &operator <<(std::ostream &os, const charslice &slice)
{
    for (size_t i = 0; i < slice.m_len; ++i) {
        os.put(slice.m_slice[i]);
    }
    return os;
}

}
