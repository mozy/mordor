#ifndef __MULTIPART_H__
#define __MULTIPART_H__
// Copyright (c) 2009 - Decho Corp.

#include <string>

#include <boost/enable_shared_from_this.hpp>
#include <boost/function.hpp>
#include <boost/noncopyable.hpp>
#include <boost/shared_ptr.hpp>

#include "http.h"
#include "mordor/common/streams/stream.h"

class BodyPart;

struct MissingMultipartBoundaryException : virtual HTTPException, virtual StreamException
{};
struct InvalidMultipartBoundaryException : virtual HTTPException
{};

class Multipart : public boost::enable_shared_from_this<Multipart>, boost::noncopyable
{
    friend class BodyPart;
public:
    typedef boost::shared_ptr<Multipart> ptr;

    static std::string randomBoundary();

    Multipart(Stream::ptr stream, std::string boundary);

    boost::shared_ptr<BodyPart> nextPart();
    void finish();

    boost::function<void ()> multipartFinished;

private:
    void partDone();

private:
    Stream::ptr m_stream;
    std::string m_boundary;
    boost::shared_ptr<BodyPart> m_currentPart;
    bool m_finished;
};

class BodyPart
{
    friend class Multipart;
public:
    typedef boost::shared_ptr<BodyPart> ptr;

private:
    BodyPart(Multipart::ptr multipart);

public:
    HTTP::EntityHeaders &headers();
    Stream::ptr stream();
    Multipart::ptr multipart();

private:
    HTTP::EntityHeaders m_headers;
    Multipart::ptr m_multipart;
    Stream::ptr m_stream;
    Multipart::ptr m_childMultipart;
};

#endif
