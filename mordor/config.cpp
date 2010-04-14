// Copyright (c) 2009 - Mozy, Inc.

#include "mordor/pch.h"

#include "config.h"

#include <algorithm>

#include "json.h"
#include "string.h"
#include "util.h"

#ifndef WINDOWS
#ifndef OSX
extern char **environ;
#endif
#endif

#ifdef OSX
#include <crt_externs.h>
#endif

namespace Mordor {

void
Config::loadFromEnvironment()
{
#ifdef WINDOWS
    wchar_t *enviro = GetEnvironmentStringsW();
    if (!enviro)
        return;
    boost::shared_ptr<wchar_t> environScope(enviro, &FreeEnvironmentStringsW);
    for (const wchar_t *env = enviro; *env; env += wcslen(env) + 1) {
        const wchar_t *equals = wcschr(env, '=');
        if (!equals)
            continue;
        if (equals == env)
            continue;
        std::string key(toUtf8(env, equals - env));
        std::string value(toUtf8(equals + 1));
#else
#ifdef OSX
	char **environ = *_NSGetEnviron();
#endif
    if (!environ)
        return;
    for (const char *env = *environ; *env; env += strlen(env) + 1) {
        const char *equals = strchr(env, '=');
        if (!equals)
            continue;
        if (equals == env)
            continue;
        std::string key(env, equals - env);
        std::string value(equals + 1);
#endif
        std::transform(key.begin(), key.end(), key.begin(), tolower);
        replace(key, '_', '.');
        if (key.find_first_not_of("abcdefghijklmnopqrstuvwxyz.") != std::string::npos)
            continue;
        ConfigVarBase::ptr var = lookup(key);
        if (var)
            var->fromString(value);
    }
}

namespace {
class JSONVisitor : public boost::static_visitor<>
{
public:
    void operator()(const JSON::Object &object)
    {
        std::string prefix;
        if (!m_current.empty())
            prefix = m_current + '.';
        for (JSON::Object::const_iterator it(object.begin());
            it != object.end();
            ++it) {
            std::string key = it->first;
            std::transform(key.begin(), key.end(), key.begin(), tolower);
            if (key.find_first_not_of("abcdefghijklmnopqrstuvwxyz") != std::string::npos)
                continue;
            m_toCheck.push_back(std::make_pair(prefix + key, &it->second));
        }
    }

    void operator()(const JSON::Array &array) const
    {
        // Ignore it
    }

    void operator()(const boost::blank &null) const
    {
        (*this)(std::string());
    }

    void operator()(const std::string &string) const
    {
        if (!m_current.empty()) {
            ConfigVarBase::ptr var = Config::lookup(m_current);
            if (var)
                var->fromString(string);
        }
    }

    template <class T> void operator()(const T &t) const
    {
        (*this)(boost::lexical_cast<std::string>(t));
    }

    std::list<std::pair<std::string, const JSON::Value *> > m_toCheck;
    std::string m_current;
};
}

void
Config::loadFromJSON(const JSON::Value &json)
{
    JSONVisitor visitor;
    visitor.m_toCheck.push_back(std::make_pair(std::string(), &json));
    while (!visitor.m_toCheck.empty()) {
        std::pair<std::string, const JSON::Value *> current =
            visitor.m_toCheck.front();
        visitor.m_toCheck.pop_front();
        visitor.m_current = current.first;
        boost::apply_visitor(visitor, *current.second);
    }
}

ConfigVarBase::ptr
Config::lookup(const std::string &name)
{
    ConfigVarBase var(name);
    ConfigVarBase::ptr ptr(&var, &nop<ConfigVarBase *>);
    std::set<ConfigVarBase::ptr, ConfigVarBase::Comparator>::iterator it = vars().find(ptr);
    if (it != vars().end())
        return *it;
    return ConfigVarBase::ptr();
}

void
Config::visit(boost::function<void (ConfigVarBase::ptr)> dg)
{
    for (std::set<ConfigVarBase::ptr,
            ConfigVarBase::Comparator>::const_iterator it = vars().begin();
        it != vars().end();
        ++it) {
        dg(*it);
    }
}

bool
ConfigVarBase::Comparator::operator()(const ConfigVarBase::ptr &lhs,
                                      const ConfigVarBase::ptr &rhs) const
{
    MORDOR_ASSERT(lhs);
    MORDOR_ASSERT(rhs);
    return lhs->m_name < rhs->m_name;
}

}
