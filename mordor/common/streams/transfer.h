#ifndef __TRANSFER_H__
#define __TRANSFER_H__
// Copyright (c) 2009 - Decho Corp.

#include "stream.h"

void transferStream(Stream& src, Stream& dst, unsigned long long toTransfer,
                    unsigned long long *totalRead = 0, unsigned long long *totalWritten = 0);

void transferStream(Stream::ptr src, Stream& dst, unsigned long long toTransfer,
                    unsigned long long *totalRead = 0, unsigned long long *totalWritten = 0);
void transferStream(Stream &src, Stream::ptr dst, unsigned long long toTransfer,
                    unsigned long long *totalRead = 0, unsigned long long *totalWritten = 0);
void transferStream(Stream::ptr src, Stream::ptr dst, unsigned long long toTransfer,
                    unsigned long long *totalRead = 0, unsigned long long *totalWritten = 0);

void transferStream(Stream &src, Stream &dst,
                    unsigned long long *totalRead = 0, unsigned long long *totalWritten = 0);
void transferStream(Stream::ptr src, Stream &dst,
                    unsigned long long *totalRead = 0, unsigned long long *totalWritten = 0);
void transferStream(Stream &src, Stream::ptr dst,
                    unsigned long long *totalRead = 0, unsigned long long *totalWritten = 0);
void transferStream(Stream::ptr src, Stream::ptr dst,
                    unsigned long long *totalRead = 0, unsigned long long *totalWritten = 0);

#endif
