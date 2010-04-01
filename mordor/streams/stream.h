#ifndef __MORDOR_STREAM_H__
#define __MORDOR_STREAM_H__
// Copyright (c) 2009 - Mozy, Inc.

#include <string>

#include <boost/noncopyable.hpp>
#include <boost/shared_ptr.hpp>

#include "mordor/assert.h"
#include "mordor/predef.h"
#include "buffer.h"

namespace Mordor {

/// @brief Byte-oriented stream
/// @details
/// Stream is the main interface for dealing with a stream of data.  It can
/// represent half or full duplex streams, as well as random access streams
/// or sequential only streams.  By default, a Stream advertises that it
/// cannot support any operations, and calling any of them will result in an
/// assertion.  close() and flush() are always safe to call.
///
/// Streams are not thread safe, except for cancelRead() and cancelWrite(),
/// and it is safe to call read() at the same time as write() (assuming the
/// Stream supports both, and don't supportSeek()).  read() and write() are
/// @b not re-entrant.
class Stream : boost::noncopyable
{
public:
    typedef boost::shared_ptr<Stream> ptr;

    /// Flags for which end of a Stream to close
    enum CloseType {
        /// Neither (should not be passed to close(); only useful for keeping
        /// track of the current state of a Stream)
        NONE  = 0x00,
        /// Further reads from this Stream should fail;
        /// further writes on the "other" end of this Stream should fail.
        READ  = 0x01,
        /// Further writes to this Stream should fail;
        /// further reads on the "other" end of this Stream should receive EOF.
        WRITE = 0x02,
        /// The default; closes both the read and write directions
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

    /// @return If it is valid to call close() with READ or WRITE
    virtual bool supportsHalfClose() { return false; }
    /// @return If it is valid to call read()
    virtual bool supportsRead() { return false; }
    /// @return If it is valid to call write()
    virtual bool supportsWrite() { return false; }
    /// @return If it is valid to call seek() with any parameters
    virtual bool supportsSeek() { return false; }
    /// @note
    /// If supportsSeek(), then the default implementation supportsTell().
    /// @return If it is valid to call tell() or call seek(0, CURRENT)
    virtual bool supportsTell() { return supportsSeek(); }
    /// @return If it is valid to call size()
    virtual bool supportsSize() { return false; }
    /// @return If it is valid to call truncate()
    virtual bool supportsTruncate() { return false; }
    /// @return If it is valid to call find()
    virtual bool supportsFind() { return false; }
    /// @return If it is valid to call unread()
    virtual bool supportsUnread() { return false; }

    /// @brief Gracefully close the Stream
    /// @details
    /// It is valid to call close() multiple times without error.
    /// @param type Which ends of the stream to close
    /// @pre type == BOTH || type == READ || type == WRITE
    /// @pre if (type != BOTH) supportsHalfClose()
    virtual void close(CloseType type = BOTH) {}

    /// @brief Read data from the Stream
    /// @details
    /// read() is allowed to return less than length, even if there is more data
    /// available. A return value of 0 is the @b only reliable method of
    /// detecting EOF. If an exception is thrown, you can be assured that
    /// nothing was read from the underlying implementation.
    /// @param buffer The Buffer to read in to
    /// @param length The maximum amount to read
    /// @return The amount actually read
    /// @pre supportsRead()
    virtual size_t read(Buffer &buffer, size_t length);
    /// @copydoc read(Buffer &, size_t)
    /// @brief Convenience function to call read() without first creating a
    /// Buffer
    /// @note
    /// A default implementation is provided which calls
    /// read(Buffer &, size_t).  Only implement if it can be more efficient
    /// than creating a new Buffer.
    virtual size_t read(void *buffer, size_t length);
    /// @brief Cancels a read that is currently blocked
    /// @details
    /// Must be called from a different Fiber/Thread than the one that is
    /// blocked. This is safe to call on any Stream, but may not have any
    /// effect.
    virtual void cancelRead() {}

    /// @brief Write data to the Stream
    /// @details
    /// write() is allowed to return less than length. If is @b not allowed to
    /// return 0. If an exception is thrown, you can be assured that nothing
    /// was written to the underlying implementation.
    /// @pre @c buffer.readAvailable() >= @c length
    /// @return The amount actually written
    /// @pre supportsWrite()
    virtual size_t write(const Buffer &buffer, size_t length);

    /// @copydoc write(const Buffer &, size_t)
    /// @brief Convenience function to call write() without first creating a
    /// Buffer
    /// @note
    /// A default implementation is provided which calls
    /// write(const Buffer &, size_t).  Only implement if it can be more efficient
    /// than creating a new Buffer.
    virtual size_t write(const void *buffer, size_t length);
    /// @copydoc write(const Buffer &, size_t)
    /// @brief
    /// Convenience function to call write() with a null-terminated string
    /// @note
    /// A default implementation is provided which calls
    /// write(const void *, size_t).  Cannot be overridden.
    size_t write(const char *string);
    /// @brief Cancels a write that is currently blocked
    /// @details
    /// Must be called from a different Fiber/Thread than the one that is
    /// blocked. This is safe to call on any Stream, but may not have any
    /// effect.
    virtual void cancelWrite() {}

    /// @brief Change the current stream pointer
    /// @param offset Where to seek to
    /// @param anchor Where to seek from
    /// @exception std::invalid_argument The resulting position would be negative
    /// @return The new stream pointer position
    /// @pre supportsSeek() || supportsTell()
    virtual long long seek(long long offset, Anchor anchor = BEGIN) { MORDOR_NOTREACHED(); }

    /// @brief Convenience method to call seek(0, CURRENT)
    /// @note Cannot be overridden.
    /// @return The current stream pointer position
    /// @pre supportsTell()
    long long tell() { return seek(0, CURRENT); }

    /// @brief Return the size of the stream
    /// @return The size of the stream
    /// @pre supportsSize()
    virtual long long size() { MORDOR_NOTREACHED(); }

    /// @brief Truncate the stream
    /// @param size The new size of the stream
    /// @pre supportsTruncate()
    virtual void truncate(long long size) { MORDOR_NOTREACHED(); }

    /// @brief Flush the stream

    /// flush() ensures that nothing is left in internal buffers, and has been
    /// fully written to the underlying implementation.  It is safe to call
    /// flush() on any Stream.  In some cases, flush() may not return until
    /// all data has been read from the other end of a pipe-like Stream.
    /// @param flushParent Also flush() a parent stream(), if there is one
    virtual void flush(bool flushParent = true) {}

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
    /// @pre supportsFind()
    virtual ptrdiff_t find(char delimiter, size_t sanitySize = ~0, bool throwIfNotFound = true) { MORDOR_NOTREACHED(); }
    virtual ptrdiff_t find(const std::string &delimiter, size_t sanitySize = ~0, bool throwIfNotFound = true) { MORDOR_NOTREACHED(); }
    //@}

    /// @brief Convenience function for calling find() then read(), and return
    /// the results in a std::string
    /// @details
    /// getDelimited() is provided so that users of the Stream class do not
    /// need to be aware of if or where a Stream that supportsFind() is in
    /// the stack of FilterStreams, and forcing all FilterStreams to deal with
    /// it. Instead, the operaton is broken up into find() and read().
    /// FilterStreams just pass the find() on to the parent, and do their stuff
    /// on the actual data as they see it in the read().
    /// @note Cannot be overridden.
    /// @param delimiter The byte to look for
    /// @param eofIsDelimiter Instead of throwing an exception if the delimiter
    /// is not found, return the remainder of the stream.
    /// @return The data from the current stream position up to and including
    /// the delimiter
    /// @pre supportsFind() && supportsRead()
    std::string getDelimited(char delimiter = '\n', bool eofIsDelimiter = false);
    std::string getDelimited(const std::string &delimiter, bool eofIsDelimiter = false);

    /// @brief Return data to the stream to be read again

    /// @param buffer The data to return
    /// @param length How much data to return
    /// @pre @c buffer.readAvailable() >= @c length
    /// @pre supportsUnread()
    virtual void unread(const Buffer &buffer, size_t length) { MORDOR_NOTREACHED(); }

protected:
    size_t read(Buffer &buffer, size_t length, bool coalesce);
    size_t write(const Buffer &buffer, size_t length, bool coalesce);
};

}

#endif
