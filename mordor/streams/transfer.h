#ifndef __MORDOR_TRANSFER_STREAM_H__
#define __MORDOR_TRANSFER_STREAM_H__
// Copyright (c) 2009 - Decho Corp.

#include "stream.h"

namespace Mordor {

unsigned long long transferStream(Stream &src, Stream &dst,
                                  unsigned long long toTransfer = ~0ull);

inline unsigned long long transferStream(Stream::ptr src, Stream &dst,
                                         unsigned long long toTransfer = ~0ull)
{ return transferStream(*src.get(), dst, toTransfer); }
inline unsigned long long transferStream(Stream &src, Stream::ptr dst,
                                         unsigned long long toTransfer = ~0ull)
{ return transferStream(src, *dst.get(), toTransfer); }
inline unsigned long long transferStream(Stream::ptr src, Stream::ptr dst,
                                         unsigned long long toTransfer = ~0ull)
{ return transferStream(*src.get(), *dst.get(), toTransfer); }

}

#endif
