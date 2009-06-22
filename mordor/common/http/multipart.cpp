// Copyright (c) 2009 - Decho Corp.

#include "multipart.h"

#include <cassert>

#include <boost/bind.hpp>

#include "mordor/common/streams/notify.h"
#include "mordor/common/streams/null.h"
#include "mordor/common/streams/transfer.h"
#include "parser.h"

static const char *allowedBoundaryChars =
    "0123456789abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ'()+_,-./:=?";

std::string
Multipart::randomBoundary()
{
    std::string result;
    result.resize(70);
    for (size_t i = 0; i < 70; ++i) {
        result[i] = allowedBoundaryChars[rand() % 36];
    }
    return result;
}

Multipart::Multipart(Stream::ptr stream, std::string boundary)
: m_stream(stream),
  m_boundary(boundary),
  m_finished(false)
{
    assert(m_stream);
    assert(m_stream->supportsRead() || m_stream->supportsWrite());
    assert(!(m_stream->supportsRead() && m_stream->supportsWrite()));
    while (!m_boundary.empty() && m_boundary[m_boundary.size() - 1] == ' ')
        m_boundary.resize(m_boundary.size() - 1);
    assert(!m_boundary.empty());
    assert(m_boundary.size() <= 70);
    if (m_boundary.find_first_not_of(allowedBoundaryChars) != std::string::npos) {
        if (stream->supportsWrite()) {
            assert(false);
        } else {
            throw std::runtime_error("Invalid multipart boundary");
        }
    }
    m_boundary = "\r\n--" + m_boundary;
    if (m_stream->supportsRead()) {
        assert(m_stream->supportsFind());
        assert(m_stream->supportsUnread());
    }
}

BodyPart::ptr
Multipart::nextPart()
{
    if (m_stream->supportsWrite()) {
        assert(!m_finished);
        assert(!m_currentPart);
        m_currentPart.reset(new BodyPart(shared_from_this()));
        std::string boundary = m_boundary + "\r\n";
        m_stream->write(boundary.c_str(), boundary.size());
        return m_currentPart;
    } else {
        if (m_finished) {
            assert(!m_currentPart);
            return m_currentPart;
        }
        if (m_currentPart) {
            transferStream(m_currentPart->stream(), NullStream::get());
            // Changed by the notification callback
            assert(!m_currentPart);
        }
        size_t offsetToBoundary = m_stream->find(m_boundary);

        Buffer b;
        size_t result = m_stream->read(b, offsetToBoundary + m_boundary.size());
        assert(result == offsetToBoundary + m_boundary.size());
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
            if (restOfLine.find_first_not_of(" \r\t") != std::string::npos) {
                throw std::runtime_error("Invalid multipart boundary");
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
    assert(m_stream->supportsWrite());
    assert(!m_finished);
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

#ifdef min
#undef min
#endif

class BodyPartStream : public FilterStream
{
public:
    BodyPartStream(FilterStream::ptr parent, std::string boundary)
        : FilterStream(parent),
          m_boundary(boundary)
    {}

    size_t read(Buffer &b, size_t len)
    {
        size_t boundary = find(m_boundary, len, false);
        if (boundary != (size_t)~0)
            len = std::min(boundary, len);
        return FilterStream::read(b, len);
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
        if (!parser.complete() || parser.error()) {
            throw std::runtime_error("Invalid multipart headers.");
        }
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
        assert(m_headers.contentType.type != "multipart");
    }
    if (!m_stream) {
        assert(m_multipart->m_stream->supportsWrite());
        std::ostringstream os;
        os << m_headers << "\r\n";
        std::string headers = os.str();
        m_multipart->m_stream->write(headers.c_str(), headers.size());
        NotifyStream *notify = new NotifyStream(m_multipart->m_stream, false);
        notify->notifyOnClose = boost::bind(&Multipart::partDone, m_multipart);
        m_stream.reset(notify);
    }
    return m_stream;
}

Multipart::ptr
BodyPart::multipart()
{
    if (m_childMultipart)
        return m_childMultipart;
    assert(m_headers.contentType.type == "multipart");
    if (!m_stream) {
        assert(m_multipart->m_stream->supportsWrite());
        std::ostringstream os;
        os << m_headers;
        std::string headers = os.str();
        m_multipart->m_stream->write(headers.c_str(), headers.size());
        NotifyStream *notify = new NotifyStream(m_multipart->m_stream, false);
        notify->notifyOnClose = boost::bind(&Multipart::partDone, m_multipart);
        m_stream.reset(notify);
    }
    HTTP::StringMap::const_iterator it = m_headers.contentType.parameters.find("boundary");
    if (it == m_headers.contentType.parameters.end()) {
        assert(!m_multipart->m_stream->supportsWrite());
        throw std::runtime_error("No boundary with multipart");
    }
    m_childMultipart.reset(new Multipart(m_stream, it->second));
    return m_childMultipart;
}
