#ifndef __MORDOR_STRING_H__
#define __MORDOR_STRING_H__
// Copyright (c) 2009 Decho Corp.

#include <ostream>
#include <string>

std::string base64decode(std::string src);
std::string base64encode(const std::string& src);
std::string base64encode(const void* src, size_t len);

void hexstringFromData(const void *data, size_t len, void *output);
std::string hexstringFromData(const void *data, size_t len);

void replace(std::string &str, char find, char replaceWith);

#ifdef WINDOWS
std::string toUtf8(const wchar_t *str, size_t len = ~0);
std::string toUtf8(const std::wstring &str);
#endif

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

#endif
