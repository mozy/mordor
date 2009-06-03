#ifndef __TRANSFER_H__
#define __TRANSFER_H__
// Copyright (c) 2009 - Decho Corp.

#include "stream.h"

void transferStream(Stream *src, Stream *dst, long long toTransfer,
                    long long *totalRead = 0, long long *totalWritten = 0);

void transferStream(Stream::ptr src, Stream *dst, long long toTransfer,
                    long long *totalRead = 0, long long *totalWritten = 0);
void transferStream(Stream *src, Stream::ptr dst, long long toTransfer,
                    long long *totalRead = 0, long long *totalWritten = 0);
void transferStream(Stream::ptr src, Stream::ptr dst, long long toTransfer,
                    long long *totalRead = 0, long long *totalWritten = 0);

void transferStream(Stream *src, Stream *dst,
                    long long *totalRead = 0, long long *totalWritten = 0);
void transferStream(Stream::ptr src, Stream *dst,
                    long long *totalRead = 0, long long *totalWritten = 0);
void transferStream(Stream *src, Stream::ptr dst,
                    long long *totalRead = 0, long long *totalWritten = 0);
void transferStream(Stream::ptr src, Stream::ptr dst,
                    long long *totalRead = 0, long long *totalWritten = 0);

#endif
