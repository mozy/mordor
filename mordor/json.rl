// Copyright (c) 2009 - Mozy, Inc.

#include "mordor/pch.h"

#include "mordor/json.h"

#include "mordor/assert.h"
#include "mordor/string.h"

namespace Mordor {
namespace JSON {

template<>
void
Value::set<bool>(const bool& value,
                 boost::enable_if<boost::is_arithmetic<bool> >::type *)
{
    (ValueBase &)*this = value;
}

namespace {
class BoolVisitor : public boost::static_visitor<>
{
public:
    void operator()(const Array &array)
    {
        result = array.empty();
    }

    void operator()(const Object &object)
    {
        result = object.empty();
    }

    template <class T>
    void operator()(const T& value)
    {
        MORDOR_THROW_EXCEPTION(boost::bad_get());
    }

    bool result;
};

class SizeVisitor : public boost::static_visitor<>
{
public:
    void operator()(const Array &array)
    {
        result = array.size();
    }

    void operator()(const Object &object)
    {
        result = object.size();
    }

    template <class T>
    void operator()(const T& value)
    {
        MORDOR_THROW_EXCEPTION(boost::bad_get());
    }

    size_t result;
};
}

#if defined(GCC) && ((__GNUC__ > 4) || (__GNUC__ == 4 && __GNUC_MINOR__ > 6))
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wmaybe-uninitialized"
#endif

bool
Value::empty() const
{
    BoolVisitor visitor;
    boost::apply_visitor(visitor, *this);
    return visitor.result;
}

size_t
Value::size() const
{
    SizeVisitor visitor;
    boost::apply_visitor(visitor, *this);
    return visitor.result;
}

#if defined(GCC) && ((__GNUC__ > 4) || (__GNUC__ == 4 && __GNUC_MINOR__ > 6))
#pragma GCC diagnostic pop
#endif

static Value g_blank;

const Value &
Value::operator[](const std::string &key) const
{
    const Object &object = get<const Object>();
    const_iterator it = object.find(key);
    if (it == object.end())
        return g_blank;
    return it->second;
}

std::string unquote(const std::string &string)
{
    MORDOR_ASSERT(string.size() >= 2);
    MORDOR_ASSERT(string[0] == '"');
    MORDOR_ASSERT(string[string.size() - 1] == '"');
    std::string result = string.substr(1, string.size() - 2);

    const char *c = string.c_str() + 1;
    const char *end = c + string.size() - 2;
    bool differed = false;
    utf16char utf16, priorUtf16 = 0;
    while (c < end)
    {
        if (*c == '\\') {
            MORDOR_ASSERT(c + 1 < end);
            if (!differed) {
                result.resize(c - (string.c_str() + 1));
                differed = true;
            }
            ++c;
            switch (*c) {
                case '"':
                case '\\':
                case '/':
                    result.append(1, *c);
                    priorUtf16 = 0;
                    break;
                case 'b':
                    result.append(1, '\b');
                    priorUtf16 = 0;
                    break;
                case 'f':
                    result.append(1, '\f');
                    priorUtf16 = 0;
                    break;
                case 'n':
                    result.append(1, '\n');
                    priorUtf16 = 0;
                    break;
                case 'r':
                    result.append(1, '\r');
                    priorUtf16 = 0;
                    break;
                case 't':
                    result.append(1, '\t');
                    priorUtf16 = 0;
                    break;
                case 'u':
                    MORDOR_ASSERT(c + 4 < end);
                    utf16 = 0;
                    ++c;
                    for (int i = 0; i < 4; ++i) {
                        utf16 *= 16;
                        if (*c >= '0' && *c <= '9')
                            utf16 += *c - '0';
                        else if (*c >= 'a' && *c <= 'f')
                            utf16 += *c - 'a' + 10;
                        else if (*c >= 'A' && *c <= 'F')
                            utf16 += *c - 'A' + 10;
                        else
                            MORDOR_NOTREACHED();
                        ++c;
                    }
                    if (isHighSurrogate(priorUtf16) && isLowSurrogate(utf16)) {
                        // Back out the incorrect UTF8 we previously saw
                        result.resize(result.size() - 3);
                        result.append(toUtf8(priorUtf16, utf16));
                        priorUtf16 = 0;
                    } else {
                        result.append(toUtf8(utf16));
                        priorUtf16 = utf16;
                    }
                    continue;
                default:
                    MORDOR_NOTREACHED();
            }
        } else if (differed) {
            result.append(1, *c);
            priorUtf16 = 0;
        }
        ++c;
    }
    return result;
}

%%{
    machine json_parser;

    action mark { mark = fpc;}
    prepush {
        prepush();
    }
    postpop {
        postpop();
    }

    ws = ' ' | '\t' | '\r' | '\n';

    unescaped = (any - ('"' | '\\') - cntrl);
    char = unescaped | ('\\' ('"' | '\\' | '/' | 'b' | 'f' | 'n' | 'r' | 't' | ('u' [0-9A-Za-z]{4})));
    string = '"' char* '"';

    action begin_number
    {
        m_nonIntegral = false;
    }
    action set_non_integral
    {
        m_nonIntegral = true;
    }
    action parse_number
    {
        if (m_nonIntegral) {
            *m_stack.top() = strtod(mark, NULL);
        } else {
            *m_stack.top() = strtoll(mark, NULL, 10);
        }
    }

    int = (digit | ([1-9] digit*));
    frac = '.' >set_non_integral digit+;
    exp = 'e'i >set_non_integral ('+' | '-')? digit+;
    number = ('-'? int frac? exp?) >mark >begin_number %parse_number;

    action parse_string
    {
        *m_stack.top() = unquote(std::string(mark, fpc - mark));
    }
    action call_parse_object
    {
        *m_stack.top() = Object();
        fcall *json_parser_en_parse_object;
    }
    action call_parse_array
    {
        *m_stack.top() = Array();
        fcall *json_parser_en_parse_array;
    }
    action parse_true
    {
        *m_stack.top() = true;
    }
    action parse_false
    {
        *m_stack.top() = false;
    }
    action parse_null
    {
        *m_stack.top() = boost::blank();
    }
    value = string >mark %parse_string | number | '{' @call_parse_object |
        '[' @call_parse_array | 'true' @parse_true | 'false' @parse_false |
        'null' @parse_null;

    object = '{' ws* (string ws* ':' ws* value ws* (',' ws* string ws* ':' ws* value ws*)*)? '}';
    array = '[' ws* (value ws* (',' ws* value ws*)*)? ']';

    action new_key

    {
        m_stack.push(&boost::get<Object>(*m_stack.top())[unquote(std::string(mark, fpc - mark))]);
    }
    action new_element
    {
        boost::get<Array>(*m_stack.top()).push_back(Value());
        m_stack.push(&boost::get<Array>(*m_stack.top()).back());
    }
    action pop_stack
    {
        m_stack.pop();
    }
    action ret {
        fret;
    }

    parse_object := parse_object_lbl: ws* (string >mark %new_key ws* ':' ws* value %pop_stack ws* (',' ws* string >mark %new_key ws* ':' ws* value %pop_stack ws*)*)? '}' @ret;
    parse_array := parse_array_lbl: ws* (value >new_element %pop_stack ws* (',' ws* value >new_element %pop_stack ws*)*)? ']' @ret;

    main := ws* value ws*;
    write data;
}%%

void
Parser::init()
{
    while (m_stack.size() > 1)
        m_stack.pop();
    *m_stack.top() = boost::blank();
    RagelParserWithStack::init();
    %% write init;
}

void
Parser::exec()
{
#ifdef MSVC
#pragma warning(push)
#pragma warning(disable : 4244)
#endif
        %% write exec;
#ifdef MSVC
#pragma warning(pop)
#endif
}

bool
Parser::final() const
{
    return cs >= json_parser_first_final;
}

bool
Parser::error() const
{
    return cs == json_parser_error;
}

std::string
quote(const std::string &str)
{
    std::string result;
    result.reserve(str.length() + 2);
    result.append(1, '"');

    static const char *escaped =
        "\0\x01\x02\x03\x04\x05\x06\x07\x08\x09\x0a\x0b\x0c\x0d\x0e\x0f"
        "\x10\x11\x12\x13\x14\x15\x16\x17\x18\x19\x1a\x1b\x1c\x1d\x1e\x1f"
        "\\\"";

    size_t lastEscape = 0;
    size_t nextEscape = str.find_first_of(escaped, 0, 34);
    std::ostringstream os;
    os.fill('0');
    os << std::hex << std::setw(4);
    while (nextEscape != std::string::npos) {
        result.append(str.substr(lastEscape, nextEscape - lastEscape));
        result.append(1, '\\');
        switch (str[nextEscape]) {
            case '"':
            case '\\':
                result.append(1, str[nextEscape]);
                break;
            case '\b':
                result.append(1, 'b');
                break;
            case '\f':
                result.append(1, 'f');
                break;
            case '\n':
                result.append(1, 'n');
                break;
            case '\r':
                result.append(1, 'r');
                break;
            case '\t':
                result.append(1, 't');
                break;
            default:
                result.append(1, 'u');
                os.str();
                os << (int)str[nextEscape];
                result.append(os.str());
                break;
        }
        lastEscape = nextEscape + 1;
        nextEscape = str.find_first_of(escaped, lastEscape, 34);
    }
    result.append(str.substr(lastEscape));
    result.append(1, '"');
    return result;
}

namespace {
class JSONVisitor : public boost::static_visitor<>
{
public:
    JSONVisitor(std::ostream &os)
    : os(os),
      depth(0)
    {}

    void operator()(const boost::blank &b)
    {
        os << "null";
    }
    void operator()(const bool &b)
    {
        os << (b ? "true" : "false");
    }
    template <class T>
    void operator()(const T &t)
    {
        os << t;
    }
    void operator()(const std::string &str)
    {
        os << quote(str);
    }
    void operator()(const Object &object)
    {
        if (object.empty()) {
            os << "{ }";
        } else {
            ++depth;
            std::string prefix(depth * 4, ' ');
            os << "{\n";
            for (Object::const_iterator it(object.begin());
                it != object.end();
                ++it) {
                if (it != object.begin())
                    os << ",\n";
                os << prefix << quote(it->first) << " : ";
                boost::apply_visitor(*this, it->second);
            }
            --depth;
            prefix.clear();
            prefix.append(depth * 4, ' ');
            os << '\n' << prefix << "}";
        }
    }
    void operator()(const Array &array)
    {
        if (array.empty()) {
            os << "[ ]";
        } else {
            ++depth;
            std::string prefix(depth * 4, ' ');
            os << "[\n";
            for (Array::const_iterator it(array.begin());
                it != array.end();
                ++it) {
                if (it != array.begin())
                    os << ",\n";
                os << prefix;
                boost::apply_visitor(*this, *it);
            }
            --depth;
            prefix.clear();
            prefix.append(depth * 4, ' ');
            os << '\n' << prefix << "]";
        }
    }

    std::ostream &os;
    int depth;
};
}

std::ostream &operator <<(std::ostream &os, const Value &json)
{
    JSONVisitor visitor(os);
    boost::apply_visitor(visitor, json);
    return os;
}

bool isBlank(Value value)
{
    boost::blank *isItBlank = boost::get<boost::blank>(&value);
    return  !!isItBlank;
}

}}
