#ifndef __TEST_H__
#define __TEST_H__
// Copyright (c) 2009 - Decho Corporation

#include <map>
#include <sstream>
#include <typeinfo>
#include <vector>

#include "mordor/assert.h"

namespace Mordor {
namespace Test {

class TestInstance;

typedef void (*TestDg)();
typedef std::pair<TestDg, std::map<std::string, TestDg> > TestSuite;
typedef std::map<std::string, TestSuite> TestSuites;

/// Create an invariant that is run before and after every test in TestSuite
#define MORDOR_SUITE_INVARIANT(TestSuite)                                       \
    static void _ ## TestSuite ## _invariant();                                 \
    namespace {                                                                 \
    static struct register__ ## TestSuite ## _invariant_struct {                \
        register__ ## TestSuite ## _invariant_struct() {                        \
            ::Mordor::Test::registerSuiteInvariant(#TestSuite,                  \
                &_ ## TestSuite ## _invariant);                                 \
        }                                                                       \
} g__ ## TestSuite ## _invariant_registration;                                  \
    }                                                                           \
    static void _ ## TestSuite ## _invariant()

/// Create a unit test that is part of TestSuite
#define MORDOR_UNITTEST(TestSuite, TestName)                                    \
    static void TestSuite ## _ ## TestName();                                   \
    namespace {                                                                 \
    static struct register_ ## TestSuite ## _ ## TestName ## _struct {          \
        register_ ## TestSuite ## _ ## TestName ## _struct() {                  \
            ::Mordor::Test::registerTest(#TestSuite, #TestName,                 \
                & TestSuite ## _ ## TestName );                                 \
        }                                                                       \
} g_ ## TestSuite ## _ ## TestName ## _registration;                            \
    }                                                                           \
    static void TestSuite ## _ ## TestName()

/// Create a unit test that is part of TestSuite, and runs as a member function
/// of Fixture
#define MORDOR_UNITTEST_FIXTURE(Fixture, TestSuite, TestName)                   \
    class TestSuite ## _ ## TestName ## Helper : public Fixture                 \
    {                                                                           \
    public:                                                                     \
        void run();                                                             \
    };                                                                          \
    MORDOR_UNITTEST(TestSuite, TestName)                                        \
    {                                                                           \
        TestSuite ## _ ## TestName ## Helper helper;                            \
        helper.run();                                                           \
    }                                                                           \
    void TestSuite ## _ ## TestName ## Helper::run()

// Public interface
class TestListener
{
public:
    virtual ~TestListener() {}

    virtual void testStarted(const std::string &suite,
        const std::string &test) = 0;
    virtual void testComplete(const std::string &suite,
        const std::string &test) = 0;
    virtual void testAsserted(const std::string &suite,
        const std::string &test, const Assertion &assertion) = 0;
    virtual void testException(const std::string &suite,
        const std::string &test) = 0;
    virtual void testsComplete() = 0;
};

// Internal functions
void registerTest(const std::string &suite, const std::string &testName,
                  TestDg test);
void registerSuiteInvariant(const std::string &suite, TestDg invariant);

// Public functions
TestSuites &allTests();
TestSuites testsForArguments(int argc, char **argv);
bool runTests();
bool runTests(const TestSuites &suites);
bool runTests(TestListener &listener);
bool runTests(const TestSuites &suites,
              TestListener &listener);

// Serialization for assertion reporting
template <class T>
struct serializer
{
    serializer(const T &t) : m_t(t) {}

    std::ostream &serialize(std::ostream &os)
    {
        return os << m_t;
    }

    const T& m_t;
};

template <class T>
std::ostream &operator <<(std::ostream &os, serializer<T> t)
{
    return t.serialize(os);
}

template <class T>
struct type_serializer
{
    std::ostream &serialize(std::ostream &os)
    {
        return os << typeid(T).name();
    }
};

#define MORDOR_NO_SERIALIZE_BARE(type)                                          \
struct serializer<type> : public ::Mordor::Test::type_serializer<type>          \
{                                                                               \
    serializer(const type &t) {}                                                \
};

#define MORDOR_NO_SERIALIZE(type)                                               \
template <>                                                                     \
MORDOR_NO_SERIALIZE_BARE(type)

template <class T>
MORDOR_NO_SERIALIZE_BARE(std::vector<T>)

// Assertion macros
#define MORDOR_TEST_ASSERT(expr)                                                \
    if (!(expr)) ::Mordor::Test::assertion(__FILE__, __LINE__,                  \
        BOOST_CURRENT_FUNCTION, #expr)

#define MORDOR_TEST_ASSERT_EQUAL(lhs, rhs)                                      \
    ::Mordor::Test::assertEqual(__FILE__, __LINE__, BOOST_CURRENT_FUNCTION,     \
        lhs, rhs, #lhs, #rhs)

#define MORDOR_TEST_ASSERT_NOT_EQUAL(lhs, rhs)                                  \
    ::Mordor::Test::assertNotEqual(__FILE__, __LINE__, BOOST_CURRENT_FUNCTION,  \
        lhs, rhs, #lhs, #rhs)

#define MORDOR_TEST_ASSERT_LESS_THAN(lhs, rhs)                                  \
    ::Mordor::Test::assertLessThan(__FILE__, __LINE__, BOOST_CURRENT_FUNCTION,  \
        lhs, rhs, #lhs, #rhs)

#define MORDOR_TEST_ASSERT_LESS_THAN_OR_EQUAL(lhs, rhs)                         \
    ::Mordor::Test::assertLessThanOrEqual(__FILE__, __LINE__,                   \
        BOOST_CURRENT_FUNCTION, lhs, rhs, #lhs, #rhs)

#define MORDOR_TEST_ASSERT_GREATER_THAN(lhs, rhs)                               \
    ::Mordor::Test::assertGreaterThan(__FILE__, __LINE__,                       \
        BOOST_CURRENT_FUNCTION, lhs, rhs, #lhs, #rhs)

#define MORDOR_TEST_ASSERT_GREATER_THAN_OR_EQUAL(lhs, rhs)                      \
    ::Mordor::Test::assertGreaterThanOrEqual(__FILE__, __LINE__,                \
        BOOST_CURRENT_FUNCTION, lhs, rhs, #lhs, #rhs)

#define MORDOR_TEST_ASSERT_ABOUT_EQUAL(lhs, rhs, variance)                      \
    ::Mordor::Test::assertAboutEqual(__FILE__, __LINE__, BOOST_CURRENT_FUNCTION,\
        lhs, rhs, #lhs, #rhs, variance)

#define MORDOR_TEST_ASSERT_EXCEPTION(code, exception)                           \
    try {                                                                       \
        code;                                                                   \
        ::Mordor::Test::assertion(__FILE__, __LINE__, BOOST_CURRENT_FUNCTION,   \
            "Expected " + std::string(typeid(exception).name()) +               \
            " from " #code);                                                    \
    } catch (exception &) {                                                     \
    }

#define MORDOR_TEST_ASSERT_ANY_EXCEPTION(code)                                  \
    try {                                                                       \
        code;                                                                   \
        ::Mordor::Test::assertion(__FILE__, __LINE__, BOOST_CURRENT_FUNCTION,   \
            "Expected an exception from " #code);                               \
    } catch (...) {                                                             \
    }

#define MORDOR_TEST_ASSERT_ASSERTED(code)                                       \
    {                                                                           \
        bool __selfAsserted = false;                                            \
        try {                                                                   \
            code;                                                               \
            __selfAsserted = true;                                              \
            ::Mordor::Test::assertion(__FILE__, __LINE__,                       \
                BOOST_CURRENT_FUNCTION, "Expected Assertion from " #code);      \
    } catch (::Mordor::Assertion &) {                                           \
            if (__selfAsserted)                                                 \
                throw;                                                          \
        }                                                                       \
    }

/// Asserts on destruction if it was alive for longer than us microseconds
struct TimeConstraint
{
    TimeConstraint(unsigned long long us);
    ~TimeConstraint();

private:
    unsigned long long m_us, m_start;
};

// Assertion internal functions
void assertion(const char *file, int line, const char *function,
               const std::string &expr);

template <class T, class U>
void assertComparison(const char *file, int line, const char *function,
    T lhs, U rhs, const char *lhsExpr, const char *rhsExpr,
    const char *op)
{
    std::ostringstream os;
    serializer<T> t(lhs);
    serializer<U> u(rhs);
    os << lhsExpr << " " << op << " " << rhsExpr
        << "\n" << t << " " << op << " " << u;
    assertion(file, line, function, os.str());
}

template <class T, class U>
void assertEqual(const char *file, int line, const char *function,
    T lhs, U rhs, const char *lhsExpr, const char *rhsExpr)
{
    if (!(lhs == rhs)) {
        assertComparison(file, line, function, lhs, rhs, lhsExpr, rhsExpr,
            "==");
    }
}

template <>
void assertEqual<const char *, const char *>(const char *file,
    int line,  const char *function, const char *lhs, const char *rhs,
    const char *lhsExpr, const char *rhsExpr);
#ifdef WINDOWS
template <>
void assertEqual<const wchar_t *, const wchar_t *>(const char *file,
    int line,  const char *function, const wchar_t *lhs, const wchar_t *rhs,
    const char *lhsExpr, const char *rhsExpr);
#endif

template <class T, class U>
void assertNotEqual(const char *file, int line, const char *function,
    T lhs, U rhs, const char *lhsExpr, const char *rhsExpr)
{
    if (!(lhs != rhs)) {
        assertComparison(file, line, function, lhs, rhs, lhsExpr, rhsExpr,
            "!=");
    }
}

template <>
void assertNotEqual<const char *, const char *>(const char *file,
    int line, const char *function, const char *lhs, const char *rhs,
    const char *lhsExpr, const char *rhsExpr);
#ifdef WINDOWS
template <>
void assertNotEqual<const wchar_t *, const wchar_t *>(const char *file,
    int line, const char *function, const wchar_t *lhs, const wchar_t *rhs,
    const char *lhsExpr, const char *rhsExpr);
#endif

template <class T, class U>
void assertLessThan(const char *file, int line, const char *function,
    T lhs, U rhs, const char *lhsExpr, const char *rhsExpr)
{
    if (!(lhs < rhs)) {
        assertComparison(file, line, function, lhs, rhs, lhsExpr, rhsExpr,
            "<");
    }
}

template <>
void assertLessThan<const char *, const char *>(const char *file,
    int line, const char *function, const char *lhs, const char *rhs,
    const char *lhsExpr, const char *rhsExpr);
#ifdef WINDOWS
template <>
void assertLessThan<const wchar_t *, const wchar_t *>(const char *file,
    int line, const char *function, const wchar_t *lhs, const wchar_t *rhs,
    const char *lhsExpr, const char *rhsExpr);
#endif

template <class T, class U>
void assertLessThanOrEqual(const char *file, int line, const char *function,
    T lhs, U rhs, const char *lhsExpr, const char *rhsExpr)
{
    if (!(lhs <= rhs)) {
        assertComparison(file, line, function, lhs, rhs, lhsExpr, rhsExpr,
            "<=");
    }
}

template <>
void assertLessThanOrEqual<const char *, const char *>(const char *file,
    int line, const char *function, const char *lhs, const char *rhs,
    const char *lhsExpr, const char *rhsExpr);
#ifdef WINDOWS
template <>
void assertLessThanOrEqual<const wchar_t *, const wchar_t *>(const char *file,
    int line, const char *function, const wchar_t *lhs, const wchar_t *rhs,
    const char *lhsExpr, const char *rhsExpr);
#endif

template <class T, class U>
void assertGreaterThan(const char *file, int line, const char *function,
    T lhs, U rhs, const char *lhsExpr, const char *rhsExpr)
{
    if (!(lhs > rhs)) {
        assertComparison(file, line, function, lhs, rhs, lhsExpr, rhsExpr,
            ">");
    }
}

template <>
void assertGreaterThan<const char *, const char *>(const char *file,
    int line, const char *function, const char *lhs, const char *rhs,
    const char *lhsExpr, const char *rhsExpr);
#ifdef WINDOWS
template <>
void assertGreaterThan<const wchar_t *, const wchar_t *>(const char *file,
    int line, const char *function, const wchar_t *lhs, const wchar_t *rhs,
    const char *lhsExpr, const char *rhsExpr);
#endif

template <class T, class U>
void assertGreaterThanOrEqual(const char *file, int line, const char *function,
    T lhs, U rhs, const char *lhsExpr, const char *rhsExpr)
{
    if (!(lhs >= rhs)) {
        assertComparison(file, line, function, lhs, rhs, lhsExpr, rhsExpr,
            ">=");
    }
}

template <>
void assertGreaterThanOrEqual<const char *, const char *>(const char *file,
    int line, const char *function, const char *lhs, const char *rhs,
    const char *lhsExpr, const char *rhsExpr);
#ifdef WINDOWS
template <>
void assertGreaterThanOrEqual<const wchar_t *, const wchar_t *>(const char *file,
    int line, const char *function, const wchar_t *lhs, const wchar_t *rhs,
    const char *lhsExpr, const char *rhsExpr);
#endif

template <class T, class U, class V>
void assertAboutEqual(const char *file, int line, const char *function,
    T lhs, U rhs, const char *lhsExpr, const char *rhsExpr, V variance)
{
    if (!(lhs - variance < rhs && lhs + variance > rhs)) {
        assertComparison(file, line, function, lhs, rhs, lhsExpr, rhsExpr,
            "~==");
    }
}

}}

#endif
