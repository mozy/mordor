#ifndef __MORDOR_JSON_H__
#define __MORDOR_JSON_H__

#include <stack>
#include <boost/type_traits.hpp>
#include <boost/utility/enable_if.hpp>
#include <boost/variant.hpp>

#include "exception.h"
#include "ragel.h"

namespace Mordor {
namespace JSON {

class Value;

typedef std::map<std::string, Value> Object;
typedef std::vector<Value> Array;
typedef boost::variant<boost::blank, bool, long long, double, std::string, Array, Object> ValueBase;

class Value : public ValueBase
{
public:
    typedef Object::iterator iterator;
    typedef Object::const_iterator const_iterator;

public:
    Value() {}
    Value(const Value &copy) : ValueBase((const ValueBase &)copy) {}
    template <class T>
    Value(const T &value)
    {
        set<T>(value);
    }

    Value &operator=(const Value &rhs)
    {
        (ValueBase &)*this = (const ValueBase &)rhs;
        return *this;
    }
    template <class T>
    Value &operator=(const T &rhs)
    {
        set<T>(rhs);
        return *this;
    }

    // All of these accessors will throw boost::bad_get if used on an
    // inappropriately typed value
    template <class T>
    T &get()
    {
        return boost::get<T>(*this);
    }

    template <class T>
    const T &get() const
    {
        return boost::get<const T>(*this);
    }

    bool isBlank() const
    {
        return !!boost::get<const boost::blank>(this);
    }

    // Applies to Array or Object
    bool empty() const;
    size_t size() const;

    /// @return the indexth item in the Array
    Value &operator[](size_t index) { return get<Array>()[index]; }
    const Value &operator[](size_t index) const { return get<Array>()[index]; }
    /// @return the item in the Object with the specified key, or a
    /// boost::blank if it was not found (use find to disambiguate a real
    /// boost::blank)
    /// @note There is not a non-const version, since it can't return a
    /// reference to an object that doesn't exist
    const Value &operator[](const std::string &key) const;

    iterator begin() { return get<Object>().begin(); }
    const_iterator begin() const { return get<Object>().begin(); }
    iterator end() { return get<Object>().end(); }
    const_iterator end() const { return get<Object>().end(); }
    iterator find(const std::string &key) { return get<Object>().find(key); }
    const_iterator find(const std::string &key) const
    { return get<Object>().find(key); }

private:
    template <typename T>
    void set(const T& value,
             typename boost::enable_if<boost::is_integral<T> >::type* dummy = 0)
    {
        (ValueBase &)*this = (long long)value;
    }

    template <typename T>
    void set(const T& value,
             typename boost::enable_if<boost::is_floating_point<T> >::type* dummy = 0)
    {
        (ValueBase &)*this = (double)value;
    }

    template<typename T>
    void set(const T& value,
             typename boost::disable_if<boost::is_arithmetic<T> >::type* dummy = 0)
    {
        (ValueBase &)*this = value;
    }
};

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
/// Parse a string or Stream into a JSON::Value
///
/// Internally, this is overloaded for anything that RagelParser::run accepts
/// so you can pass a const char *, std::string, Stream &, or Stream::ptr
/// @throws std::invalid_argument on malformed JSON
template <class T> Value parse(T &t)
{ return load(t); }

std::ostream &operator <<(std::ostream &os, const Value &json);

}}

#endif
