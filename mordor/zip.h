#ifndef __MORDOR_ZIP_H__
#define __MORDOR_ZIP_H__
// Copyright (c) 2010 - Mozy, Inc.

#include <boost/noncopyable.hpp>
#include <boost/shared_ptr.hpp>

#include "exception.h"

namespace Mordor {

class CRC32Stream;
class DeflateStream;
class LimitedStream;
class NotifyStream;
class Stream;
class Zip;

struct CorruptZipException : virtual Exception {};
struct UnsupportedCompressionMethodException : virtual Exception {};
struct SpannedZipNotSupportedException : virtual Exception {};

/// A single file within a Zip archive
class ZipEntry
{
    friend class Zip;
private:
    ZipEntry(Zip &outer)
        : m_size(-1ll),
          m_compressedSize(-1ll),
          m_startOffset(0ll),
          m_outer(&outer),
          m_crc(0),
          m_flags(0x0800),
          m_extraFieldsLength(0)
    {}

public:
    const std::string &filename() const { return m_filename; }
    void filename(const std::string &filename);

    const std::string &comment() const { return m_comment; }
    void comment(const std::string &comment);

    long long size() const { return m_size; }
    /// Providing size is optional; if it is not provided, Zip64 extensions are
    /// assumed
    void size(long long size);

    long long compressedSize() const { return m_compressedSize; }

    /// @note Only one ZipEntry stream can be accessed at any one time from a
    /// single Zip; accessing the stream() of another ZipEntry will implicitly
    /// close the previously accessed one
    boost::shared_ptr<Stream> stream();
    boost::shared_ptr<Stream> stream() const;

private:
    /// Writes the local file header (and extra field, if any)
    void commit();
    /// Flushes the deflate stream, and stores the CRC and actual sizes
    void close();
    /// Resets all fields
    void clear();

private:
    std::string m_filename, m_comment;
    long long m_size, m_compressedSize;
    long long m_startOffset;
    mutable Zip *m_outer;
    unsigned int m_crc;
    unsigned short m_flags;
    unsigned short m_extraFieldsLength;
};

class ZipEntries : public std::multimap<std::string, ZipEntry>, boost::noncopyable
{};

/// @brief Zip Archive Format access
///
/// Supports reading or writing a Zip, but not both at the same time.  It fully
/// supports non-seekable streams for both reading and writing, though many
/// utilities (i.e. Windows) do not support Zips that weren't seekable when
/// written.  When writing a zip, it will automatically use Zip64 extensions
/// if necessary, which means if you want to avoid Zip64 extensions, you need
/// to provide the filesize (under 4GB) to ZipEntry prior to writing to
/// ZipEntry::stream().
///
/// Example of randomly accessing a zip file:
/// @code
/// Zip zip(stream);
/// const ZipEntries &entries = zip.getAllEntries();
/// ZipEntries::const_iterator it = entries.find("somefile.txt");
/// transferStream(it->second.stream(), NullStream::get());
/// @endcode
/// Example of sequentially accessing a (possibly non-seekable) zip file:
/// @code
/// Zip zip(stream);
/// ZipEntry const *entry = zip.getNextEntry();
/// while (entry) {
///     FileStream outputStream(entry->filename(), FileStream::WRITE,
///         FileStream::OVERWRITE_OR_CREATE);
///     transferStream(entry->stream, outputStream);
///     entry = zip.getNextEntry();
/// }
/// @endcode
/// Example of writing a (possibly non-seekable) zip file:
/// @code
/// Zip zip(stream);
/// for (std::vector<std::string>::const_iterator it = files.begin();
///     it != files.end();
///     ++it) {
///     ZipEntry &entry = zip.addEntry();
///     entry.filename(*it);
///     FileStream inputStream(*it, FileStream::READ);
///     entry.size(inputStream.size());
///     transferStream(inputStream, entry.stream());
/// }
/// zip.close();
/// @endcode
class Zip : boost::noncopyable
{
    friend class ZipEntry;
public:
    enum OpenMode
    {
        /// Opens the zip file for reading
        READ,
        /// Opens the zip file for writing (truncating existing files)
        WRITE,
        /// Infer the OpenMode.  If the stream supportsWrite(), it will open in
        /// WRITE mode, otherwise READ
        INFER
    };
public:
    Zip(boost::shared_ptr<Stream> stream, OpenMode mode = INFER);

    /// @pre stream->supportsWrite()
    ZipEntry &addFile();
    /// Writes the central directory
    /// @pre stream->supportsWrite()
    void close();

    /// @pre stream->supportsRead()
    ZipEntry const *getNextEntry();
    /// @pre stream->supportsRead()
    /// @pre stream->supportsSeek() && stream->supportsSize()
    const ZipEntries &getAllEntries();

private:
    void onFileEOF();

private:
    boost::shared_ptr<Stream> m_stream, m_fileStream;
    boost::shared_ptr<LimitedStream> m_uncompressedStream, m_compressedStream;
    boost::shared_ptr<DeflateStream> m_deflateStream;
    boost::shared_ptr<CRC32Stream> m_crcStream;
    boost::shared_ptr<NotifyStream> m_notifyStream;
    ZipEntry *m_currentFile;
    ZipEntry m_scratchFile;
    OpenMode m_mode;
    ZipEntries m_centralDirectory;
};

}

#endif
