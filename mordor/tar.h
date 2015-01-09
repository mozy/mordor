#ifndef __MORDOR_TAR_H__
#define __MORDOR_TAR_H__
// Copyright (c) 2010 - Mozy, Inc.

#include <boost/noncopyable.hpp>
#include <boost/shared_ptr.hpp>

#include "exception.h"

namespace Mordor {

class Stream;
class Tar;

struct CorruptTarException : virtual Exception {};
struct UnsupportedTarFormatException : virtual Exception {};
struct IncompleteTarHeaderException : virtual Exception {};
struct UnexpectedTarSizeException : virtual Exception {};

/// A single file within a Tar archive
class TarEntry
{
    friend class Tar;
public:
    /// Only support those file types and common metadata attributes
    enum FileType
    {
        REGULAR,
        DIRECTORY,
        SYMLINK
    };

    /// Accessors
    const std::string& filename() const { return m_filename; }
    void filename(const std::string &filename) { m_filename = filename; }

    const std::string& linkname() const { return m_linkname; }
    void linkname(const std::string& linkname) { m_linkname = linkname; }

    FileType filetype() const { return m_filetype; }
    void filetype(FileType filetype) { m_filetype = filetype; }

#ifdef POSIX
    /// Only for permissions
    mode_t mode() const { return m_mode; }
    void mode(mode_t mode) { m_mode = mode; }

    uid_t uid() const { return m_uid; }
    void uid(uid_t uid) { m_uid = uid; }

    gid_t gid() const { return m_gid; }
    void gid(gid_t gid) { m_gid = gid; }
#endif

    const std::string& uname() const { return m_uname; }
    void uname(const std::string& uname) { m_uname = uname; }

    const std::string& gname() const { return m_gname; }
    void gname(const std::string& gname) { m_gname = gname; }

    long long size() const { return m_size; }
    void size(long long size) { m_size = size; }

    time_t atime() const { return m_atime; }
    void atime(time_t atime) { m_atime = atime; }

    time_t ctime() const { return m_ctime; }
    void ctime(time_t ctime) { m_ctime = ctime; }

    time_t mtime() const { return m_mtime; }
    void mtime(time_t mtime) { m_mtime = mtime; }

    /// Gets stream for writing file to tar archive entry
    /// @note Only one TarEntry stream can be accessed at any one time from a
    /// single Tar; accessing the stream() of another TarEntry will implicitly
    /// close the previously accessed one
    boost::shared_ptr<Stream> stream();

    /// Gets stream for read file from tar archive entry
    /// @return pointer is valid only it is a regular file
    boost::shared_ptr<Stream> stream() const;

private:
    TarEntry(Tar* outer)
        : m_filetype(REGULAR),
#ifdef POSIX
          m_mode(0),
          m_uid(0),
          m_gid(0),
#endif
          m_size(-1ll),
          m_atime(0),
          m_ctime(0),
          m_mtime(0),
          m_startOffset(0ll),
          m_dataOffset(0ll),
          m_outer(outer)
    {}

    /// Writes the file header
    void commit();

    /// Fills up the full block
    void close();

    /// Resets all fields
    void clear();

    std::string m_filename;
    std::string m_linkname;
    FileType m_filetype;
#ifdef POSIX
    mode_t m_mode;
    uid_t m_uid;
    gid_t m_gid;
#endif
    std::string m_uname;
    std::string m_gname;
    long long m_size;
    time_t m_atime;
    time_t m_ctime;
    time_t m_mtime;

    long long m_startOffset;
    long long m_dataOffset;
    mutable Tar* m_outer;
};

std::ostream& operator <<(std::ostream &os, const TarEntry& entry);

/// @brief Tar Archive Format access
///
/// Supports reading or writing a Tar archive of ustar/pax (POSIX.1-2001) format.
/// It supports GNU long name and long link extension for reading.
/// And it supports non-seekable streams for both reading and writing.
///
/// Example of sequentially accessing a (possibly non-seekable) tar file:
/// @code
/// Tar tar(stream);
/// while (const TarEntry* entry = tar.getNextEntry()) {
///     Stream::ptr stream = entry->stream();
///     if (stream) {
///         FileStream outputStream(entry->filename(), FileStream::WRITE,
///             FileStream::OVERWRITE_OR_CREATE);
///         transferStream(stream, outputStream);
///     }
/// }
/// @endcode
///
/// Example of writing a (possibly non-seekable) tar file:
/// @code
/// Tar tar(stream);
/// for (std::vector<std::string>::const_iterator it = files.begin();
///     it != files.end(); ++it) {
///     TarEntry& entry = tar.addFile();
///     entry.filename(*it);
///     FileStream inputStream(*it, FileStream::READ);
///     entry.size(inputStream.size());
///     entry.filetype(TarEntry::REGULAR);
///     transferStream(inputStream, entry.stream());
/// }
/// tar.close();
/// @endcode
class Tar : boost::noncopyable
{
    friend class TarEntry;
public:
    enum OpenMode
    {
        /// Opens the tar file for reading
        READ,
        /// Opens the tar file for writing (truncating existing files)
        WRITE,
        /// Infer the OpenMode.  If the stream supportsWrite(), it will open in
        /// WRITE mode, otherwise READ
        INFER
    };

    Tar(boost::shared_ptr<Stream> stream, OpenMode mode = INFER);

    /// Adds a file entry
    /// @pre stream->supportsWrite()
    TarEntry& addFile();

    /// Writes the end-of-archive entry
    /// @pre stream->supportsWrite()
    void close();

    /// Gets a file entry
    /// @pre stream->supportsRead()
    const TarEntry* getNextEntry();

private:
    void getContentString(long long size, std::string& str);

    boost::shared_ptr<Stream> m_stream;
    boost::shared_ptr<Stream> m_dataStream;
    TarEntry* m_currentEntry;
    TarEntry m_scratchEntry;
    TarEntry m_defaults; // pax global defaults
    OpenMode m_mode;
};

}

#endif

