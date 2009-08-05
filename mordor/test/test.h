#ifndef __TEST_H__
#define __TEST_H__
// Copyright (c) 2009 - Decho Corp.

#include <map>
#include <sstream>
#include <typeinfo>
#include <vector>

#include "mordor/common/assert.h"

class TestInstance;

typedef void (*TestDg)();
typedef std::pair<TestDg, std::map<std::string, TestDg> > TestSuite;
typedef std::map<std::string, TestSuite> TestSuites;


// Test definitions
#define TEST(TestName)                                                          \
    static void TestName();                                                     \
    static struct register_ ## TestName ## _struct {                            \
        register_ ## TestName ## _struct() {                                    \
            registerTest("", #TestName, & ## TestName );                        \
        }                                                                       \
} g_ ## TestName ## _registration;                                              \
    static void TestName()


#define SUITE_INVARIANT(TestSuite)                                              \
    static void _ ## TestSuite ## _invariant();                                 \
    static struct register__ ## TestSuite ## _invariant_struct {                \
        register__ ## TestSuite ## _invariant_struct() {                        \
            registerSuiteInvariant(#TestSuite,                                  \
                &_ ## TestSuite ## _invariant);                                 \
        }                                                                       \
} g__ ## TestSuite ## _invariant_registration;                                  \
    static void _ ## TestSuite ## _invariant()


#define TEST_WITH_SUITE(TestSuite, TestName)                                    \
    static void TestSuite ## _ ## TestName();                                   \
    static struct register_ ## TestSuite ## _ ## TestName ## _struct {          \
        register_ ## TestSuite ## _ ## TestName ## _struct() {                  \
            registerTest(#TestSuite, #TestName, & TestSuite ## _ ## TestName ); \
        }                                                                       \
} g_ ## TestSuite ## _ ## TestName ## _registration;                            \
    static void TestSuite ## _ ## TestName()


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
        const std::string &test, const std::string &message) = 0;
    virtual void testException(const std::string &suite,
        const std::string &test, const std::exception &ex) = 0;
    virtual void testUnknownException(const std::string &suite,
        const std::string &test) = 0;
    virtual void testsComplete() = 0;
};

// Internal functions
void registerTest(const std::string &suite, const std::string &testName,
                  TestDg test);
void registerSuiteInvariant(const std::string &suite, TestDg invariant);

// Public functions
const TestSuites &allTests();
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

#define NO_SERIALIZE_BARE(type)                                                 \
struct serializer<type > : public type_serializer<type >                        \
{                                                                               \
    serializer(const type &t) {}                                                \
};

#define NO_SERIALIZE(type)                                                      \
template <>                                                                     \
NO_SERIALIZE_BARE(type)

template <class T>
NO_SERIALIZE_BARE(std::vector<T>)

// Assertion macros
#define TEST_ASSERT(expr)                                                       \
    if (!(expr)) assertion(__FILE__, __LINE__, #expr)

#define TEST_ASSERT_EQUAL(lhs, rhs)                                             \
    assertEqual(__FILE__, __LINE__, lhs, rhs, #lhs, #rhs)

#define TEST_ASSERT_NOT_EQUAL(lhs, rhs)                                         \
    assertNotEqual(__FILE__, __LINE__, lhs, rhs, #lhs, #rhs)

#define TEST_ASSERT_LESS_THAN(lhs, rhs)                                         \
    assertLessThan(__FILE__, __LINE__, lhs, rhs, #lhs, #rhs)

#define TEST_ASSERT_LESS_THAN_OR_EQUAL(lhs, rhs)                                \
    assertLessThanOrEqual(__FILE__, __LINE__, lhs, rhs, #lhs, #rhs)

#define TEST_ASSERT_GREATER_THAN(lhs, rhs)                                      \
    assertGreaterThan(__FILE__, __LINE__, lhs, rhs, #lhs, #rhs)

#define TEST_ASSERT_GREATER_THAN_OR_EQUAL(lhs, rhs)                             \
    assertGreaterThanOrEqual(__FILE__, __LINE__, lhs, rhs, #lhs, #rhs)

#define TEST_ASSERT_ABOUT_EQUAL(lhs, rhs, variance)                             \
    assertAboutEqual(__FILE__, __LINE__, lhs, rhs, #lhs, #rhs, variance)

#define TEST_ASSERT_EXCEPTION(code, exception)                                  \
    try {                                                                       \
        code;                                                                   \
        assertion(__FILE__, __LINE__, "Expected " #exception " from " #code);   \
    } catch (exception) {                                                       \
    }

#define TEST_ASSERT_ASSERTED(code)                                              \
    {                                                                           \
        bool __selfAsserted = false;                                            \
        try {                                                                   \
            code;                                                               \
            __selfAsserted = true;                                              \
            assertion(__FILE__, __LINE__, "Expected Assertion from " #code);    \
        } catch (Assertion) {                                                   \
            if (__selfAsserted)                                                 \
                throw;                                                          \
        }                                                                       \
    }

// Assertion internal functions
void assertion(const char *file, int line, const std::string &expr);

template <class T, class U>
void assertComparison(const char *file, int line,
    T lhs, U rhs, const char *lhsExpr, const char *rhsExpr,
    const char *op)
{
    std::ostringstream os;
    serializer<T> t(lhs);
    serializer<U> u(rhs);
    os << lhsExpr << " " << op << " " << rhsExpr
        << "\n" << t << " " << op << " " << u;
    assertion(file, line, os.str());
}

template <class T, class U>
void assertEqual(const char *file, int line,
    T lhs, U rhs, const char *lhsExpr, const char *rhsExpr)
{
    if (!(lhs == rhs)) {
        assertComparison(file, line, lhs, rhs, lhsExpr, rhsExpr, "==");
    }
}

template <>
void assertEqual<const char *, const char *>(const char *file,
    int line, const char *lhs, const char *rhs, const char *lhsExpr,
    const char *rhsExpr);

template <class T, class U>
void assertNotEqual(const char *file, int line,
    T lhs, U rhs, const char *lhsExpr, const char *rhsExpr)
{
    if (!(lhs != rhs)) {
        assertComparison(file, line, lhs, rhs, lhsExpr, rhsExpr, "!=");
    }
}

template <>
void assertNotEqual<const char *, const char *>(const char *file,
    int line, const char *lhs, const char *rhs, const char *lhsExpr,
    const char *rhsExpr);

template <class T, class U>
void assertLessThan(const char *file, int line,
    T lhs, U rhs, const char *lhsExpr, const char *rhsExpr)
{
    if (!(lhs < rhs)) {
        assertComparison(file, line, lhs, rhs, lhsExpr, rhsExpr, "<");
    }
}

template <>
void assertLessThan<const char *, const char *>(const char *file,
    int line, const char *lhs, const char *rhs, const char *lhsExpr,
    const char *rhsExpr);

template <class T, class U>
void assertLessThanOrEqual(const char *file, int line,
    T lhs, U rhs, const char *lhsExpr, const char *rhsExpr)
{
    if (!(lhs <= rhs)) {
        assertComparison(file, line, lhs, rhs, lhsExpr, rhsExpr, "<=");
    }
}

template <>
void assertLessThanOrEqual<const char *, const char *>(const char *file,
    int line, const char *lhs, const char *rhs, const char *lhsExpr,
    const char *rhsExpr);

template <class T, class U>
void assertGreaterThan(const char *file, int line,
    T lhs, U rhs, const char *lhsExpr, const char *rhsExpr)
{
    if (!(lhs > rhs)) {
        assertComparison(file, line, lhs, rhs, lhsExpr, rhsExpr, ">");
    }
}

template <>
void assertGreaterThan<const char *, const char *>(const char *file,
    int line, const char *lhs, const char *rhs, const char *lhsExpr,
    const char *rhsExpr);

template <class T, class U>
void assertGreaterThanOrEqual(const char *file, int line,
    T lhs, U rhs, const char *lhsExpr, const char *rhsExpr)
{
    if (!(lhs >= rhs)) {
        assertComparison(file, line, lhs, rhs, lhsExpr, rhsExpr, ">=");
    }
}

template <>
void assertGreaterThanOrEqual<const char *, const char *>(const char *file,
    int line, const char *lhs, const char *rhs, const char *lhsExpr,
    const char *rhsExpr);

template <class T, class U, class V>
void assertAboutEqual(const char *file, int line,
    T lhs, U rhs, const char *lhsExpr, const char *rhsExpr, V variance)
{
    if (!(lhs - variance < rhs && lhs + variance > rhs)) {
        assertComparison(file, line, lhs, rhs, lhsExpr, rhsExpr, "~==");
    }
}

#endif
