#ifndef __MORDOR_STRING_H__
#define __MORDOR_STRING_H__
// Copyright (c) 2009 Mozy, Inc.

#include <ostream>
#include <string>
#include <vector>

#ifndef BOOST_TYPEOF_SILENT
#define BOOST_TYPEOF_SILENT
#endif
#include <boost/typeof/typeof.hpp>

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

namespace detail
{
    template <class T>
    typename boost::enable_if_c<sizeof(T) == 2, wchar_t>::type utf16func();
    template <class T>
    typename boost::disable_if_c<sizeof(T) == 2, unsigned short>::type utf16func();
    template <class T>
    typename boost::enable_if_c<sizeof(T) == 4, wchar_t>::type utf32func();
    template <class T>
    typename boost::disable_if_c<sizeof(T) == 4, unsigned int>::type utf32func();
};

typedef BOOST_TYPEOF(detail::utf16func<wchar_t>()) utf16char;
typedef BOOST_TYPEOF(detail::utf32func<wchar_t>()) utf32char;

typedef std::basic_string<utf16char> utf16string;
typedef std::basic_string<utf32char> utf32string;

#ifdef WINDOWS
std::string toUtf8(const utf16char *str, size_t len = ~0);
std::string toUtf8(const utf16string &str);
utf16string toUtf16(const char *str, size_t len = ~0);
utf16string toUtf16(const std::string &str);
#elif defined (OSX)
std::string toUtf8(CFStringRef string);
utf16string toUtf16(const char *str, size_t len = ~0);
utf16string toUtf16(const std::string &str);
#endif
std::string toUtf8(utf16char character);
std::string toUtf8(utf32char character);
utf32char toUtf32(utf16char highSurrogate, utf16char lowSurrogate);
std::string toUtf8(utf16char highSurrogate, utf16char lowSurrogate);
bool isHighSurrogate(utf16char character);
bool isLowSurrogate(utf16char character);

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
