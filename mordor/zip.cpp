// Copyright (c) 2010 - Mozy, Inc.

#include "zip.h"

#include "mordor/assert.h"
#include "mordor/log.h"
#include "mordor/streams/buffered.h"
#include "mordor/streams/deflate.h"
#include "mordor/streams/hash.h"
#include "mordor/streams/limited.h"
#include "mordor/streams/notify.h"
#include "mordor/streams/null.h"
#include "mordor/streams/singleplex.h"
#include "mordor/streams/transfer.h"

namespace {
#pragma pack(push)
#pragma pack(1)
struct LocalFileHeader
{
    unsigned int signature;             // 0x04034b50
    unsigned short extractVersion;
    unsigned short generalPurposeFlags;
    unsigned short compressionMethod;
    unsigned short modifiedTime;
    unsigned short modifiedDate;
    unsigned int crc32;
    unsigned int compressedSize;
    unsigned int decompressedSize;
    unsigned short fileNameLength;
    unsigned short extraFieldLength;
    //char   FileName[FileNameLength];
    //       ExtraField[ExtraFieldLength]; // Array of ExtraField structures; length is in bytes
};

struct DataDescriptor
{
    unsigned int signature;
    unsigned int crc32;
    unsigned int compressedSize;
    unsigned int uncompressedSize;
};

struct DataDescriptor64
{
    unsigned int signature;
    unsigned int crc32;
    long long compressedSize;
    long long uncompressedSize;
};

struct FileHeader
{
    unsigned int signature;
    unsigned short versionMadeBy;
    unsigned short extractVersion;
    unsigned short generalPurposeFlags;
    unsigned short compressionMethod;
    unsigned short modifiedTime;
    unsigned short modifiedDate;
    unsigned int crc32;
    unsigned int compressedSize;
    unsigned int uncompressedSize;
    unsigned short fileNameLength;
    unsigned short extraFieldLength;
    unsigned short fileCommentLength;
    unsigned short diskNumberStart;
    unsigned short internalFileAttributes;
    unsigned int externalFileAttributes;
    unsigned int localHeaderOffset;
    // char fileName[fileNameLength];
    // char extraField[extraFieldLength];
    // char fileComment[fileCommentLength];
};

struct Zip64EndOfCentralDirectory
{
    unsigned int signature;
    unsigned long long sizeOfEndOfCentralDirectory;
    unsigned short versionMadeBy;
    unsigned short extractVersion;
    unsigned int numberOfThisDisk;
    unsigned int startOfCentralDirectoryDisk;
    unsigned long long centralDirectoryEntriesThisDisk;
    unsigned long long centralDirectoryEntries;
    unsigned long long sizeOfCentralDirectory;
    long long startOfCentralDirectoryOffset;
};

struct Zip64EndOfCentralDirectoryLocator
{
    unsigned int signature;
    unsigned int endOfCentralDirectoryDisk;
    long long endOfCentralDirectoryOffset;
    unsigned int totalDisks;
};

struct EndOfCentralDirectory
{
    unsigned int signature;
    unsigned short numberOfThisDisk;
    unsigned short startOfCentralDirectoryDisk;
    unsigned short centralDirectoryEntriesThisDisk;
    unsigned short centralDirectoryEntries;
    unsigned int sizeOfCentralDirectory;
    unsigned int startOfCentralDirectoryOffset;
    unsigned short commentLength;
};
#pragma pack(pop)
}

namespace Mordor {

Zip::Zip(Stream::ptr stream, OpenMode mode)
    : m_stream(stream),
      m_currentFile(NULL),
#ifdef MSVC
#pragma warning(push)
#pragma warning(disable: 4355)
#endif
      m_scratchFile(*this)
#ifdef MSVC
#pragma warning(pop)
#endif
{
    if (!m_stream->supportsTell())
        m_stream.reset(new LimitedStream(m_stream,
            0x7fffffffffffffffll));
    m_stream.reset(new BufferedStream(m_stream));

    if (mode == INFER) {
        if (m_stream->supportsWrite())
            mode = WRITE;
        else
            mode = READ;
    }
    switch (mode) {
        case READ:
            MORDOR_ASSERT(m_stream->supportsRead());
            if (m_stream->supportsSeek() && m_stream->supportsSize()) {
                unsigned char buffer[65536];
                long long fileSize = m_stream->size();
                if (fileSize < (long long)sizeof(EndOfCentralDirectory))
                    MORDOR_THROW_EXCEPTION(CorruptZipException());
                size_t toRead = (size_t)std::min(65536ll, fileSize);
                m_stream->seek(-(long long)toRead, Stream::END);
                m_stream->read(buffer, toRead);
                unsigned char *search = buffer + toRead -
                    sizeof(EndOfCentralDirectory);
                while (search >= buffer) {
                    if (*(unsigned int *)search == 0x06054b50)
                        break;
                    --search;
                }
                if (search < buffer)
                    MORDOR_THROW_EXCEPTION(CorruptZipException());
                EndOfCentralDirectory &eocd = *(EndOfCentralDirectory *)search;
                if (eocd.numberOfThisDisk != eocd.startOfCentralDirectoryDisk)
                    MORDOR_THROW_EXCEPTION(SpannedZipNotSupportedException());

                // Look for zip64 extensions
                search -= sizeof(Zip64EndOfCentralDirectoryLocator);
                long long startOfCentralDirectory;
                unsigned long long entries = 0;
                if (search >= buffer && *(unsigned int*)search == 0x07064b50) {
                    Zip64EndOfCentralDirectoryLocator &z64eocdl =
                        *(Zip64EndOfCentralDirectoryLocator *)search;
                    if (z64eocdl.totalDisks > 1)
                        MORDOR_THROW_EXCEPTION(SpannedZipNotSupportedException());
                    m_stream->seek(z64eocdl.endOfCentralDirectoryOffset);
                    Zip64EndOfCentralDirectory z64eocd;
                    if (m_stream->read(&z64eocd,
                        sizeof(Zip64EndOfCentralDirectory)) !=
                        sizeof(Zip64EndOfCentralDirectory))
                        MORDOR_THROW_EXCEPTION(UnexpectedEofException());
                    if (z64eocd.startOfCentralDirectoryDisk !=
                        z64eocd.numberOfThisDisk)
                        MORDOR_THROW_EXCEPTION(SpannedZipNotSupportedException());
                    startOfCentralDirectory =
                        z64eocd.startOfCentralDirectoryOffset;
                    entries = z64eocd.centralDirectoryEntries;
                } else {
                    startOfCentralDirectory =
                        eocd.startOfCentralDirectoryOffset;
                    entries = eocd.centralDirectoryEntries;
                }
                m_stream->seek(startOfCentralDirectory);
                for (unsigned long long i = 0; i < entries; ++i) {
                    FileHeader header;
                    if (m_stream->read(&header, sizeof(FileHeader))
                        != sizeof(FileHeader))
                        MORDOR_THROW_EXCEPTION(UnexpectedEofException());
                    if (header.signature != 0x02014b50)
                        MORDOR_THROW_EXCEPTION(CorruptZipException());
                    ZipEntry file(*this);
                    if (header.fileNameLength) {
                        file.m_filename.resize(header.fileNameLength);
                        if (m_stream->read(&file.m_filename[0],
                            header.fileNameLength) != header.fileNameLength)
                            MORDOR_THROW_EXCEPTION(UnexpectedEofException());
                    }
                    file.m_crc = header.crc32;
                    file.m_compressedSize = header.compressedSize;
                    file.m_size = header.uncompressedSize;
                    file.m_startOffset = header.localHeaderOffset;
                    file.m_flags = header.generalPurposeFlags;
                    if (header.extraFieldLength) {
                        std::string extraFields;
                        extraFields.resize(header.extraFieldLength);
                        size_t read = m_stream->read(&extraFields[0],
                            header.extraFieldLength);
                        if (read != header.extraFieldLength)
                            MORDOR_THROW_EXCEPTION(UnexpectedEofException());
                        const unsigned char *extra =
                            (const unsigned char *)extraFields.c_str();
                        const unsigned char *end = extra +
                            header.extraFieldLength;
                        while (true) {
                            if (extra + 2 >= end)
                                break;
                            unsigned short type =
                                *(const unsigned short *)extra;
                            extra += 2;
                            if (extra + 2 >= end)
                                break;
                            unsigned short length =
                                *(const unsigned short *)extra;
                            if (extra + length >= end)
                                break;
                            if (type == 0x0001u) {
                                unsigned short expectedLength = 0;
                                if (header.uncompressedSize == 0xffffffffu)
                                    expectedLength += 8;
                                if (header.compressedSize == 0xffffffffu)
                                    expectedLength += 8;
                                if (header.localHeaderOffset == 0xffffffffu)
                                    expectedLength += 8;
                                if (header.diskNumberStart == 0xffffu)
                                    expectedLength += 4;
                                if (length != expectedLength) {
                                    extra += length;
                                    continue;
                                }
                                if (header.uncompressedSize == 0xffffffffu) {
                                    file.m_size = *(const long long *)extra;
                                    extra += 8;
                                }
                                if (header.compressedSize == 0xffffffffu) {
                                    file.m_compressedSize =
                                        *(const long long *)extra;
                                    extra += 8;
                                }
                                if (header.localHeaderOffset == 0xffffffffu)
                                    file.m_startOffset =
                                        *(const long long *)extra;
                            } else {
                                extra += length;
                            }
                        }
                    }
                    m_centralDirectory.insert(std::make_pair(file.m_filename,
                        file));
                }

                // Return stream pointer to 0 for getNextFile to work properly
                m_stream->seek(0);
            }
            break;
        case WRITE:
            MORDOR_ASSERT(m_stream->supportsWrite());
            break;
        default:
            MORDOR_NOTREACHED();
    }
    m_mode = mode;
}

ZipEntry &
Zip::addFile()
{
    if (m_currentFile) {
        MORDOR_ASSERT(m_currentFile == &m_scratchFile);
        m_scratchFile.close();
        m_centralDirectory.insert(std::make_pair(m_scratchFile.filename(),
            m_scratchFile));
        m_scratchFile.clear();
    } else {
        m_currentFile = &m_scratchFile;
    }
    return m_scratchFile;
}

void
Zip::close()
{
    MORDOR_ASSERT(m_mode == WRITE);
    if (m_currentFile) {
        MORDOR_ASSERT(m_currentFile == &m_scratchFile);
        m_scratchFile.close();
        m_centralDirectory.insert(std::make_pair(m_scratchFile.filename(),
            m_scratchFile));
        m_scratchFile.clear();
    }
    unsigned long long centralDirectorySize = 0;
    unsigned long long centralDirectoryRecords = 0;
    long long startOfCentralDirectory = m_stream->tell();
    for (ZipEntries::const_iterator it = m_centralDirectory.begin();
        it != m_centralDirectory.end();
        ++it) {
        const ZipEntry &file = it->second;
        FileHeader header;
        header.signature = 0x02014b50;
        header.versionMadeBy = 10;
        header.extractVersion = 10;
        header.generalPurposeFlags = file.m_flags;
        header.compressionMethod = 8;
        header.modifiedTime = 0;
        header.modifiedDate = 0;
        header.crc32 = file.m_crc;
        header.compressedSize = (unsigned int)std::min<long long>(0xffffffff,
            file.m_compressedSize);
        header.uncompressedSize = (unsigned int)std::min<long long>(0xffffffff,
            file.m_size);
        header.fileNameLength = (unsigned short)file.m_filename.size();
        header.extraFieldLength = 0;
        header.fileCommentLength = (unsigned short)file.m_comment.size();
        header.diskNumberStart = 0;
        header.internalFileAttributes = 0;
        header.externalFileAttributes = 0;
        header.localHeaderOffset = (unsigned int)std::min<long long>(0xffffffff,
            file.m_startOffset);
        std::string extraFields;
        if (header.compressedSize == 0xffffffffu ||
            header.uncompressedSize == 0xffffffffu ||
            header.localHeaderOffset == 0xffffffffu) {
            header.extraFieldLength += 4;
            if (header.uncompressedSize == 0xffffffffu)
                header.extraFieldLength += 8;
            if (header.compressedSize == 0xffffffffu)
                header.extraFieldLength += 8;
            if (header.localHeaderOffset == 0xffffffffu)
                header.extraFieldLength += 8;
            extraFields.resize(header.extraFieldLength);
            unsigned char *extra = (unsigned char *)&extraFields[0];
            *(unsigned short *)extra = 0x0001;
            extra += 2;
            *(unsigned short *)extra = header.extraFieldLength - 4u;
            extra += 2;
            if (header.uncompressedSize == 0xffffffffu) {
                *(long long *)extra = file.m_size;
                extra += 8;
            }
            if (header.compressedSize == 0xffffffffu) {
                *(long long *)extra = file.m_compressedSize;
                extra += 8;
            }
            if (header.localHeaderOffset == 0xffffffffu) {
                *(long long *)extra = file.m_startOffset;
                extra += 8;
            }
        }
        m_stream->write(&header, sizeof(FileHeader));
        if (header.fileNameLength)
            m_stream->write(file.m_filename.c_str(), file.m_filename.size());
        if (header.extraFieldLength)
            m_stream->write(extraFields.c_str(), extraFields.size());
        if (header.fileCommentLength)
            m_stream->write(file.m_comment.c_str(), file.m_comment.size());
        ++centralDirectoryRecords;
        centralDirectorySize += sizeof(FileHeader) + header.fileNameLength +
            header.extraFieldLength + header.fileCommentLength;
    }
    if (centralDirectoryRecords >= 0xffffu ||
        centralDirectorySize >= 0xffffffffu ||
        startOfCentralDirectory >= 0xffffffffu) {
        Zip64EndOfCentralDirectory eocd;
        eocd.signature = 0x06064b50;
        eocd.sizeOfEndOfCentralDirectory =
            sizeof(Zip64EndOfCentralDirectory) - 12;
        eocd.versionMadeBy = 10;
        eocd.extractVersion = 10;
        eocd.numberOfThisDisk = 0;
        eocd.startOfCentralDirectoryDisk = 0;
        eocd.centralDirectoryEntriesThisDisk = centralDirectoryRecords;
        eocd.centralDirectoryEntries = centralDirectoryRecords;
        eocd.sizeOfCentralDirectory = centralDirectorySize;
        eocd.startOfCentralDirectoryOffset = startOfCentralDirectory;
        Zip64EndOfCentralDirectoryLocator locator;
        locator.signature = 0x07064b50;
        locator.endOfCentralDirectoryDisk = 0;
        locator.endOfCentralDirectoryOffset = m_stream->tell();
        locator.totalDisks = 1;
        m_stream->write(&eocd, sizeof(Zip64EndOfCentralDirectory));
        m_stream->write(&locator, sizeof(Zip64EndOfCentralDirectoryLocator));
    }
    EndOfCentralDirectory eocd;
    eocd.signature = 0x06054b50;
    eocd.numberOfThisDisk = 0;
    eocd.startOfCentralDirectoryDisk = 0;
    eocd.centralDirectoryEntriesThisDisk = (unsigned short)
        std::min<unsigned long long>(0xffffu, centralDirectoryRecords);
    eocd.centralDirectoryEntries = eocd.centralDirectoryEntriesThisDisk;
    eocd.sizeOfCentralDirectory = (unsigned int)
        std::min<unsigned long long>(0xffffffffu, centralDirectorySize);
    eocd.startOfCentralDirectoryOffset = (unsigned int)
        std::min<unsigned long long>(0xffffffffu, startOfCentralDirectory);
    eocd.commentLength = 0;
    m_stream->write(&eocd, sizeof(EndOfCentralDirectory));
    m_stream->close();
}

ZipEntry const *
Zip::getNextEntry()
{
    if (m_currentFile) {
        if (!m_stream->supportsSeek()) {
            transferStream(m_currentFile->stream(), NullStream::get());
        } else {
            m_stream->seek(m_currentFile->m_startOffset +
                sizeof(LocalFileHeader) +
                m_currentFile->m_filename.size() +
                m_currentFile->m_extraFieldsLength +
                m_currentFile->m_compressedSize);
        }
    }
    LocalFileHeader header;
    m_scratchFile.clear();
    m_scratchFile.m_startOffset = m_stream->tell();
    m_currentFile = NULL;
    if (m_stream->read(&header, sizeof(LocalFileHeader)) !=
        sizeof(LocalFileHeader))
        return NULL;
    if (header.signature != 0x04034b50)
        return NULL;
    if (header.fileNameLength) {
        m_scratchFile.m_filename.resize(header.fileNameLength);
        if (m_stream->read(&m_scratchFile.m_filename[0],
            header.fileNameLength) != header.fileNameLength)
            MORDOR_THROW_EXCEPTION(UnexpectedEofException());
    }
    m_scratchFile.m_crc = header.crc32;
    m_scratchFile.m_compressedSize = header.compressedSize;
    m_scratchFile.m_size = header.decompressedSize;
    m_currentFile = &m_scratchFile;
    std::pair<ZipEntries::iterator, ZipEntries::iterator> range =
        m_centralDirectory.equal_range(m_scratchFile.m_filename);
    while (range.first != range.second) {
        if (range.first->second.m_startOffset == m_scratchFile.m_startOffset) {
            m_currentFile = &range.first->second;
            break;
        }
        ++range.first;
    }
    if (header.extraFieldLength) {
        if (m_currentFile != &m_scratchFile) {
            // We got an entry from the central directory; no need to bother
            // reading the extra fields from the local header
            MORDOR_ASSERT(m_stream->supportsSeek());
            m_stream->seek(header.extraFieldLength, Stream::CURRENT);
        } else {
            std::string extraFields;
            extraFields.resize(header.extraFieldLength);
            size_t read = m_stream->read(&extraFields[0],
                header.extraFieldLength);
            if (read != header.extraFieldLength)
                MORDOR_THROW_EXCEPTION(UnexpectedEofException());
            const unsigned char *extra =
                (const unsigned char *)extraFields.c_str();
            const unsigned char *end = extra + header.extraFieldLength;
            while (true) {
                if (extra + 2 >= end)
                    break;
                unsigned short type = *(const unsigned short *)extra;
                extra += 2;
                if (extra + 2 >= end)
                    break;
                unsigned short length = *(const unsigned short *)extra;
                if (extra + length >= end)
                    break;
                if (type == 0x0001u) {
                    unsigned short expectedLength = 0;
                    if (header.decompressedSize == 0xffffffffu)
                        expectedLength += 8;
                    if (header.compressedSize == 0xffffffffu)
                        expectedLength += 8;
                    if (length != expectedLength) {
                        extra += length;
                        continue;
                    }
                    if (header.decompressedSize == 0xffffffffu) {
                        m_scratchFile.m_size = *(const long long *)extra;
                        extra += 8;
                    }
                    if (header.compressedSize == 0xffffffffu)
                        m_scratchFile.m_compressedSize =
                            *(const long long *)extra;
                } else {
                    extra += length;
                }
            }
        }
    }

    if (!m_deflateStream) {
        MORDOR_ASSERT(!m_compressedStream);
        MORDOR_ASSERT(!m_deflateStream);
        m_compressedStream.reset(
            new LimitedStream(m_stream, m_currentFile->m_compressedSize,
                false));
        m_deflateStream.reset(new DeflateStream(
            m_compressedStream));
    } else {
        MORDOR_ASSERT(m_compressedStream);
        MORDOR_ASSERT(m_deflateStream);
        m_compressedStream->reset(m_currentFile->m_compressedSize);
        m_deflateStream->reset();
    }
    switch (header.compressionMethod) {
        case 0:
            m_fileStream = m_compressedStream;
            break;
        case 8:
            m_fileStream = m_deflateStream;
            break;
        default:
            MORDOR_THROW_EXCEPTION(
                UnsupportedCompressionMethodException());
    }
    if (header.generalPurposeFlags & 0x0008) {
        if (!m_notifyStream)
            m_notifyStream.reset(new NotifyStream(
                m_fileStream));
        m_notifyStream->parent(m_fileStream);
        m_notifyStream->notifyOnEof = boost::bind(&Zip::onFileEOF, this);
        m_fileStream = m_notifyStream;
    }
    return m_currentFile;
}

const ZipEntries &
Zip::getAllEntries()
{
    MORDOR_ASSERT(m_stream->supportsSeek() && m_stream->supportsSize());
    return m_centralDirectory;
}

void
Zip::onFileEOF()
{
    MORDOR_ASSERT(m_currentFile);
    MORDOR_ASSERT(m_notifyStream);
    m_notifyStream->notifyOnEof = NULL;

    unsigned char buffer[24];
    size_t expectedSize = 12;
    // TODO: if you read the same file more than once, this will break
    if (m_currentFile->m_compressedSize == 0xffffffffll)
        expectedSize += 4;
    if (m_currentFile->m_size == 0xffffffffll)
        expectedSize += 4;
    if (m_stream->read(buffer, expectedSize) != expectedSize)
        MORDOR_THROW_EXCEPTION(UnexpectedEofException());
    unsigned char *start = buffer;
    if (*(unsigned int *)buffer == 0x08074b50) {
        // Had a signature, read an additional four bytes
        start += 4u;
        if (m_stream->read(buffer + expectedSize, 4u) != 4u)
            MORDOR_THROW_EXCEPTION(UnexpectedEofException());
    }
    m_currentFile->m_crc = *(unsigned int *)start;
    start += 4;
    if (m_currentFile->m_compressedSize == 0xffffffffll) {
        m_currentFile->m_compressedSize = *(long long *)start;
        start += 8;
    } else {
        m_currentFile->m_compressedSize = *(unsigned int *)start;
        start += 4;
    }
    if (m_currentFile->m_size == 0xffffffffll)
        m_currentFile->m_size = *(long long *)start;
    else
        m_currentFile->m_size = *(unsigned int *)start;
}

void
ZipEntry::filename(const std::string &filename)
{
    m_filename = filename;
}

void
ZipEntry::comment(const std::string &comment)
{
    m_comment = comment;
}

void
ZipEntry::size(long long size)
{
    m_size = size;
}

Stream::ptr
ZipEntry::stream()
{
    m_startOffset = m_outer.m_stream->tell();
    commit();
    if (m_outer.m_compressedStream) {
        MORDOR_ASSERT(m_outer.m_deflateStream);
        MORDOR_ASSERT(m_outer.m_crcStream);
        MORDOR_ASSERT(m_outer.m_uncompressedStream);
        m_outer.m_compressedStream->reset();
        m_outer.m_deflateStream->reset();
        m_outer.m_crcStream->reset();
        m_outer.m_uncompressedStream->reset(
            m_size == -1ll ? 0x7fffffffffffffffll : m_size);
    } else {
        MORDOR_ASSERT(!m_outer.m_deflateStream);
        MORDOR_ASSERT(!m_outer.m_crcStream);
        MORDOR_ASSERT(!m_outer.m_uncompressedStream);
        m_outer.m_compressedStream.reset(
            new LimitedStream(m_outer.m_stream, 0x7fffffffffffffffll));
        m_outer.m_deflateStream.reset(
            new DeflateStream(m_outer.m_compressedStream, false));
        m_outer.m_crcStream.reset(
            new CRC32Stream(m_outer.m_deflateStream));
        m_outer.m_uncompressedStream.reset(
            new LimitedStream(m_outer.m_crcStream,
                m_size == -1ll ? 0x7fffffffffffffffll : m_size));
    }
    return m_outer.m_uncompressedStream;
}

Stream::ptr
ZipEntry::stream() const
{
    if (m_outer.m_currentFile != this) {
        MORDOR_ASSERT(m_outer.m_stream->supportsSeek());
        m_outer.m_stream->seek(m_startOffset);
        LocalFileHeader header;
        size_t read = m_outer.m_stream->read(&header, sizeof(LocalFileHeader));
        if (read < sizeof(LocalFileHeader))
            MORDOR_THROW_EXCEPTION(UnexpectedEofException());
        if (header.signature != 0x04034b50)
            MORDOR_THROW_EXCEPTION(CorruptZipException());
        if (header.fileNameLength + header.extraFieldLength)
            m_outer.m_stream->seek(header.fileNameLength +
                header.extraFieldLength, Stream::CURRENT);

        if (!m_outer.m_deflateStream) {
            MORDOR_ASSERT(!m_outer.m_compressedStream);
            MORDOR_ASSERT(!m_outer.m_deflateStream);
            m_outer.m_compressedStream.reset(
                new LimitedStream(m_outer.m_stream, m_compressedSize, false));
            m_outer.m_deflateStream.reset(new DeflateStream(
                m_outer.m_compressedStream));
        } else {
            MORDOR_ASSERT(m_outer.m_compressedStream);
            MORDOR_ASSERT(m_outer.m_deflateStream);
            m_outer.m_compressedStream->reset(m_compressedSize);
            m_outer.m_deflateStream->reset();
        }
        switch (header.compressionMethod) {
            case 0:
                m_outer.m_fileStream = m_outer.m_compressedStream;
                break;
            case 8:
                m_outer.m_fileStream = m_outer.m_deflateStream;
                break;
            default:
                MORDOR_THROW_EXCEPTION(
                    UnsupportedCompressionMethodException());
        }
        if (header.generalPurposeFlags & 0x0008) {
            if (!m_outer.m_notifyStream)
                m_outer.m_notifyStream.reset(new NotifyStream(
                    m_outer.m_fileStream));
            m_outer.m_notifyStream->parent(m_outer.m_fileStream);
            m_outer.m_notifyStream->notifyOnEof = boost::bind(&Zip::onFileEOF,
                &m_outer);
            m_outer.m_fileStream = m_outer.m_notifyStream;
        }
    } else {
        MORDOR_ASSERT(m_outer.m_fileStream);
    }
    return m_outer.m_fileStream;
}

void
ZipEntry::commit()
{
    if (!m_outer.m_stream->supportsSeek())
        m_flags = 0x0808;
    LocalFileHeader header;

    header.signature = 0x04034b50;
    header.extractVersion = 10;
    header.generalPurposeFlags = m_flags;
    header.compressionMethod = 8;
    header.modifiedTime = 0;
    header.modifiedDate = 0;
    header.crc32 = 0;
    // Enabling Zip64
    if (m_size == -1ll || m_size >= 0xffffffffll) {
        header.compressedSize = 0xffffffffu;
        header.decompressedSize = 0xffffffffu;
    } else {
        header.compressedSize = 0;
        header.decompressedSize = 0;
    }
    header.fileNameLength = (unsigned int)m_filename.size();
    std::string extraFields;
    if ((m_size == -1ll || m_size >= 0xffffffffll) && !(m_flags & 0x0008)) {
        extraFields.resize(20);
        unsigned char *extra = (unsigned char *)&extraFields[0];
        *(unsigned short *)extra = 0x0001u;
        extra += 2;
        *(unsigned short *)extra = 16u;
        extra += 2;
    }
    header.extraFieldLength = (unsigned short)extraFields.size();
    m_outer.m_stream->write(&header, sizeof(LocalFileHeader));
    m_outer.m_stream->write(m_filename.c_str(), m_filename.size());
    if (header.extraFieldLength)
        m_outer.m_stream->write(extraFields.c_str(), extraFields.size());
}

void
ZipEntry::close()
{
    MORDOR_ASSERT(m_size == -1ll ||
        m_outer.m_uncompressedStream->tell() == m_size);
    m_outer.m_uncompressedStream->close();
    bool zip64 = (m_size == -1ll || m_size >= 0xffffffffll);
    m_size = m_outer.m_uncompressedStream->tell();
    m_compressedSize = m_outer.m_compressedStream->tell();
    m_outer.m_crcStream->hash(&m_crc, sizeof(unsigned int));
    m_crc = byteswapOnLittleEndian(m_crc);
    if (m_flags & 0x0008) {
        if (zip64) {
            DataDescriptor64 dd;
            dd.signature = 0x08074b50;
            dd.crc32 = m_crc;
            dd.compressedSize = m_compressedSize;
            dd.uncompressedSize = m_size;
            m_outer.m_stream->write(&dd, sizeof(DataDescriptor64));
        } else {
            DataDescriptor dd;
            dd.signature = 0x08074b50;
            dd.crc32 = m_crc;
            dd.compressedSize = (unsigned int)m_compressedSize;
            dd.uncompressedSize = (unsigned int)m_size;
            m_outer.m_stream->write(&dd, sizeof(DataDescriptor));
        }
    }
    if (m_outer.m_stream->supportsSeek()) {
        long long current = m_outer.m_stream->tell();
        m_outer.m_stream->seek(m_startOffset + 14);
        m_outer.m_stream->write(&m_crc, 4);
        unsigned int size = (unsigned int)std::min<long long>(
            0xffffffffu, m_compressedSize);
        m_outer.m_stream->write(&size, 4);
        size = (unsigned int)std::min<long long>(0xffffffffu, m_size);
        m_outer.m_stream->write(&size, 4);
        if (zip64 && !(m_flags & 0x0008)) {
            std::string extraFields;
            extraFields.resize(20);
            unsigned char *extra = (unsigned char *)&extraFields[0];
            *(unsigned short *)extra = 0x0001u;
            extra += 2;
            *(unsigned short *)extra = 16u;
            extra += 2;
            *(long long *)extra = m_size;
            extra += 8;
            *(long long *)extra = m_compressedSize;
            extra += 8;
            m_outer.m_stream->seek(4 + m_filename.size(), Stream::CURRENT);
            m_outer.m_stream->write(extraFields.c_str(), extraFields.size());
        }
        m_outer.m_stream->seek(current);
    }
}

void
ZipEntry::clear()
{
    m_filename.clear();
    m_comment.clear();
    m_size = m_compressedSize = -1ll;
    m_startOffset = 0;
    m_crc = 0;
    m_flags = 0x0800;
    m_extraFieldsLength = 0;
}

}
