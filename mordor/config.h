#ifndef __CONFIG_H__
#define __CONFIG_H__
// Copyright (c) 2009 - Decho Corporation

#include "predef.h"

#include <set>
#include <sstream>
#include <string>
#include <vector>

#include <boost/function.hpp>
#include <boost/lexical_cast.hpp>
#include <boost/multi_index_container.hpp>
#include <boost/multi_index/ordered_index.hpp>
#include <boost/multi_index/global_fun.hpp>
#include <boost/noncopyable.hpp>
#include <boost/shared_ptr.hpp>
#include <boost/signals2/signal.hpp>

#include "assert.h"
#include "json.h"

namespace Mordor {

#ifdef WINDOWS
class IOManager;
#endif

class ConfigVarBase : public boost::noncopyable
{
public:
    typedef boost::shared_ptr<ConfigVarBase> ptr;

public:
    ConfigVarBase(const std::string &name, const std::string &description = "")
        : m_name(name),
          m_description(description)
    {}
    virtual ~ConfigVarBase() {}

    std::string name() const { return m_name; }
    std::string description() const { return m_description; }

    /// onChange should not throw any exceptions
    boost::signals2::signal<void ()> onChange;
    /// @deprecated (use onChange directly)
    void monitor(boost::function<void ()> dg) { onChange.connect(dg); }

    virtual std::string toString() const = 0;
    /// @return If the new value was accepted
    virtual bool fromString(const std::string &str) = 0;

private:
    std::string m_name, m_description;
};

template <class T>
class ConfigVar : public ConfigVarBase
{
public:
    struct BreakOnFailureCombiner
    {
        typedef bool result_type;
        template <typename InputIterator>
        bool operator()(InputIterator first, InputIterator last) const
        {
            try {
                for (; first != last; ++first)
                    if (!*first) return false;
            } catch (...) {
                return false;
            }
            return true;
        }
    };

    typedef boost::shared_ptr<ConfigVar> ptr;
    typedef boost::signals2::signal<bool (const T&), BreakOnFailureCombiner> before_change_signal_type;
    typedef boost::signals2::signal<void (const T&)> on_change_signal_type;

public:
    ConfigVar(const std::string &name, const T &defaultValue,
        const std::string &description = "")
        : ConfigVarBase(name, description),
          m_val(defaultValue)
    {}

    std::string toString() const
    {
        return boost::lexical_cast<std::string>(m_val);
    }

    bool fromString(const std::string &str)
    {
        try {
            return val(boost::lexical_cast<T>(str));
        } catch (boost::bad_lexical_cast &) {
            return false;
        }
    }

    /// beforeChange gives the opportunity to reject the new value;
    /// return false or throw an exception to prevent the change
    before_change_signal_type beforeChange;
    /// onChange should not throw any exceptions
    on_change_signal_type onChange;

    // TODO: atomicCompareExchange and/or mutex
    T val() const { return m_val; }
    bool val(const T &v)
    {
        T oldVal = m_val;
        if (oldVal != v) {
            if (!beforeChange(v))
                return false;
            m_val = v;
            onChange(v);
            ConfigVarBase::onChange();
        }
        return true;
    }

private:
    T m_val;
};

class Config
{
private:
    static std::string getName(const ConfigVarBase::ptr &var)
    {
        return var->name();
    }
    typedef boost::multi_index_container<
        ConfigVarBase::ptr,
        boost::multi_index::indexed_by<
            boost::multi_index::ordered_unique<
            boost::multi_index::global_fun<const ConfigVarBase::ptr &,
                std::string, &getName> >
        >
    > ConfigVarSet;

public:
#ifdef WINDOWS
    /// Encapsulates monitoring the registry for ConfigVar changes
    struct RegistryMonitor
    {
        friend class Config;
    public:
        typedef boost::shared_ptr<RegistryMonitor> ptr;
    private:
        RegistryMonitor(IOManager &iomanager, HKEY hKey,
            const std::wstring &subKey);

    public:
        RegistryMonitor(const RegistryMonitor &copy);
        ~RegistryMonitor();

    private:
        static void onRegistryChange(boost::weak_ptr<RegistryMonitor> self);

    private:
        IOManager &m_ioManager;
        HKEY m_hKey;
        HANDLE m_hEvent;
    };
#endif

public:
    template <class T>
    static typename ConfigVar<T>::ptr lookup(const std::string &name,
        const T &defaultValue, const std::string &description = "")
    {
        MORDOR_ASSERT(name.find_first_not_of("abcdefghijklmnopqrstuvwxyz.")
            == std::string::npos);

        MORDOR_ASSERT(vars().find(name) == vars().end());
        typename ConfigVar<T>::ptr v(new ConfigVar<T>(name, defaultValue,
            description));
        vars().insert(v);
        return v;
    }

    static ConfigVarBase::ptr lookup(const std::string &name);
    static void visit(boost::function<void (ConfigVarBase::ptr)> dg);

    static void loadFromEnvironment();
    static void loadFromJSON(const JSON::Value &json);
#ifdef WINDOWS
    static void loadFromRegistry(HKEY key, const std::string &subKey);
    static void loadFromRegistry(HKEY key, const std::wstring &subKey);
    /// @see RegistryMonitor
    static RegistryMonitor::ptr monitorRegistry(IOManager &ioManager, HKEY key,
        const std::string &subKey);
    /// @see RegistryMonitor
    static RegistryMonitor::ptr monitorRegistry(IOManager &ioManager, HKEY key,
        const std::wstring &subKey);
#endif

private:
    static ConfigVarSet &vars()
    {
        static ConfigVarSet vars;
        return vars;
    }
};

}

#endif
