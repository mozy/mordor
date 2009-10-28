#ifndef __MORDOR_STREAM_H__
#define __MORDOR_STREAM_H__
// Copyright (c) 2009 - Decho Corp.

#include <string>

#include <boost/noncopyable.hpp>
#include <boost/shared_ptr.hpp>

#include "mordor/assert.h"
#include "mordor/predef.h"
#include "buffer.h"

namespace Mordor {

/// Byte-oriented stream

/// Stream is the main interface for dealing with a stream of data.  It can
/// represent half or full duplex streams, as well as random access streams
/// or sequential only streams.  By default, a Stream advertises that it
/// cannot support any operations, and calling any of them will result in an
/// assertion.  close() and flush() are always safe to call.
class Stream : boost::noncopyable
{
public:
    typedef boost::shared_ptr<Stream> ptr;

    /// Flags for which end of a Stream to close
    enum CloseType {
        /// Neither
        NONE  = 0x00,
        /// Further reads from this Stream should fail;
        /// further writes on the "other" end of this Stream should fail.
        READ  = 0x01,
        /// Further writes to this Stream should fail;
        /// further reads on the "other" end of this Stream should receive EOF.
        WRITE = 0x02,
        /// The default, and what is always interpreted if the underlying
        /// implementation does not support half-open
        BOTH  = 0x03
    };

    /// Flags for where to seek from
    enum Anchor {
        /// Relative to the beginning of the stream
        BEGIN,
        /// Relative to the current position in the stream
        CURRENT,
        /// Relative to the end of the stream
        END
    };

public:
    /// Cleans up the underlying implementation, possibly by ungracefully
    /// closing it.
    virtual ~Stream() {}

    virtual bool supportsRead() { return false; }
    virtual bool supportsWrite() { return false; }
    virtual bool supportsSeek() { return false; }
    virtual bool supportsSize() { return false; }
    virtual bool supportsTruncate() { return false; }
    virtual bool supportsFind() { return false; }
    virtual bool supportsUnread() { return false; }

    /// @brief Gracefully close the Stream

    /// It is valid to call close() multiple times without error
    /// @param type Which ends of the stream to close
    virtual void close(CloseType type = BOTH) {}

    /// @brief Read data from the Stream

    /// read() is allowed to return less than length, even if there is more data
    /// available. A return value of 0 is the @b only reliable method of
    /// detecting EOF. If an exception is thrown, you can be assured that
    /// nothing was read from the underlying implementation.
    /// @param buffer The Buffer to read in to
    /// @param length The maximum amount to read
    /// @return The amount actually read
    virtual size_t read(Buffer &buffer, size_t length) { MORDOR_NOTREACHED(); }

    /// @brief Write data to the Stream

    /// write() is allowed to return less than length. If is @b not allowed to
    /// return 0. If an exception is thrown, you can be assured that nothing
    /// was written to the underlying implementation.
    /// @pre @c buffer.readAvailable() >= @c length
    /// @return The amount actually written
    virtual size_t write(const Buffer &buffer, size_t length) { MORDOR_NOTREACHED(); }

    /// @brief Change the current stream pointer

    /// @param offset Where to seek to
    /// @param anchor Where to seek from
    /// @exception std::invalid_argument The resulting position would be negative
    /// @return The new stream pointer position
    virtual long long seek(long long offset, Anchor anchor) { MORDOR_NOTREACHED(); }

    /// @brief Return the size of the stream
    /// @return The size of the stream
    virtual long long size() { MORDOR_NOTREACHED(); }

    /// @brief Truncate the stream
    /// @param size The new size of the stream
    virtual void truncate(long long size) { MORDOR_NOTREACHED(); }

    /// @brief Flush the stream

    /// flush() ensures that nothing is left in internal buffers, and has been
    /// fully written to the underlying implementation.  It is safe to call
    /// flush() on any Stream.  In some cases, flush() may not return until
    /// all data has been read from the other end of a pipe-like Stream.
    virtual void flush() {}

    //@{
    /// @brief Find a delimiter by looking ahead in the stream
    /// @param delimiter The byte to look for
    /// @param sanitySize The maximum amount to look ahead before throwing an
    /// exception
    /// @param throwIfNotFound Instead of throwing an exception on error, it
    /// will return a negative number.  Negate and subtract 1 to find out how
    /// much buffered data is available before hitting the error
    /// @exception BufferOverflowException @c delimiter was not found before
    /// @c sanitySize
    /// @exception UnexpectedEofException EOF was reached without finding
    /// @c delimiter
    /// @return Offset from the current stream position of the found
    /// @c delimiter
    virtual ptrdiff_t find(char delimiter, size_t sanitySize = ~0, bool throwIfNotFound = true) { MORDOR_NOTREACHED(); }
    virtual ptrdiff_t find(const std::string &delimiter, size_t sanitySize = ~0, bool throwIfNotFound = true) { MORDOR_NOTREACHED(); }
    //@}

    /// @brief Return data to the stream to be read again

    /// @param buffer The data to return
    /// @param length How much data to return
    /// @pre @c buffer.readAvailable() >= @c length
    virtual void unread(const Buffer &buffer, size_t length) { MORDOR_NOTREACHED(); }

    // Convenience functions - do *not* implement in FilterStream, so that
    // filters do not need to implement these
    virtual size_t write(const void *buffer, size_t length);
    size_t write(const char *sz);

    std::string getDelimited(char delim = '\n', bool eofIsDelimiter = false);
};

}

#endif
