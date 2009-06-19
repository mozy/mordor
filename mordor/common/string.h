#ifndef __MORDOR_STRING_H__
#define __MORDOR_STRING_H__
// Copyright (c) 2009 Decho Corp.

#include <string>

std::string base64decode(std::string src);
std::string base64encode(const std::string& src);
std::string base64encode(const void* src, size_t len);

void hexstringFromData(const void *data, size_t len, void *output);
std::string hexstringFromData(const void *data, size_t len);

#endif
