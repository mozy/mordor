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
    Result() {}

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

    /// Get the value of a cell
    ///
    /// Supported overloads:
    ///  * std::string  (bytea, text, <any>)
    ///  * const char * (bytea, text, <any>)
    ///  * bool (boolean)
    ///  * char (char)
    ///  * long long (bigint)
    ///  * short (smallint)
    ///  * int (integer)
    ///  * float (real)
    ///  * double (double precision)
    ///  * boost::posix_time::ptime (timestamp)
    ///  * std::vector<int> (integer[])
    ///  * std::pair<IPAddress::ptr, unsigned int> (inet, cidr)
    ///  * IPAddress::ptr (inet, cidr)
    /// @note Except for the string overloads, these will all ASSERT if the
    ///       value is NULL.  Additionally, the integer[] will also ASSERT if
    ///       any of the values within the array are NULL.
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
