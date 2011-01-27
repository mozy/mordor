// Copyright (c) 2010 - Mozy, Inc.

#include "result.h"

#include <boost/date_time/posix_time/posix_time_types.hpp>

#include "mordor/assert.h"
#include "mordor/endian.h"

#define BOOLOID 16
#define CHAROID 18
#define INT8OID 20
#define INT2OID 21
#define INT4OID 23
#define FLOAT4OID 700
#define FLOAT8OID 701
#define TIMESTAMPOID 1114
#define TIMESTAMPTZOID 1184

namespace Mordor {
namespace PQ {

size_t
Result::rows() const
{
    return (size_t)PQntuples(m_result.get());
}

size_t
Result::columns() const
{
    return (size_t)PQnfields(m_result.get());
}

size_t
Result::column(const char *name) const
{
    return (size_t)PQfnumber(m_result.get(), name);
}

Oid
Result::getType(size_t column) const
{
    return PQftype(m_result.get(), (int)column);
}

bool Result::getIsNull(size_t row, size_t column) const {
    return PQgetisnull(m_result.get(), (int)row, (int)column) == 1;
}

template <>
std::string
Result::get<std::string>(size_t row, size_t column) const
{
    return std::string(PQgetvalue(m_result.get(), (int)row, (int)column),
        PQgetlength(m_result.get(), (int)row, (int)column));
}

template <>
const char *
Result::get<const char *>(size_t row, size_t column) const
{
    return PQgetvalue(m_result.get(), (int)row, (int)column);
}

template <>
bool
Result::get<bool>(size_t row, size_t column) const
{
    MORDOR_ASSERT(getType(column) == BOOLOID);
    MORDOR_ASSERT(PQgetlength(m_result.get(), (int)row, (int)column) == 1);
    return !!*PQgetvalue(m_result.get(), (int)row, (int)column);
}

template <>
char
Result::get<char>(size_t row, size_t column) const
{
    MORDOR_ASSERT(getType(column) == CHAROID);
    MORDOR_ASSERT(PQgetlength(m_result.get(), (int)row, (int)column) == 1);
    return *PQgetvalue(m_result.get(), (int)row, (int)column);
}

template <>
long long
Result::get<long long>(size_t row, size_t column) const
{
    switch (getType(column)) {
        case INT8OID:
            MORDOR_ASSERT(PQgetlength(m_result.get(), (int)row, (int)column) == 8);
            return byteswapOnLittleEndian(*(long long *)PQgetvalue(m_result.get(), (int)row, (int)column));
        case INT2OID:
            MORDOR_ASSERT(PQgetlength(m_result.get(), (int)row, (int)column) == 2);
            return byteswapOnLittleEndian(*(short *)PQgetvalue(m_result.get(), (int)row, (int)column));
        case INT4OID:
            MORDOR_ASSERT(PQgetlength(m_result.get(), (int)row, (int)column) == 4);
            return byteswapOnLittleEndian(*(int *)PQgetvalue(m_result.get(), (int)row, (int)column));
        default:
            MORDOR_NOTREACHED();
    }
}

template <>
short
Result::get<short>(size_t row, size_t column) const
{
    MORDOR_ASSERT(getType(column) == INT2OID);
    MORDOR_ASSERT(PQgetlength(m_result.get(), (int)row, (int)column) == 2);
    return byteswapOnLittleEndian(*(short *)PQgetvalue(m_result.get(), (int)row, (int)column));
}

template <>
int
Result::get<int>(size_t row, size_t column) const
{
    switch (getType(column)) {
        case INT2OID:
            MORDOR_ASSERT(PQgetlength(m_result.get(), (int)row, (int)column) == 2);
            return byteswapOnLittleEndian(*(short *)PQgetvalue(m_result.get(), (int)row, (int)column));
        case INT4OID:
            MORDOR_ASSERT(PQgetlength(m_result.get(), (int)row, (int)column) == 4);
            return byteswapOnLittleEndian(*(int *)PQgetvalue(m_result.get(), (int)row, (int)column));
        default:
            MORDOR_NOTREACHED();
    }
}

template <>
float
Result::get<float>(size_t row, size_t column) const
{
    MORDOR_ASSERT(getType(column) == FLOAT4OID);
    MORDOR_ASSERT(PQgetlength(m_result.get(), (int)row, (int)column) == 4);
    int temp = byteswapOnLittleEndian(*(int *)PQgetvalue(m_result.get(), (int)row, (int)column));
    return *(float *)&temp;
}

template <>
double
Result::get<double>(size_t row, size_t column) const
{
    int templ;
    long long templl;
    switch (getType(column)) {
        case FLOAT4OID:
            MORDOR_ASSERT(PQgetlength(m_result.get(), (int)row, (int)column) == 4);
            templ = byteswapOnLittleEndian(*(int *)PQgetvalue(m_result.get(), (int)row, (int)column));
            return *(float *)&templ;
        case FLOAT8OID:
            MORDOR_ASSERT(PQgetlength(m_result.get(), (int)row, (int)column) == 8);
            templl = byteswapOnLittleEndian(*(long long *)PQgetvalue(m_result.get(), (int)row, (int)column));
            return *(double *)&templl;
        default:
            MORDOR_NOTREACHED();
    }
}

static const boost::posix_time::ptime postgres_epoch(boost::gregorian::date(2000, 1, 1));

template<>
boost::posix_time::ptime
Result::get<boost::posix_time::ptime>(size_t row, size_t column) const
{
    MORDOR_ASSERT(getType(column) == TIMESTAMPOID ||
        getType(column) == TIMESTAMPTZOID);
    if (PQgetlength(m_result.get(), (int)row, (int)column) == 0)
        return boost::posix_time::ptime();
    MORDOR_ASSERT(PQgetlength(m_result.get(), (int)row, (int)column) == 8);
    long long microseconds = byteswapOnLittleEndian(*(long long *)PQgetvalue(m_result.get(), (int)row, (int)column));
    return postgres_epoch +
        boost::posix_time::seconds((long)(microseconds / 1000000)) +
        boost::posix_time::microseconds(microseconds % 1000000);
}

}}
