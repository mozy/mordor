#ifndef __MORDOR_PQ_RESULT_H__
#define __MORDOR_PQ_RESULT_H__
// Copyright (c) 2010 - Mozy, Inc.

#include <boost/shared_ptr.hpp>

#include <libpq-fe.h>

namespace Mordor {
namespace PQ {

class Connection;
class PreparedStatement;

class Result
{
    friend class Connection;
    friend class PreparedStatement;
private:
    Result(boost::shared_ptr<PGresult> result)
        : m_result(result)
    {}

public:
    size_t rows() const;
    size_t columns() const;

    size_t column(const char *name) const;
    size_t column(const std::string &name) const
    { return column(name.c_str()); }

    Oid getType(size_t column) const;
    Oid getType(const char *col) const
    { return getType(column(col)); }
    Oid getType(const std::string &col) const
    { return getType(column(col)); }

    bool getIsNull(size_t row, size_t column) const;
    bool getIsNull(size_t row, const char *col) const
    { return getIsNull(row, column(col)); }
    bool getIsNull(size_t row, const std::string &col) const
    { return getIsNull(row, column(col)); }

    template <class T> T get(size_t row, size_t column) const;
    template <class T> T get(size_t row, const char* col) const
        { return get<T>(row, column(col)); }
    template <class T> T get(size_t row, const std::string & col) const
        { return get<T>(row, column(col)); }

private:
    boost::shared_ptr<PGresult> m_result;
};


}}

#endif
