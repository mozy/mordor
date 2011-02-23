#ifndef __MORDOR_YAML_H__
#define __MORDOR_YAML_H__
// Copyright (c) 2010 - Mozy, Inc.

#include <string>

#include <boost/shared_ptr.hpp>

#include "exception.h"
#include "json.h"

namespace Mordor {
class Stream;

namespace YAML {

struct Exception : virtual Mordor::Exception
{
public:
    Exception(const char *problem, const char *context)
        : m_problem(problem),
          m_context(context)
    {}

    const char *what() const throw() { return m_problem; }

private:
    const char *m_problem;
    const char *m_context;
};

JSON::Value parse(const std::string &string);
JSON::Value parse(Stream &stream);
JSON::Value parse(boost::shared_ptr<Stream> stream);

}}

#endif
