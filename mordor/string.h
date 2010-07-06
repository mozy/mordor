#ifndef __MORDOR_STRING_H__
#define __MORDOR_STRING_H__
// Copyright (c) 2009 Mozy, Inc.

#include <ostream>
#include <string>
#include <vector>

#include "version.h"

#ifdef OSX
#include <CoreFoundation/CFString.h>
#endif

namespace Mordor {

std::string base64decode(const std::string &src);
std::string base64encode(const std::string &data);
std::string base64encode(const void *data, size_t len);

// Returns result in hex
std::string md5(const std::string &data);
std::string sha1(const std::string &data);
// Returns result in blob
std::string md5sum(const std::string &data);
std::string md5sum(const void *data, size_t len);
std::string sha1sum(const std::string &data);
std::string sha1sum(const void *data, size_t len);
std::string hmacMd5(const std::string &text, const std::string &key);
std::string hmacSha1(const std::string &text, const std::string &key);

// Output must be of size len * 2, and will *not* be null-terminated
void hexstringFromData(const void *data, size_t len, char *output);
std::string hexstringFromData(const void *data, size_t len);
std::string hexstringFromData(const std::string &data);

void replace(std::string &str, char find, char replaceWith);
void replace(std::string &str, char find, const std::string &replaceWith);
void replace(std::string &str, const std::string &find, const std::string &replaceWith);

std::vector<std::string> split(const std::string &str, char delim, size_t max = ~0);
std::vector<std::string> split(const std::string &str, const char *delims, size_t max = ~0);

#ifdef WINDOWS
std::string toUtf8(const wchar_t *str, size_t len = ~0);
std::string toUtf8(const std::wstring &str);
std::wstring toUtf16(const char *str, size_t len = ~0);
std::wstring toUtf16(const std::string &str);
#elif defined (OSX)
std::string toUtf8(CFStringRef string);
#endif
std::string toUtf8(wchar_t character);
std::string toUtf8(int character);
int toUtf32(wchar_t highSurrogate, wchar_t lowSurrogate);
std::string toUtf8(wchar_t highSurrogate, wchar_t lowSurrogate);
bool isHighSurrogate(wchar_t character);
bool isLowSurrogate(wchar_t character);

struct caseinsensitiveless
{
    bool operator()(const std::string& lhs, const std::string& rhs) const;
};

struct charslice
{
    friend std::ostream &operator <<(std::ostream &os, const charslice &slice);
public:
    charslice(const char *slice, size_t len)
        : m_slice(slice),
          m_len(len)
    {}

private:
    const char *m_slice;
    size_t m_len;
};

std::ostream &operator <<(std::ostream &os, const charslice &slice);

}

#endif
