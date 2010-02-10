#ifndef __MORDOR_ANYMAP_H__
#define __MORDOR_ANYMAP_H__
// Copyright (c) 2009 - Mozy, Inc.

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

class anymap
{
public:
    typedef boost::shared_ptr<anymap> ptr;

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
    typename TagType::value_type&
    operator[](const TagType &)
    {
        boost::any &val = m_map[typeid(typename TagType::tag_type)];
        if (val.empty()) {
            val = typename TagType::value_type();
        }
        return *boost::any_cast<typename TagType::value_type>(&val);
    }

    template <class TagType>
    size_t erase(const TagType &)
    {
        return m_map.erase(typeid(typename TagType::tag_type));
    }

    void clear()
    {
        m_map.clear();
    }

    size_t size() const
    {
        return m_map.size();
    }


    bool empty() const
    {
        return m_map.empty();
    }

private:
    std::map<anymaptypeinfo, boost::any> m_map;
};

}

#endif
