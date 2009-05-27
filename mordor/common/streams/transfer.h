#ifndef __TRANSFER_H__
#define __TRANSFER_H__
// Copyright (c) 2009 - Decho Corp.

class Stream;

void transferStream(Stream *src, Stream *dst, long long toTransfer,
                    long long *totalRead = 0, long long *totalWritten = 0);

void transferStream(Stream *src, Stream *dst,
                    long long *totalRead = 0, long long *totalWritten = 0);

#endif
