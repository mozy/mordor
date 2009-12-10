#ifndef __MORDOR_ANYMAP_H__
#define __MORDOR_ANYMAP_H__
// Copyright (c) 2009 - Decho Corp.

#include <map>
#include <typeinfo>

#include <boost/any.hpp>

namespace Mordor {

template <class Tag, class T>
class anymaptag
{
public:
    typedef Tag tag_type;
    typedef T value_type;
};

template <class T>
class anymapvalue
{
    friend class anymap;
private:
    anymapvalue(boost::any &any)
        : m_any(any)
    {}

public:
    anymapvalue &operator =(const T &v)
    {
        m_any = v;
        return *this;
    }

    operator T() const
    { return boost::any_cast<T>(m_any); }

    bool empty() const
    { return m_any.empty(); }

private:
    boost::any m_any;
};

class anymap
{
private:
    class anymaptypeinfo
    {
    public:
        anymaptypeinfo(const std::type_info &type)
            : m_type(type)
        {}
        anymaptypeinfo(const anymaptypeinfo &copy)
            : m_type(copy.m_type)
        {}
        bool operator <(const anymaptypeinfo& rhs) const
        {
            return !!m_type.before(rhs.m_type);
        }

    private:
        const std::type_info &m_type;
    };
public:
    template <class TagType>
    anymapvalue<typename TagType::value_type>
    operator[](const TagType &)
    {
        return anymapvalue<typename TagType::value_type>(m_map[typeid(typename TagType::tag_type)]);
    }

private:
    std::map<anymaptypeinfo, boost::any> m_map;
};

}

#endif
