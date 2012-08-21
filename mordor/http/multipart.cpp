// Copyright (c) 2009 - Mozy, Inc.

#include "multipart.h"

#include <boost/bind.hpp>

#include "mordor/assert.h"
#include "mordor/streams/buffer.h"
#include "mordor/streams/buffered.h"
#include "mordor/streams/notify.h"
#include "mordor/streams/null.h"
#include "mordor/streams/transfer.h"
#include "parser.h"

namespace Mordor {

static const char allowedBoundaryChars[] =
    "0123456789abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ'()+_,-./:=?";

std::string
Multipart::randomBoundary()
{
    std::string result;
    result.resize(40);
    for (size_t i = 0; i < 40; ++i)
        result[i] = allowedBoundaryChars[rand() % 74];
    return result;
}

Multipart::Multipart(Stream::ptr stream, std::string boundary)
: m_stream(stream),
  m_boundary(boundary),
  m_finished(false)
{
    MORDOR_ASSERT(m_stream);
    MORDOR_ASSERT(m_stream->supportsRead() || m_stream->supportsWrite());
    MORDOR_ASSERT(!(m_stream->supportsRead() && m_stream->supportsWrite()));
    while (!m_boundary.empty() && m_boundary[m_boundary.size() - 1] == ' ')
        m_boundary.resize(m_boundary.size() - 1);
    MORDOR_ASSERT(!m_boundary.empty());
    MORDOR_ASSERT(m_boundary.size() <= 70);
    if (m_boundary.find_first_not_of(allowedBoundaryChars) != std::string::npos) {
        if (stream->supportsWrite()) {
            MORDOR_NOTREACHED();
        } else {
            MORDOR_THROW_EXCEPTION(InvalidMultipartBoundaryException());
        }
    }
    m_boundary = "\r\n--" + m_boundary;
    if (m_stream->supportsRead()) {
        if (!m_stream->supportsFind())
            m_stream.reset(new BufferedStream(m_stream));
        MORDOR_ASSERT(m_stream->supportsFind());
        MORDOR_ASSERT(m_stream->supportsUnread());
    }
}

BodyPart::ptr
Multipart::nextPart()
{
    if (m_stream->supportsWrite()) {
        MORDOR_ASSERT(!m_finished);
        MORDOR_ASSERT(!m_currentPart);
        m_currentPart.reset(new BodyPart(shared_from_this()));
        std::string boundary = m_boundary + "\r\n";
        m_stream->write(boundary.c_str(), boundary.size());
        return m_currentPart;
    } else {
        if (m_finished) {
            MORDOR_ASSERT(!m_currentPart);
            return m_currentPart;
        }
        if (m_currentPart) {
            transferStream(m_currentPart->stream(), NullStream::get());
            // Changed by the notification callback
            MORDOR_ASSERT(!m_currentPart);
        }
        size_t offsetToBoundary = m_stream->find(m_boundary);

        Buffer b;
        size_t result = m_stream->read(b, offsetToBoundary + m_boundary.size());
        MORDOR_ASSERT(result == offsetToBoundary + m_boundary.size());
        b.clear();
        result = m_stream->read(b, 2);
        if (b == "--") {
            m_finished = true;
        }
        if (b == "\n") {
            m_stream->unread(b, 1);
        }
        if (b != "\r\n") {
            std::string restOfLine = m_stream->getDelimited();
            MORDOR_ASSERT(!restOfLine.empty());
            restOfLine.resize(restOfLine.size() - 1);
            if (restOfLine.find_first_not_of(" \r\t") != std::string::npos) {
                MORDOR_THROW_EXCEPTION(InvalidMultipartBoundaryException());
            }
        }

        if (m_finished) {
            if (multipartFinished)
                multipartFinished();
            return m_currentPart;
        }
        m_currentPart.reset(new BodyPart(shared_from_this()));
        return m_currentPart;
    }
}

void
Multipart::finish()
{
    MORDOR_ASSERT(m_stream->supportsWrite());
    MORDOR_ASSERT(!m_finished);
    std::string finalBoundary = m_boundary + "--\r\n";
    m_stream->write(finalBoundary.c_str(), finalBoundary.size());
    m_finished = true;
    if (multipartFinished)
        multipartFinished();
}

void
Multipart::partDone()
{
    m_currentPart.reset();
}

class BodyPartStream : public MutatingFilterStream
{
public:
    BodyPartStream(Stream::ptr parent, std::string boundary)
        : MutatingFilterStream(parent),
          m_boundary(boundary)
    {}

    using MutatingFilterStream::read;
    size_t read(Buffer &b, size_t len)
    {
        ptrdiff_t boundary = parent()->find(m_boundary, len, false);
        if (boundary >= 0)
            len = (std::min)((size_t)boundary, len);
        return parent()->read(b, len);
    }

private:
    std::string m_boundary;
};

BodyPart::BodyPart(Multipart::ptr multipart)
: m_multipart(multipart)
{
    if (m_multipart->m_stream->supportsRead()) {
        HTTP::TrailerParser parser(m_headers);
        parser.run(m_multipart->m_stream);
        if (parser.error())
            MORDOR_THROW_EXCEPTION(HTTP::BadMessageHeaderException());
        if (!parser.complete())
            MORDOR_THROW_EXCEPTION(HTTP::IncompleteMessageHeaderException());
        m_stream.reset(new BodyPartStream(m_multipart->m_stream, m_multipart->m_boundary));
        NotifyStream *notify = new NotifyStream(m_stream);
        notify->notifyOnEof = boost::bind(&Multipart::partDone, m_multipart);
        m_stream.reset(notify);
    }
}

HTTP::EntityHeaders &
BodyPart::headers()
{
    return m_headers;
}

Stream::ptr
BodyPart::stream()
{
    if (m_multipart->m_stream->supportsWrite()) {
        MORDOR_ASSERT(m_headers.contentType.type != "multipart");
    }
    if (!m_stream) {
        MORDOR_ASSERT(m_multipart->m_stream->supportsWrite());
        std::ostringstream os;
        os << m_headers << "\r\n";
        std::string headers = os.str();
        m_multipart->m_stream->write(headers.c_str(), headers.size());
        NotifyStream *notify = new NotifyStream(m_multipart->m_stream, false);
        notify->notifyOnClose(boost::bind(&Multipart::partDone, m_multipart));
        m_stream.reset(notify);
    }
    return m_stream;
}

Multipart::ptr
BodyPart::multipart()
{
    if (m_childMultipart)
        return m_childMultipart;
    MORDOR_ASSERT(m_headers.contentType.type == "multipart");
    if (!m_stream) {
        MORDOR_ASSERT(m_multipart->m_stream->supportsWrite());
        std::ostringstream os;
        os << m_headers;
        std::string headers = os.str();
        m_multipart->m_stream->write(headers.c_str(), headers.size());
        NotifyStream *notify = new NotifyStream(m_multipart->m_stream, false);
        notify->notifyOnClose(boost::bind(&Multipart::partDone, m_multipart));
        m_stream.reset(notify);
    }
    HTTP::StringMap::const_iterator it = m_headers.contentType.parameters.find("boundary");
    if (it == m_headers.contentType.parameters.end()) {
        MORDOR_ASSERT(!m_multipart->m_stream->supportsWrite());
        MORDOR_THROW_EXCEPTION(InvalidMultipartBoundaryException());
    }
    m_childMultipart.reset(new Multipart(m_stream, it->second));
    return m_childMultipart;
}

}
