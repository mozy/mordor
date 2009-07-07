#ifndef __TEST_H__
#define __TEST_H__
// Copyright (c) 2009 - Decho Corp.

#include <map>
#include <sstream>
#include <typeinfo>
#include <vector>

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

#define TEST_ASSERT_EXCEPTION(code, exception)                                  \
    try {                                                                       \
        code;                                                                   \
        assertion(__FILE__, __LINE__, "Expected " #exception " from " #code);   \
    } catch (exception) {                                                       \
    }

// Assertion internal functions
void assertion(const char *file, int line, const std::string &expr);

template <class T, class U>
void assertEqual(const char *file, int line,
    T lhs, U rhs, const char *lhsExpr, const char *rhsExpr)
{
    if (!(lhs == rhs)) {
        std::ostringstream os;
        serializer<T> t(lhs);
        serializer<U> u(rhs);
        os << lhsExpr << " == " << rhsExpr << "\n" << t << " == " << u;
        assertion(file, line, os.str());
    }
}


#endif
