#ifndef __MORDOR_TRANSFER_STREAM_H__
#define __MORDOR_TRANSFER_STREAM_H__
// Copyright (c) 2009 - Decho Corp.

#include "stream.h"

namespace Mordor {

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

}

#endif
