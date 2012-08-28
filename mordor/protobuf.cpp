// Copyright (c) 2010 - Mozy, Inc.

#include "protobuf.h"

#include "mordor/assert.h"
#include "mordor/streams/buffer.h"

#ifdef MSVC
// Disable some warnings, but only while
// processing the google generated code
#pragma warning(push)
#pragma warning(disable : 4244)
#endif

#include <google/protobuf/io/zero_copy_stream.h>
#include <google/protobuf/message.h>

#ifdef MSVC
#pragma warning(pop)
#endif

using namespace google::protobuf;

#ifdef MSVC
#ifdef _DEBUG
#pragma comment(lib, "libprotobuf-d.lib")
#else
#pragma comment(lib, "libprotobuf.lib")
#endif
#endif

namespace Mordor {

class BufferZeroCopyInputStream : public io::ZeroCopyInputStream
{
public:
    BufferZeroCopyInputStream(const Buffer &buffer)
        : m_iovs(buffer.readBuffers()),
          m_currentIov(0),
          m_currentIovOffset(0),
          m_complete(0)
    {}

    bool Next(const void **data, int *size)
    {
        if (m_currentIov >= m_iovs.size())
            return false;
        MORDOR_ASSERT(m_currentIovOffset <= m_iovs[m_currentIov].iov_len);
        *data = (char *)m_iovs[m_currentIov].iov_base + m_currentIovOffset;
        *size = (int)(m_iovs[m_currentIov].iov_len - m_currentIovOffset);

        m_complete += *size;
        m_currentIovOffset = 0;
        ++m_currentIov;

        return true;
    }

    void BackUp(int count)
    {
        MORDOR_ASSERT(count >= 0);
        MORDOR_ASSERT(count <= m_complete);
        m_complete -= count;
        while (count) {
            if (m_currentIovOffset == 0) {
                MORDOR_ASSERT(m_currentIov > 0);
                m_currentIovOffset = m_iovs[--m_currentIov].iov_len;
            }
            size_t todo = (std::min)(m_currentIovOffset, (size_t)count);
            m_currentIovOffset -= todo;
            count -= (int)todo;
        }
    }

    bool Skip(int count) {
        MORDOR_ASSERT(count >= 0);
        while (count) {
            if (m_currentIov >= m_iovs.size())
                return false;
            size_t todo = (std::min)((size_t)m_iovs[m_currentIov].iov_len -
                m_currentIovOffset, (size_t)count);
            m_currentIovOffset += todo;
            count -= (int)todo;
            m_complete += todo;
            if (m_currentIovOffset == m_iovs[m_currentIov].iov_len) {
                m_currentIovOffset = 0;
                ++m_currentIov;
            }
        }
        return true;
    }

    int64 ByteCount() const { return m_complete; }

private:
    std::vector<iovec> m_iovs;
    size_t m_currentIov;
    size_t m_currentIovOffset;
    int64 m_complete;
};

class BufferZeroCopyOutputStream : public io::ZeroCopyOutputStream
{
public:
    BufferZeroCopyOutputStream(Buffer &buffer, size_t bufferSize = 1024)
        : m_buffer(buffer),
          m_bufferSize(bufferSize),
          m_pendingProduce(0),
          m_total(0)
    {}
    ~BufferZeroCopyOutputStream()
    {
        m_buffer.produce(m_pendingProduce);
    }

    bool Next(void **data, int *size)
    {
        m_buffer.produce(m_pendingProduce);
        m_pendingProduce = 0;

        // TODO: protect against std::bad_alloc?
        iovec iov = m_buffer.writeBuffer(m_bufferSize, false);
        *data = iov.iov_base;
        m_total += m_pendingProduce = iov.iov_len;
        *size = (int)m_pendingProduce;

        return true;
    }

    void BackUp(int count)
    {
        MORDOR_ASSERT(count <= (int)m_pendingProduce);
        m_pendingProduce -= count;
        m_total -= count;
    }

    int64 ByteCount() const
    {
        return m_total;
    }

private:
    Buffer &m_buffer;
    size_t m_bufferSize, m_pendingProduce;
    int64 m_total;
};

void serializeToBuffer(const Message &proto, Buffer &buffer)
{
    BufferZeroCopyOutputStream stream(buffer);
    if (!proto.SerializeToZeroCopyStream(&stream))
        MORDOR_THROW_EXCEPTION(std::invalid_argument("proto"));
}

void parseFromBuffer(Message &proto, const Buffer &buffer)
{
    BufferZeroCopyInputStream stream(buffer);
    if (!proto.ParseFromZeroCopyStream(&stream))
        MORDOR_THROW_EXCEPTION(std::invalid_argument("buffer"));
}

}
