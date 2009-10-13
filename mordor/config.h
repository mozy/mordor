#ifndef __CONFIG_H__
#define __CONFIG_H__
// Copyright (c) 2009 - Decho Corp.

#include <set>
#include <sstream>
#include <string>
#include <vector>

#include <boost/function.hpp>
#include <boost/shared_ptr.hpp>

#include "assert.h"

namespace Mordor {

class ConfigVarBase
{
public:
    typedef boost::shared_ptr<ConfigVarBase> ptr;

    struct Comparator
    {
        bool operator() (const ConfigVarBase::ptr &lhs, const ConfigVarBase::ptr &rhs) const;
    };

public:
    ConfigVarBase(const std::string &name, const std::string &description = "",
        bool dynamic = true, bool automatic = false)
        : m_name(name),
          m_description(description),
          m_dynamic(dynamic),
          m_automatic(automatic)
    {}
    virtual ~ConfigVarBase() {}

    std::string name() const { return m_name; }
    std::string description() const { return m_description; }
    bool dynamic() const { return m_dynamic; }
    bool automatic() const { return m_automatic; }

    void monitor(boost::function<void ()> dg) { m_dgs.push_back(dg); }

    virtual std::string toString() const { return ""; };
    virtual void fromString(const std::string &str) {};

private:
    std::string m_name, m_description;
    bool m_dynamic, m_automatic;

protected:
    std::vector<boost::function<void ()> > m_dgs;
};

template <class T>
class ConfigVar : public ConfigVarBase
{
public:
    typedef boost::shared_ptr<ConfigVar> ptr;

public:
    ConfigVar(const std::string &name, const T &defaultValue,
        const std::string &description = "", bool dynamic = true,
        bool automatic = false)
        : ConfigVarBase(name, description, dynamic, automatic),
          m_val(defaultValue)
    {}

    std::string toString() const
    {
        std::ostringstream os;
        os << m_val;
        return os.str();
    }

    void fromString(const std::string &str)
    {
        std::istringstream is(str);
        T v;
        is >> v;
        val(v);
    }

    // TODO: atomicCompareExchange and/or mutex
    T val() const { return m_val; }
    void val(const T &v)
    {
        T oldVal = m_val;
        m_val = v;
        if (oldVal != v) {
            notify();
        }
    }

private:
    void notify() const
    {
        // TODO: lock and copy?
        for (std::vector<boost::function<void ()> >::const_iterator it(m_dgs.begin());
            it != m_dgs.end();
            ++it) {
            (*it)();
        }
    }

private:
    T m_val;
};

class Config
{
public:
    template <class T>
    static typename ConfigVar<T>::ptr lookup(const std::string &name, const T &defaultValue,
        const std::string &description = "", bool dynamic = true,
        bool automatic = false)
    {
        MORDOR_ASSERT(name.find_first_not_of("abcdefghijklmnopqrstuvwxyz.") == std::string::npos);

        typename ConfigVar<T>::ptr v(new ConfigVar<T>(name, defaultValue, description, dynamic, automatic));
        MORDOR_ASSERT(vars().find(v) == vars().end());
        vars().insert(v);
        return v;
    }

    static ConfigVarBase::ptr lookup(const std::string &name);

    static void loadFromEnvironment();

private:
    static std::set<ConfigVarBase::ptr, ConfigVarBase::Comparator> &vars()
    {
        static std::set<ConfigVarBase::ptr, ConfigVarBase::Comparator> vars;
        return vars;
    }
};

}

#endif
