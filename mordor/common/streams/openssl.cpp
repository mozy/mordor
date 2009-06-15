// Copyright (c) 2009 - Decho Corp.

#include "openssl.h"

OpenSSLStream::OpenSSLStream(BIO *bio, bool own)
: m_bio(bio),
  m_own(own)
{
    assert(bio);
}

OpenSSLStream::~OpenSSLStream()
{
    close();
}

void
OpenSSLStream::close(CloseType type)
{
    if (m_bio && m_own && type == Stream::BOTH) {
        BIO_free_all(m_bio);
        m_bio = NULL;
    }
}

size_t
OpenSSLStream::read(Buffer &b, size_t len)
{
    assert(m_bio);
    std::vector<iovec> bufs = b.writeBufs(len);
    assert(!bufs.empty());
    int toRead = (int)std::min<size_t>(0x0fffffff, bufs[0].iov_len);
    int result = BIO_read(m_bio, bufs[0].iov_base, toRead);
    if (result > 0) {
        b.produce(result);
        return result;
    } else if (BIO_eof(m_bio)) {
        return 0;
    } else {
        assert(false);
        return -1;
    }
}

size_t
OpenSSLStream::write(const Buffer &b, size_t len)
{
    assert(m_bio);
    std::vector<iovec> bufs = b.readBufs(len);
    assert(!bufs.empty());
    int toWrite = (int)std::min<size_t>(0x0fffffff, bufs[0].iov_len);
    int result = BIO_read(m_bio, bufs[0].iov_base, toWrite);
    if (result > 0) {
        return result;
    } else {
        assert(false);
        return -1;
    }
}

long long
OpenSSLStream::seek(long long offset, Anchor anchor)
{
    assert(m_bio);
    assert(anchor == BEGIN || anchor == CURRENT);
    if (anchor == CURRENT && offset == 0) {
        int result = BIO_tell(m_bio);
        assert(result >= 0);
        return result;
    } else if (anchor == CURRENT) {
        int cur = BIO_tell(m_bio);
        assert(cur >= 0);
        offset += cur;
        anchor = BEGIN;
    } 
    if (anchor == BEGIN) {
        int result = BIO_seek(m_bio, (int)offset);
        assert(result >= 0);
        return result;
    }
    assert(false);
    return 0;
}

void
OpenSSLStream::flush()
{
    int result = BIO_flush(m_bio);
    assert(result == 1);
}

static int stream_write(BIO *bio, const char *buf, int size);
static int stream_read(BIO *bio, char *buf, int size);
static long stream_ctrl(BIO *bio, int cmd, long arg1, void *arg2);
static int stream_new(BIO *bio);
static int stream_free(BIO *bio);

static BIO_METHOD g_s_stream =
{
    BIO_TYPE_STREAM,"stream",
    stream_write,
    stream_read,
    NULL,
    NULL,
    stream_ctrl,
    stream_new,
    stream_free,
    NULL,
};

BIO_METHOD *BIO_s_stream()
{
    return &g_s_stream;
}

BIO *BIO_new_stream(Stream::ptr parent, bool own)
{
    BIO *result = BIO_new(BIO_s_stream());
    if (!result) {
        return NULL;
    }
    BIO_set_stream(result, parent, own);
    return result;
}

static int stream_new(BIO *bio)
{
    bio->init = 0;
    bio->num = -1;
    bio->ptr = NULL;
    bio->flags = 0;
    return 1;
}

static int stream_free(BIO *bio)
{
    if (!bio)
        return 0;
    if (bio->shutdown) {
        if (bio->ptr) {
            delete (Stream::ptr *)bio->ptr;
        }
        bio->ptr = NULL;
        bio->flags = 0;
    }
    return 1;
}

static int stream_read(BIO *bio, char *out, int size)
{
    if (out) {
        try {
            // TODO: zero-copy
            Buffer b;
            int result = (int)(*(Stream::ptr *)bio->ptr)->read(b, size);
            if (result == 0) {
                // TODO: set EOF flag
            }
            b.copyOut(out, result);
            return result;
        } catch (std::exception) {
            return -1;
        }
    }
    return 0;
}

static int stream_write(BIO *bio, const char *in, int size)
{
    try {
        return (int)(*(Stream::ptr *)bio->ptr)->write(in, size);
    } catch (std::exception) {
        return -1;
    }
}

static long stream_ctrl(BIO *bio, int cmd, long arg1, void *arg2)
{
    long ret = 1;
    Stream::ptr stream;
    if (bio->ptr)
        stream = *(Stream::ptr *)bio->ptr;
    switch (cmd) {
        case BIO_C_FILE_SEEK:
            if (stream.get()) {
                if (stream->supportsSeek()) {
                    try {
                        ret = (long)stream->seek(arg1, Stream::BEGIN);
                    } catch (std::exception) {
                        ret = -1;
                    }
                } else {
                    ret = -1;
                }
            }
            break;
        case BIO_C_FILE_TELL:
        case BIO_CTRL_INFO:
            if (stream.get()) {
                if (stream->supportsSeek()) {
                    try {
                        ret = (long)stream->seek(0, Stream::CURRENT);
                    } catch (std::exception) {
                        ret = -1;
                    }
                } else {
                    ret = -1;
                }
            }
            break;
        case BIO_C_SET_STREAM:
            if (bio->ptr) {
                delete (Stream::ptr *)bio->ptr;
            }
            bio->ptr = new Stream::ptr(*(Stream::ptr *)arg2);
            bio->shutdown = arg1;
            bio->init = 1;
            break;
        case BIO_C_GET_STREAM:
            if (bio->init) {
                if (arg2)
                    *(void**)arg2 = bio->ptr;
                ret = (long)bio->ptr;
            } else {
                ret = -1;
            }
            break;
        case BIO_CTRL_GET_CLOSE:
            ret = bio->shutdown;
            break;
        case BIO_CTRL_SET_CLOSE:
            bio->shutdown = (int)arg1;
            break;
        case BIO_CTRL_FLUSH:
            if (stream.get()) {
                try {
                    stream->flush();
                } catch (std::exception) {
                    ret = -1;
                }
            }
            break;
        case BIO_CTRL_DUP:
            if (stream.get()) {
                bio->ptr = new Stream::ptr(stream);
            }
            break;
        default:
            ret = 0;
            break;
    }
    return ret;
}
