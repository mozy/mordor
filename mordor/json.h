#ifndef __MORDOR_JSON_H__
#define __MORDOR_JSON_H__

#include <stack>

#include <boost/variant.hpp>

#include "exception.h"
#include "ragel.h"

namespace Mordor {
namespace JSON {

class Object;
class Array;

typedef boost::variant<boost::blank, bool, long long, double, std::string, Array, Object> Value;

// Have to do it this way so we can forward declare the circular typedef
class Object : public std::multimap<std::string, Value>
{};
class Array : public std::vector<Value>
{};

class Parser : public RagelParserWithStack
{
public:
    Parser(Value &root)
    {
        m_stack.push(&root);
    }

    void init();
    bool complete() const { return false; }
    bool final() const;
    bool error() const;

protected:
    void exec();

private:
    std::stack<Value *> m_stack;
    bool m_nonIntegral;
};

std::string quote(const std::string &string);
std::string unquote(const std::string &string);

template <class T> Value load(T &t)
{
    Value result;
    Parser parser(result);
    parser.run(t);
    if (!parser.final() || parser.error())
        MORDOR_THROW_EXCEPTION(std::invalid_argument("Invalid JSON"));
    return result;
}

std::ostream &operator <<(std::ostream &os, const Value &json);

}}

#endif
