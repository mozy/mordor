// Copyright (c) 2010 - Mozy, Inc.

#include "socks.h"

#include "mordor/http/broker.h"
#include "mordor/socket.h"
#include "mordor/streams/stream.h"

namespace Mordor {
namespace SOCKS {

Stream::ptr tunnel(HTTP::StreamBroker::ptr streamBroker, const URI &proxy,
    IPAddress::ptr targetIP, const std::string &targetDomain,
    unsigned short targetPort, unsigned char version)
{
    MORDOR_ASSERT(version == 4 || version == 5);
    MORDOR_ASSERT(version == 5 || !targetIP ||
        targetIP->family() == AF_INET);

    MORDOR_ASSERT(streamBroker);
    MORDOR_ASSERT(targetIP || !targetDomain.empty());
    std::string buffer;
    buffer.resize(std::max<size_t>(targetDomain.size() + 1u, 16u) + 9);

    Stream::ptr stream = streamBroker->getStream(proxy);
    if (version == 5) {
        buffer[0] = version;
        buffer[1] = 1;
        buffer[2] = 0;
        size_t written = 0;
        while (written < 3)
            written += stream->write(buffer.data() + written, 3 - written);
        if (stream->read(&buffer[0], 1) == 0)
            MORDOR_THROW_EXCEPTION(UnexpectedEofException());
        if (buffer[0] != 5)
            MORDOR_THROW_EXCEPTION(ProtocolViolationException());
        if (stream->read(&buffer[0], 1) == 0)
            MORDOR_THROW_EXCEPTION(UnexpectedEofException());
        if ((unsigned char)buffer[0] == 0xff)
            MORDOR_THROW_EXCEPTION(NoAcceptableAuthenticationMethodException());
        if (buffer[0] != 0)
            MORDOR_THROW_EXCEPTION(ProtocolViolationException());
    }
    buffer[0] = version;
    buffer[1] = 1;
    size_t size;
    if (version == 4) {
        if (targetIP)
            *(unsigned short *)&buffer[2] = byteswapOnLittleEndian(targetIP->port());
        else
            *(unsigned short *)&buffer[2] = byteswapOnLittleEndian(targetPort);
        if (targetIP)
            *(unsigned int *)&buffer[4] = byteswapOnLittleEndian((unsigned int)(((sockaddr_in *)targetIP->name())->sin_addr.s_addr));
        else
            *(unsigned int *)&buffer[4] = byteswapOnLittleEndian(0x00000001);
        buffer[8] = 0;
        if (!targetIP) {
            memcpy(&buffer[9], targetDomain.c_str(), targetDomain.size());
            buffer[9 + targetDomain.size()] = 0;
        }
        size = 9 + targetDomain.size() + (targetDomain.empty() ? 0 : 1);
    } else {
        buffer[2] = 0;
        if (targetIP) {
            if (targetIP->family() == AF_INET) {
                buffer[3] = 1;
                *(unsigned int *)&buffer[4] = byteswapOnLittleEndian((unsigned int)(((sockaddr_in *)targetIP->name())->sin_addr.s_addr));
                size = 7;
            } else {
                buffer[3] = 4;
                memcpy(&buffer[4], &((sockaddr_in6 *)targetIP->name())->sin6_addr, 16);
                size = 19;
            }
        } else {
            buffer[3] = 3;
            buffer[4] = (unsigned char)targetDomain.size();
            memcpy(&buffer[5], targetDomain.c_str(), targetDomain.size());
            size = 5 + targetDomain.size();
        }
        if (targetIP)
            *(unsigned short *)&buffer[size] = byteswapOnLittleEndian(targetIP->port());
        else
            *(unsigned short *)&buffer[size] = byteswapOnLittleEndian(targetPort);
        size += 2;
    }
    size_t written = 0;
    while (written < size)
        written += stream->write(buffer.data() + written, size - written);
    if (stream->read(&buffer[0], 1) == 0)
        MORDOR_THROW_EXCEPTION(UnexpectedEofException());
    if ((version == 4 && buffer[0] != 0) || (version == 5 && buffer[0] != 5))
        MORDOR_THROW_EXCEPTION(ProtocolViolationException());
    if (stream->read(&buffer[0], 1) == 0)
        MORDOR_THROW_EXCEPTION(UnexpectedEofException());
    if ((version == 4 && buffer[0] != 0x5a) || (version == 5 && buffer[0] != 0))
        MORDOR_THROW_EXCEPTION(InvalidResponseException(buffer[0]));
    if (version == 4) {
        size = 0;
        while (size < 6) {
            written = stream->read(&buffer[0], 6 - size);
            if (written == 0)
                MORDOR_THROW_EXCEPTION(UnexpectedEofException());
            size += written;
        }
    } else {
        if (stream->read(&buffer[0], 1) == 0)
            MORDOR_THROW_EXCEPTION(UnexpectedEofException());
        if (buffer[0] != 0)
            MORDOR_THROW_EXCEPTION(InvalidResponseException(buffer[0]));
        if (stream->read(&buffer[0], 1) == 0)
            MORDOR_THROW_EXCEPTION(UnexpectedEofException());
        if (buffer[1] == 3) {
            if (buffer[0] != 0)
                MORDOR_THROW_EXCEPTION(ProtocolViolationException());
            size = buffer[1] + 2;
        } else if (buffer[1] == 1) {
            size = 6;
        } else if (buffer[1] == 4) {
            size = 18;
        } else {
            MORDOR_THROW_EXCEPTION(ProtocolViolationException());
        }
        while (size > 0) {
            written = stream->read(&buffer[0], size);
            if (written == 0)
                MORDOR_THROW_EXCEPTION(UnexpectedEofException());
            size -= written;
        }
    }
    return stream;
}

}}
