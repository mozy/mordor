#ifndef __MORDOR_TRANSFER_STREAM_H__
#define __MORDOR_TRANSFER_STREAM_H__
// Copyright (c) 2009 - Mozy, Inc.

#include "stream.h"

namespace Mordor {

enum ExactLength
{
    /// If toTransfer == ~0ull, use EOF, otherwise EXACT
    INFER,
    /// If toTransfer bytes can't be read, throw UnexpectedEofException
    EXACT,
    /// Transfer as many bytes as possible until EOF is hit
    UNTILEOF
};

unsigned long long transferStream(Stream &src, Stream &dst,
                                  unsigned long long toTransfer = ~0ull,
                                  ExactLength exactLength = INFER);

inline unsigned long long transferStream(Stream::ptr src, Stream &dst,
                                         unsigned long long toTransfer = ~0ull,
                                         ExactLength exactLength = INFER)
{ return transferStream(*src.get(), dst, toTransfer, exactLength); }
inline unsigned long long transferStream(Stream &src, Stream::ptr dst,
                                         unsigned long long toTransfer = ~0ull,
                                         ExactLength exactLength = INFER)
{ return transferStream(src, *dst.get(), toTransfer, exactLength); }
inline unsigned long long transferStream(Stream::ptr src, Stream::ptr dst,
                                         unsigned long long toTransfer = ~0ull,
                                         ExactLength exactLength = INFER)
{ return transferStream(*src.get(), *dst.get(), toTransfer, exactLength); }

}

#endif
