// Copyright (c) 2010 - Mozy, Inc.

#include "tar.h"

#ifdef WINDOWS

/* The bits in mode: */
#define TSUID   04000
#define TSGID   02000
#define TSVTX   01000
#define TUREAD  00400
#define TUWRITE 00200
#define TUEXEC  00100
#define TGREAD  00040
#define TGWRITE 00020
#define TGEXEC  00010
#define TOREAD  00004
#define TOWRITE 00002
#define TOEXEC  00001
/* The values for typeflag:
   Values 'A'-'Z' are reserved for custom implementations.
   All other values are reserved for future POSIX.1 revisions.  */
#define REGTYPE     '0'   /* Regular file (preferred code).  */
#define AREGTYPE    '\0'  /* Regular file (alternate code).  */
#define LNKTYPE     '1'   /* Hard link.  */
#define SYMTYPE     '2'   /* Symbolic link (hard if not supported).  */
#define CHRTYPE     '3'   /* Character special.  */
#define BLKTYPE     '4'   /* Block special.  */
#define DIRTYPE     '5'   /* Directory.  */
#define FIFOTYPE    '6'   /* Named pipe.  */
#define CONTTYPE    '7'   /* Contiguous file */
/* Contents of magic field and its length.  */
#define TMAGIC  "ustar"
#define TMAGLEN 6
/* Contents of the version field and its length.  */
#define TVERSION    "00"
#define TVERSLEN    2

#elif defined(POSIX)
#include <tar.h>
#endif

#include <boost/lexical_cast.hpp>
#include <boost/static_assert.hpp>

#include "mordor/assert.h"
#include "mordor/log.h"
#include "mordor/streams/buffered.h"
#include "mordor/streams/limited.h"
#include "mordor/streams/null.h"
#include "mordor/streams/transfer.h"
#include "mordor/streams/zero.h"

namespace {

const unsigned int BLOCK_SIZE = 512;
const unsigned int HEADER_NAME_LEN = 100;
const unsigned int HEADER_PREFIX_LEN = 155;
char zeroBlock[BLOCK_SIZE] = {0};
char zeroMagic[TMAGLEN] = {0};
const std::string GNU_LONGLINK_TAG = "././@LongLink";
const std::string PAX_GLOBAL_HEADER_NAME_TAG = "/tmp/GlobalHead";
const std::string PAX_HEADER_NAME_TAG = "./PaxHeaders/";

// GNU extension types
const char GNU_LONGNAME_TYPE = 'L';
const char GNU_LONGLINK_TYPE = 'K';
// pax types
const char PAX_FILE_TYPE = 'x';
const char PAX_GLOBAL_TYPE = 'g';
// pax attrs
const std::string PAX_ATTR_PATH = "path";
const std::string PAX_ATTR_LINKPATH = "linkpath";
const std::string PAX_ATTR_SIZE = "size";
const std::string PAX_ATTR_UID = "uid";
const std::string PAX_ATTR_GID = "gid";
const std::string PAX_ATTR_UNAME = "uname";
const std::string PAX_ATTR_GNAME = "gname";
const std::string PAX_ATTR_ATIME = "atime";
const std::string PAX_ATTR_CTIME = "ctime";
const std::string PAX_ATTR_MTIME = "mtime";
// unstar limits
const unsigned int USTAR_MAX_ID = 2097151;
const long long USTAR_MAX_SIZE = 8589934591ll; // 8G

#pragma pack(push)
#pragma pack(1)
// posix ustar header
struct TarHeader
{
    char name[100];
    char mode[8];
    char uid[8];
    char gid[8];
    char size[12];
    char mtime[12];
    char chksum[8];
    char typeflag;
    char linkname[100];
    char magic[6];
    char version[2];
    char uname[32];
    char gname[32];
    char devmajor[8];
    char devminor[8];
    char prefix[155];
    char padding[12];
};
#pragma pack(pop)

BOOST_STATIC_ASSERT(sizeof(TarHeader) == BLOCK_SIZE);

// helpers
template <typename T>
T oct2dec(const std::string& in)
{
    T out;
    std::stringstream ss;
    ss << std::oct << in;
    ss >> out;
    return out;
}

template <typename T>
std::string dec2oct(T in)
{
    std::ostringstream ss;
    ss << std::oct << in;
    return ss.str();
}

void fillHeaderNumber(char* dest, size_t size, long long num, int padding = 1)
{
    std::string oct = dec2oct(num);
    long long index = size - oct.length() - padding;
    memset(dest, '0', size);
    if (index >= 0) {
        strncpy(&dest[index], oct.data(), oct.length() + padding);
    }
}

std::string fillPaxAttribute(const std::string& attr, const std::string& value)
{
    std::string kv = " " + attr + "=" + value + "\n";
    std::string lenStr = boost::lexical_cast<std::string>(kv.length());
    size_t len = kv.length() + lenStr.length();
    lenStr = boost::lexical_cast<std::string>(len);
    std::string item = lenStr + kv;
    if (item.size() > len) {
        item = boost::lexical_cast<std::string>(++len) + kv;
    }
    return item;
}

void parsePaxAttributes(const std::string& paxAttrs, Mordor::TarEntry& entry)
{
    if (paxAttrs.empty()) {
        return;
    }

    std::string::size_type start = 0;
    std::string::size_type end = 0;

    try {
        while ((end = paxAttrs.find(' ', start)) != std::string::npos) {
            size_t len = boost::lexical_cast<size_t>(paxAttrs.substr(start, end - start));
            if (len == 0) {
                break;
            }

            len += start;
            start = end + 1;
            if ((end = paxAttrs.find('=', start)) != std::string::npos) {
                std::string key(paxAttrs.substr(start, end - start));
                start = end + 1;
                std::string value(paxAttrs.substr(start, len - start - 1));
                start = len;

                if (key == PAX_ATTR_PATH) {
                    entry.filename(value);
                } else if (key == PAX_ATTR_LINKPATH) {
                    entry.linkname(value);
                } else if (key == PAX_ATTR_SIZE) {
                    entry.size(boost::lexical_cast<long long>(value));
#ifdef POSIX
                } else if (key == PAX_ATTR_UID) {
                    entry.uid(boost::lexical_cast<uid_t>(value));
                } else if (key == PAX_ATTR_GID) {
                    entry.gid(boost::lexical_cast<gid_t>(value));
#endif
                } else if (key == PAX_ATTR_UNAME) {
                    entry.uname(value);
                } else if (key == PAX_ATTR_GNAME) {
                    entry.gname(value);
                } else if (key == PAX_ATTR_ATIME) {
                    entry.atime((time_t)boost::lexical_cast<double>(value));
                } else if (key == PAX_ATTR_CTIME) {
                    entry.ctime((time_t)boost::lexical_cast<double>(value));
                } else if (key == PAX_ATTR_MTIME) {
                    entry.mtime((time_t)boost::lexical_cast<double>(value));
                } else {
                    entry.setAttribute(key, value);
                }
            } else {
                break;
            }
        }
    } catch (const boost::bad_lexical_cast&) {
        // ignore
    }
}

inline long long blockSize(long long size)
{
    return (size + BLOCK_SIZE - 1) & ~((long long)BLOCK_SIZE - 1);
}

inline long long padSize(long long size)
{
    return blockSize(size) - size;
}

unsigned int calChecksum(const TarHeader& header)
{
    unsigned int sum = 0;
    for (unsigned int i = 0; i < BLOCK_SIZE; ++i) {
        sum += ((unsigned char*)&header)[i];
    }
    for (unsigned int i = 0; i < 8; ++i) {
        sum += (' ' - (unsigned char)header.chksum[i]);
    }
    return sum;
}

}

namespace Mordor {

static Logger::ptr g_log = Log::lookup("mordor:tar");

void
TarEntry::commit()
{
    // reset size to zero if type is directory or symlink
    if (m_filetype == DIRECTORY || m_filetype == SYMLINK) {
        m_size = 0;
    }

    // check mandatory items
    if (m_filename.empty() || m_size == -1) {
        MORDOR_THROW_EXCEPTION(IncompleteTarHeaderException());
    }

    TarHeader header;
    memset(&header, 0, BLOCK_SIZE);
    strncpy(header.magic, TMAGIC, TMAGLEN);
    strncpy(header.version, TVERSION, TVERSLEN);

    // set pax global attrs
    if (!m_outer->m_defaults.m_attrs.empty()) {
        strncpy(header.name, PAX_GLOBAL_HEADER_NAME_TAG.c_str(), sizeof(header.name));
        header.typeflag = PAX_GLOBAL_TYPE;

        std::string global;
        for (std::map<std::string, std::string>::iterator it = m_outer->m_defaults.m_attrs.begin();
            it != m_outer->m_defaults.m_attrs.end(); ++it) {
            global += fillPaxAttribute(it->first, it->second);
        }

        fillHeaderNumber(header.size, sizeof(header.size), global.length());
        fillHeaderNumber(header.chksum, sizeof(header.chksum), calChecksum(header), 2);
        header.chksum[7] = ' ';

        m_outer->m_stream->write(&header, BLOCK_SIZE);
        m_outer->m_stream->write(global.data(), global.length());
        // padding nulls to block boundary
        transferStream(ZeroStream::get_ptr(), m_outer->m_stream, padSize(global.length()));

        // reset that do not fill them next time unless they're set again
        m_outer->m_defaults.m_attrs.clear();
    }

    // set pax content, ignore local m_attrs currently
    std::string pax;
    if (m_filename.length() > HEADER_NAME_LEN) {
        pax += fillPaxAttribute(PAX_ATTR_PATH, m_filename);
    }

    if (m_linkname.length() > sizeof(header.linkname)) {
        pax += fillPaxAttribute(PAX_ATTR_LINKPATH, m_linkname);
    }

    try {
#ifdef POSIX
        if (m_uid > USTAR_MAX_ID) {
            pax += fillPaxAttribute(PAX_ATTR_UID, boost::lexical_cast<std::string>(m_uid));
        }

        if (m_gid > USTAR_MAX_ID) {
            pax += fillPaxAttribute(PAX_ATTR_GID, boost::lexical_cast<std::string>(m_gid));
        }
#endif

        if (m_size > USTAR_MAX_SIZE) {
            pax += fillPaxAttribute(PAX_ATTR_SIZE, boost::lexical_cast<std::string>(m_size));
        }

        if (m_atime != 0) {
            pax += fillPaxAttribute(PAX_ATTR_ATIME, boost::lexical_cast<std::string>(m_atime));
        }

        if (m_ctime != 0) {
            pax += fillPaxAttribute(PAX_ATTR_CTIME, boost::lexical_cast<std::string>(m_ctime));
        }
    } catch (const boost::bad_lexical_cast&) {
        // ignore
    }

    if (!pax.empty()) {
        // for simplicity, just prefix pax tag to filename for name field
        strncpy(header.name, (PAX_HEADER_NAME_TAG + m_filename).c_str(), sizeof(header.name));
        header.typeflag = PAX_FILE_TYPE;

        fillHeaderNumber(header.size, sizeof(header.size), pax.length());
        fillHeaderNumber(header.chksum, sizeof(header.chksum), calChecksum(header), 2);
        header.chksum[7] = ' ';

        m_outer->m_stream->write(&header, BLOCK_SIZE);
        m_outer->m_stream->write(pax.data(), pax.length());
        // padding nulls to block boundary
        transferStream(ZeroStream::get_ptr(), m_outer->m_stream, padSize(pax.length()));
    }

    strncpy(header.name, m_filename.data(), sizeof(header.name));
#ifdef POSIX
    fillHeaderNumber(header.mode, sizeof(header.mode), m_mode);
    fillHeaderNumber(header.uid, sizeof(header.uid), m_uid);
    fillHeaderNumber(header.gid, sizeof(header.gid), m_gid);
#endif
    fillHeaderNumber(header.size, sizeof(header.size), m_size);
    fillHeaderNumber(header.mtime, sizeof(header.mtime), m_mtime);
    switch (m_filetype) {
    case REGULAR:
        header.typeflag = REGTYPE;
        break;
    case SYMLINK:
        header.typeflag = SYMTYPE;
        break;
    case DIRECTORY:
        header.typeflag = DIRTYPE;
        break;
    default:
        header.typeflag = AREGTYPE;
        break;
    }
    strncpy(header.linkname, m_linkname.data(), sizeof(header.linkname));
    strncpy(header.uname, m_uname.data(), sizeof(header.uname));
    strncpy(header.gname, m_gname.data(), sizeof(header.gname));

    fillHeaderNumber(header.chksum, sizeof(header.chksum), calChecksum(header), 2);
    header.chksum[7] = ' ';

    m_outer->m_stream->write(&header, BLOCK_SIZE);

    m_dataOffset = m_outer->m_stream->tell();
    if (m_dataOffset % BLOCK_SIZE != 0) {
        MORDOR_THROW_EXCEPTION(IncompleteTarHeaderException());
    }
}

void
TarEntry::close()
{
    if (m_dataOffset == 0) { // if not write header yet
        commit();
    }

    // check data size when closing
    if (m_outer->m_dataStream) {
        if (m_outer->m_stream->supportsSeek()) { // tell (seek) on limited stream
            if (m_outer->m_dataStream->tell() - m_dataOffset != m_size) {
                MORDOR_THROW_EXCEPTION(UnexpectedTarSizeException());
            }
        } else {
            if (m_outer->m_dataStream->tell() != m_size) {
                MORDOR_THROW_EXCEPTION(UnexpectedTarSizeException());
            }
        }
        m_outer->m_dataStream.reset();
    }

    // padding nulls to block boundary
    transferStream(ZeroStream::get_ptr(), m_outer->m_stream, padSize(m_size));
    MORDOR_LOG_DEBUG(g_log) << "tar (" << m_outer << ") adding file done: " << m_filename;
}

Stream::ptr
TarEntry::stream()
{
    m_startOffset = m_outer->m_stream->tell();
    commit();

    if (m_size > 0) {
        m_outer->m_dataStream.reset(new LimitedStream(m_outer->m_stream, m_size, false));
    } else {
        m_outer->m_dataStream.reset();
    }
    return m_outer->m_dataStream;
}

Stream::ptr
TarEntry::stream() const
{
    MORDOR_ASSERT(m_outer->m_currentEntry == this);
    return m_outer->m_dataStream;
}

void
TarEntry::clear()
{
    m_filename.clear();
    m_linkname.clear();
    m_filetype = REGULAR;
#ifdef POSIX
    m_mode = 0;
    m_uid = 0;
    m_gid = 0;
#endif
    m_uname.clear();
    m_gname.clear();
    m_size = -1;
    m_atime = 0;
    m_ctime = 0;
    m_mtime = 0;
    m_startOffset = 0;
    m_dataOffset = 0;
}

void
TarEntry::setAttribute(const std::string& key, const std::string& value) {
    m_attrs[key] = value;
}

std::string
TarEntry::getAttribute(const std::string& key) const {
    std::map<std::string, std::string>::const_iterator it = m_attrs.find(key);
    return it == m_attrs.end() ? std::string() : it->second;
}

const std::map<std::string, std::string> &
TarEntry::getAttributes() const {
    return m_attrs;
}

std::ostream& operator <<(std::ostream& os, const TarEntry& entry)
{
    const static int width = 10;
    os << std::left << std::setw(width) << "filename: " << entry.filename() << std::endl;
    os << std::left << std::setw(width) << "filetype: " << entry.filetype() << std::endl;
#ifdef POSIX
    os << std::left << std::setw(width) << "mode: " << entry.mode() << std::endl;
    os << std::left << std::setw(width) << "uid: " << entry.uid() << std::endl;
    os << std::left << std::setw(width) << "gid: " << entry.gid() << std::endl;
#endif
    os << std::left << std::setw(width) << "uname: " << entry.uname() << std::endl;
    os << std::left << std::setw(width) << "gname: " << entry.gname() << std::endl;
    os << std::left << std::setw(width) << "size: " << entry.size() << std::endl;
    os << std::left << std::setw(width) << "atime: " << entry.atime() << std::endl;
    os << std::left << std::setw(width) << "ctime: " << entry.ctime() << std::endl;
    os << std::left << std::setw(width) << "mtime: " << entry.mtime() << std::endl;
    os << std::left << std::setw(width) << "linkname: " << entry.linkname() << std::endl;
    os << std::endl;
    return os;
}

Tar::Tar(Stream::ptr stream, OpenMode mode)
    : m_stream(stream),
      m_currentEntry(NULL),
#ifdef MSVC
#pragma warning(push)
#pragma warning(disable: 4355)
#endif
      m_scratchEntry(this),
#ifdef MSVC
#pragma warning(pop)
#endif
      m_defaults(NULL),
      m_firstEntry(false)
{
    if (!m_stream->supportsTell()) {
        m_stream.reset(new LimitedStream(m_stream, 0x7fffffffffffffffll));
    }
    m_stream.reset(new BufferedStream(m_stream));

    if (mode == INFER) {
        mode = m_stream->supportsWrite() ? WRITE : READ;
    }

    switch (mode) {
    case READ:
        MORDOR_ASSERT(m_stream->supportsRead());
        // try to get first global entry
        getNextEntry();
        m_firstEntry = true;
        break;
    case WRITE:
        MORDOR_ASSERT(m_stream->supportsWrite());
        break;
    default:
        MORDOR_NOTREACHED();
    }
    m_mode = mode;
}

const TarEntry*
Tar::getNextEntry()
{
    if (m_firstEntry) {
        m_firstEntry = false;
        return m_currentEntry;
    } else if (m_currentEntry) {
        // find next file
        if (!m_stream->supportsSeek()) {
            Stream::ptr stream = static_cast<const TarEntry*>(m_currentEntry)->stream();
            if (stream) {
                transferStream(stream, NullStream::get());
                transferStream(m_stream, NullStream::get(), padSize(m_currentEntry->m_size));
            }
        } else {
            m_stream->seek(m_currentEntry->m_dataOffset + blockSize(m_currentEntry->m_size));
        }
    }

    m_scratchEntry.clear();
    m_scratchEntry.m_startOffset = m_stream->tell();
    m_currentEntry = NULL;
    m_dataStream.reset();

    TarHeader header;
    bool isPreBlockZero = false;
    std::string gnuLongName;
    std::string gnuLongLink;
    std::string paxAttrs;

    while (m_stream->read(&header, BLOCK_SIZE) == BLOCK_SIZE) {
        if (memcmp(&header, zeroBlock, BLOCK_SIZE) == 0) {
            if (isPreBlockZero) { // two zero blocks
                MORDOR_LOG_DEBUG(g_log) << "end of tar archive";
                return NULL;
            }
            isPreBlockZero = true;
            continue;
        } else {
            isPreBlockZero = false;
        }

        // checksum
        unsigned int checksum = oct2dec<unsigned int>(std::string(header.chksum, sizeof(header.chksum)));
        if (calChecksum(header) != checksum) {
            MORDOR_THROW_EXCEPTION(CorruptTarException());
        }

        // check old or ustar
        if (memcmp(header.magic, zeroMagic, TMAGLEN) != 0 &&
            TMAGIC != std::string(header.magic, sizeof(header.magic) - 1)) {
            MORDOR_THROW_EXCEPTION(UnsupportedTarFormatException());
        }

        long long size = oct2dec<long long>(std::string(header.size, sizeof(header.size)));

        switch (header.typeflag) {
        case GNU_LONGNAME_TYPE:
            getContentString(size, gnuLongName);
            gnuLongName = gnuLongName.c_str(); // remove trailing nulls
            continue;
        case GNU_LONGLINK_TYPE:
            getContentString(size, gnuLongLink);
            gnuLongLink = gnuLongLink.c_str();
            continue;
        case PAX_FILE_TYPE:
            getContentString(size, paxAttrs);
            continue;
        case PAX_GLOBAL_TYPE: {
                std::string pax;
                getContentString(size, pax);
                parsePaxAttributes(pax, m_defaults);
                continue;
            }
        case SYMTYPE:
            m_scratchEntry.m_linkname = (header.linkname[HEADER_NAME_LEN - 1] == 0 ?
                std::string(header.linkname) : std::string(header.linkname, HEADER_NAME_LEN));
            m_scratchEntry.m_filetype = TarEntry::SYMLINK;
            break;
        case DIRTYPE:
            m_scratchEntry.m_filetype = TarEntry::DIRECTORY;
            break;
        case REGTYPE:
        case AREGTYPE:
        // not supported type as regular file
        case LNKTYPE:
        case CHRTYPE:
        case BLKTYPE:
        case FIFOTYPE:
        case CONTTYPE:
        default:
            m_scratchEntry.m_filetype = TarEntry::REGULAR;
            break;
        }

        m_scratchEntry.m_size = size;
        m_scratchEntry.m_filename = (header.name[HEADER_NAME_LEN - 1] == 0 ?
            std::string(header.name) : std::string(header.name, HEADER_NAME_LEN));
        std::string prefix = (header.prefix[HEADER_PREFIX_LEN - 1] == 0 ?
            std::string(header.prefix) : std::string(header.prefix, HEADER_PREFIX_LEN));
        if (!prefix.empty()) {
            m_scratchEntry.m_filename = prefix + "/" + m_scratchEntry.m_filename;
        }

        // get metadata
#ifdef POSIX
        m_scratchEntry.m_mode = oct2dec<mode_t>(std::string(header.mode, sizeof(header.mode)));
        m_scratchEntry.m_uid = oct2dec<uid_t>(std::string(header.uid, sizeof(header.uid)));
        m_scratchEntry.m_gid = oct2dec<gid_t>(std::string(header.gid, sizeof(header.gid)));
#endif
        m_scratchEntry.m_mtime = oct2dec<time_t>(std::string(header.mtime, sizeof(header.mtime)));
        m_scratchEntry.m_uname = header.uname;
        m_scratchEntry.m_gname = header.gname;

        // gnu ext overrides
        if (!gnuLongName.empty()) {
            m_scratchEntry.m_filename = gnuLongName;
        }
        if (!gnuLongLink.empty()) {
            m_scratchEntry.m_linkname = gnuLongLink;
        }

        // global defaults overrides
        if (!m_defaults.m_filename.empty()) {
            m_scratchEntry.m_filename = m_defaults.m_filename;
        }
        if (!m_defaults.m_linkname.empty()) {
            m_scratchEntry.m_linkname = m_defaults.m_linkname;
        }
        if (m_defaults.m_size != -1) {
            m_scratchEntry.m_size = m_defaults.m_size;
        }
#ifdef POSIX
        if (m_defaults.m_uid != 0) {
            m_scratchEntry.m_uid = m_defaults.m_uid;
        }
        if (m_defaults.m_gid != 0) {
            m_scratchEntry.m_gid = m_defaults.m_gid;
        }
#endif
        if (!m_defaults.m_uname.empty()) {
            m_scratchEntry.m_uname = m_defaults.m_uname;
        }
        if (!m_defaults.m_gname.empty()) {
            m_scratchEntry.m_gname = m_defaults.m_gname;
        }
        if (m_defaults.m_atime != 0) {
            m_scratchEntry.m_atime = m_defaults.m_atime;
        }
        if (m_defaults.m_ctime != 0) {
            m_scratchEntry.m_ctime = m_defaults.m_ctime;
        }
        if (m_defaults.m_mtime != 0) {
            m_scratchEntry.m_mtime = m_defaults.m_mtime;
        }

        // local ext overrides
        parsePaxAttributes(paxAttrs, m_scratchEntry);

        // set data stream after size properly set
        if (m_scratchEntry.m_size > 0) {
            m_dataStream.reset(new LimitedStream(m_stream, m_scratchEntry.m_size, false));
        }
        m_scratchEntry.m_dataOffset = m_stream->tell();
        if (m_scratchEntry.m_dataOffset % BLOCK_SIZE != 0) {
            MORDOR_THROW_EXCEPTION(UnexpectedEofException());
        }

        MORDOR_LOG_DEBUG(g_log) << "get file entry: " << m_scratchEntry.m_filename;
        m_currentEntry = &m_scratchEntry;
        return m_currentEntry;
    }
    return NULL;
}

void
Tar::getContentString(long long size, std::string& str)
{
    str.clear();
    char buf[BLOCK_SIZE];
    int tail = size % BLOCK_SIZE;
    long long blocks = (size / BLOCK_SIZE) + (tail ? 1 : 0);
    // read by block
    while (blocks--) {
        if (m_stream->read(buf, BLOCK_SIZE) == BLOCK_SIZE) {
            if (blocks == 0 && tail != 0) {
                str += std::string(buf, tail);
            } else {
                str += std::string(buf, BLOCK_SIZE);
            }
        } else {
            break;
        }
    }
}

TarEntry&
Tar::addFile()
{
    if (m_currentEntry) {
        MORDOR_ASSERT(m_currentEntry == &m_scratchEntry);
        m_scratchEntry.close();
        m_scratchEntry.clear();
    } else {
        m_currentEntry = &m_scratchEntry;
    }
    return m_scratchEntry;
}

void
Tar::close()
{
    if (m_mode == READ) {
        return; // ignore for read mode
    }

    if (m_currentEntry) {
        MORDOR_ASSERT(m_currentEntry == &m_scratchEntry);
        m_scratchEntry.close();
        m_scratchEntry.clear();
    }

    // end of tar archive
    transferStream(ZeroStream::get_ptr(), m_stream, BLOCK_SIZE * 2);
    m_stream->close();
    MORDOR_LOG_DEBUG(g_log) << "all files done, tar archive (" << this << ") closed";
}

void
Tar::setGlobalAttribute(const std::string& key, const std::string& value) {
    m_defaults.setAttribute(key, value);
}

std::string
Tar::getGlobalAttribute(const std::string& key) const {
    return m_defaults.getAttribute(key);
}

}
