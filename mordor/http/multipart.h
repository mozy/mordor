#ifndef __MORDOR_MULTIPART_H__
#define __MORDOR_MULTIPART_H__
// Copyright (c) 2009 - Decho Corporation

#include <string>

#include <boost/enable_shared_from_this.hpp>
#include <boost/function.hpp>
#include <boost/noncopyable.hpp>
#include <boost/shared_ptr.hpp>

#include "http.h"

namespace Mordor {

class BodyPart;
class Stream;

struct MissingMultipartBoundaryException : virtual HTTP::Exception, virtual StreamException
{};
struct InvalidMultipartBoundaryException : virtual HTTP::Exception
{};

class Multipart : public boost::enable_shared_from_this<Multipart>, boost::noncopyable
{
    friend class BodyPart;
public:
    typedef boost::shared_ptr<Multipart> ptr;

    static std::string randomBoundary();

    Multipart(boost::shared_ptr<Stream> stream, std::string boundary);

    boost::shared_ptr<BodyPart> nextPart();
    void finish();

    boost::function<void ()> multipartFinished;

private:
    void partDone();

private:
    boost::shared_ptr<Stream> m_stream;
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
    boost::shared_ptr<Stream> stream();
    Multipart::ptr multipart();

private:
    HTTP::EntityHeaders m_headers;
    Multipart::ptr m_multipart;
    boost::shared_ptr<Stream> m_stream;
    Multipart::ptr m_childMultipart;
};

}

#endif
