// Copyright (c) 2010 - Mozy, Inc.

#include "filter.h"

#include "mordor/assert.h"

namespace Mordor {

long long
MutatingFilterStream::seek(long long offset, Anchor anchor)
{
    MORDOR_NOTREACHED();
}

long long
MutatingFilterStream::size()
{
    MORDOR_NOTREACHED();
}

void
MutatingFilterStream::truncate(long long size)
{
    MORDOR_NOTREACHED();
}

ptrdiff_t
MutatingFilterStream::find(char delim, size_t sanitySize, bool throwIfNotFound)
{
    MORDOR_NOTREACHED();
}

ptrdiff_t
MutatingFilterStream::find(const std::string &str, size_t sanitySize,
    bool throwIfNotFound)
{
    MORDOR_NOTREACHED();
}

void
MutatingFilterStream::unread(const Buffer &b, size_t len)
{
    MORDOR_NOTREACHED();
}

}
