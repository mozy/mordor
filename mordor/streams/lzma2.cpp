#include "lzma2.h"

#include "mordor/assert.h"
#include "mordor/exception.h"
#include "mordor/log.h"

#ifdef MSVC
#pragma comment(lib, "liblzma")
#endif

namespace Mordor {

static Logger::ptr g_log = Log::lookup("mordor:streams:lzma2");

LZMAStream::LZMAStream(Stream::ptr parent, uint32_t preset, lzma_check check, bool own)
    : MutatingFilterStream(parent, own)
{
    lzma_stream strm = LZMA_STREAM_INIT;
    m_strm = strm;
    lzma_ret ret;
    if (supportsRead()) {
        ret = lzma_stream_decoder(&m_strm, UINT64_MAX, LZMA_CONCATENATED);
    } else {
        lzma_options_lzma opt_lzma2;
        if (lzma_lzma_preset(&opt_lzma2, preset)) {
            MORDOR_THROW_EXCEPTION(std::invalid_argument("unsupported option"));
        }
        lzma_filter filters[] = {
#if defined(X86_64) || defined(X86)
            { LZMA_FILTER_X86, NULL },
#elif defined(PPC)
            { LZMA_FILTER_POWERPC, NULL },
#elif defined(ARM)
#if defined(__thumb__)
            { LZMA_FILTER_ARMTHUMB, NULL },
#else
            { LZMA_FILTER_ARM, NULL },
#endif
#endif
            { LZMA_FILTER_LZMA2, &opt_lzma2 },
            { LZMA_VLI_UNKNOWN, NULL },
        };
        ret = lzma_stream_encoder(&m_strm, filters, check);
    }
    switch (ret) {
    case LZMA_OK:
        m_closed = false;
        break;
    case LZMA_MEM_ERROR:
        MORDOR_THROW_EXCEPTION(std::bad_alloc());
        break;
    case LZMA_OPTIONS_ERROR:
        MORDOR_THROW_EXCEPTION(std::invalid_argument("unsupported option"));
        break;
    case LZMA_UNSUPPORTED_CHECK:
        MORDOR_THROW_EXCEPTION(std::invalid_argument("unsupported check"));
        break;
    default:
        MORDOR_THROW_EXCEPTION(std::runtime_error("unknown lzma error"));
        break;
    }
}

LZMAStream::~LZMAStream()
{
    lzma_end(&m_strm);
}

void
LZMAStream::close(CloseType type)
{
    if ((type == READ && supportsWrite()) ||
        (type == WRITE && supportsRead()) ||
        m_closed) {
        if (ownsParent())
            parent()->close(type);
        return;
    }

    finish();

    if (ownsParent())
        parent()->close(type);
}

size_t
LZMAStream::read(Buffer &buffer, size_t length)
{
    struct iovec outbuf = buffer.writeBuffer(length, false);
    m_strm.next_out = (uint8_t*)outbuf.iov_base;
    m_strm.avail_out = outbuf.iov_len;

    for (;;) {
        std::vector<iovec> inbufs = m_inBuffer.readBuffers();
        if (inbufs.empty()) {
            m_strm.next_in = NULL;
            m_strm.avail_in = 0;
        } else {
            m_strm.next_in = (uint8_t*)inbufs[0].iov_base;
            m_strm.avail_in = inbufs[0].iov_len;
        }
        lzma_ret rc = lzma_code(&m_strm, LZMA_RUN);
        MORDOR_LOG_DEBUG(g_log) << this << " lzma_code(("
            << (inbufs.empty() ? 0 : inbufs[0].iov_len) << ", "
            << outbuf.iov_len << ")): " << rc << " (" << m_strm.avail_in
            << ", " << m_strm.avail_out << ")";
        if (!inbufs.empty())
            m_inBuffer.consume(inbufs[0].iov_len - m_strm.avail_in);
        size_t result;
        switch (rc) {
            case LZMA_BUF_ERROR:
                // no progress...
                MORDOR_ASSERT(m_strm.avail_in == 0);
                MORDOR_ASSERT(inbufs.empty());
                result = parent()->read(m_inBuffer, BUFFER_SIZE);
                // the end of input file has been reached, and since we are in
                // LZMA_CONCATENATED mode, we need to tell lzma_code() that no
                // more input will be coming.
                if (result != 0)
                    break;
                finish();
                return result;
            case LZMA_STREAM_END:
                // May have still produced output
                result = outbuf.iov_len - m_strm.avail_out;
                buffer.produce(result);
                finish();
                return result;
            case LZMA_OK:
                result = outbuf.iov_len - m_strm.avail_out;
                // It consumed input, but produced no output... DON'T return eof
                if (result == 0)
                    continue;
                buffer.produce(result);
                return result;
            case LZMA_MEM_ERROR:
                MORDOR_THROW_EXCEPTION(std::bad_alloc());
            case LZMA_FORMAT_ERROR:
                MORDOR_THROW_EXCEPTION(UnknownLZMAFormatException());
            case LZMA_DATA_ERROR:
                MORDOR_THROW_EXCEPTION(CorruptedLZMAStreamException());
            default:
                MORDOR_THROW_EXCEPTION(LZMAException(rc));
        }
    }
}

size_t
LZMAStream::write(const Buffer &buffer, size_t length)
{
    MORDOR_ASSERT(!m_closed);
    flushBuffer();
    for (;;) {
        if (m_outBuffer.writeAvailable() == 0)
            m_outBuffer.reserve(BUFFER_SIZE);
        struct iovec inbuf = buffer.readBuffer(length, false);
        struct iovec outbuf = m_outBuffer.writeBuffer(~0u, false);
        m_strm.next_in = (uint8_t*)inbuf.iov_base;
        m_strm.avail_in = inbuf.iov_len;
        m_strm.next_out = (uint8_t*)outbuf.iov_base;
        m_strm.avail_out = outbuf.iov_len;
        lzma_ret rc = lzma_code(&m_strm, LZMA_RUN);
        MORDOR_LOG_DEBUG(g_log) << this << " lzma_code((" << inbuf.iov_len << ", "
            << outbuf.iov_len << "), LZMA_RUN): " << rc << " ("
            << m_strm.avail_in << ", " << m_strm.avail_out << ")";
        // We are always providing both input and output
        MORDOR_ASSERT(rc != LZMA_BUF_ERROR);
        // We're not doing LZMA_FINISH, so we shouldn't get EOF
        MORDOR_ASSERT(rc != LZMA_STREAM_END);
        MORDOR_ASSERT(rc == LZMA_OK);
        size_t result = inbuf.iov_len - m_strm.avail_in;
        if (result == 0)
            continue;
        m_outBuffer.produce(outbuf.iov_len - m_strm.avail_out);
        try {
            flushBuffer();
        } catch (std::runtime_error) {
            // Swallow it
        }
        return result;
    }
}

void
LZMAStream::flush(bool flushParent)
{
    flushBuffer();
    if (flushParent)
        parent()->flush();
}

void
LZMAStream::flushBuffer()
{
    while (m_outBuffer.readAvailable() > 0)
        m_outBuffer.consume(parent()->write(m_outBuffer,
            m_outBuffer.readAvailable()));
}


void
LZMAStream::finish()
{
    if (m_outBuffer.writeAvailable() == 0)
        m_outBuffer.reserve(BUFFER_SIZE);
    iovec outbuf = m_outBuffer.writeBuffer(~0u, false);
    MORDOR_ASSERT(m_strm.avail_in == 0);
    m_strm.next_out = (uint8_t*)outbuf.iov_base;
    m_strm.avail_out = outbuf.iov_len;
    lzma_ret rc = lzma_code(&m_strm, LZMA_FINISH);
    MORDOR_ASSERT(m_strm.avail_in == 0);
    MORDOR_LOG_DEBUG(g_log) << this << " lzma_code(0, " << outbuf.iov_len
        << "): " << rc << " (0, " << m_strm.avail_out << ")";
    m_outBuffer.produce(outbuf.iov_len - m_strm.avail_out);
    if (rc != LZMA_STREAM_END) {
        MORDOR_THROW_EXCEPTION(LZMAException(rc));
    }
    m_closed = true;
    flushBuffer();
}

}
