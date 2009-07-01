#ifndef __TEST_H__
#define __TEST_H__
// Copyright (c) 2009 - Decho Corp.

#include <map>
#include <sstream>
#include <typeinfo>
#include <vector>

class TestInstance;

typedef void (*TestDg)();
typedef std::map<std::string, TestDg> TestSuite;
typedef std::map<std::string, TestSuite> AllTests;

#define TEST(TestName)                                                          \
    static void TestName();                                                     \
    static struct register_ ## TestName ## _struct {                            \
        register_ ## TestName ## _struct() {                                    \
            registerTest("", #TestName, & ## TestName );                        \
        }                                                                       \
} g_ ## TestName ## _registration;                                              \
    static void TestName()


#define TEST_WITH_SUITE(TestSuite, TestName)                                    \
    static void TestSuite ## _ ## TestName();                                   \
    static struct register_ ## TestSuite ## _ ## TestName ## _struct {          \
        register_ ## TestSuite ## _ ## TestName ## _struct() {                  \
            registerTest(#TestSuite, #TestName, & TestSuite ## _ ## TestName ); \
        }                                                                       \
} g_ ## TestSuite ## _ ## TestName ## _registration;                            \
    static void TestSuite ## _ ## TestName()

#define TEST_ASSERT(expr)                                                       \
    if (!(expr)) TestInstance::assertion(__FILE__, __LINE__, #expr)

#define TEST_ASSERT_EQUAL(lhs, rhs)                                             \
    TestInstance::assertEqual(__FILE__, __LINE__, lhs, rhs, #lhs, #rhs)

void registerTest(const std::string &suite, const std::string &testName,
                  TestDg test);
void runTests();

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

class TestInstance
{
public:
    static void assertion(const char *file, int line, const std::string &expr);
    template <class T, class U>
    static void assertEqual(const char *file, int line,
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

    void run(TestDg test);

    std::string m_suite;
    std::string m_test;
};

#endif
