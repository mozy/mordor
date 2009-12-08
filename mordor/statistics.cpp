// Copyright (c) 2009 - Decho Corp.

#include "mordor/pch.h"

#include "statistics.h"

#include "atomic.h"
#include "timer.h"

namespace Mordor {

Statistic *Statistics::lookup(const std::string &name)
{
    StatisticsCache::const_iterator it = stats().find(name);
    if (it == stats().end()) {
        return NULL;
    } else {
        return it->second.second.get();
    }
}

static std::ostream &
dump(std::ostream &os, const Statistic &stat, int level = 0)
{
    std::string indent(" ", level * 4);
    ++level;
    os << indent << typeid(stat).name() << ": " << stat;
    if (stat.units)
        os << " " << stat.units;
    os << std::endl;
    const Statistic *substat = stat.begin();
    while (substat) {
        dump(os, *substat, level);
        substat = stat.next(substat);
    }
    return os;
}

std::ostream &
Statistics::dump(std::ostream &os)
{
    for (StatisticsCache::const_iterator it = statistics().begin();
        it != statistics().end();
        ++it) {
        os << it->first;
        if (!it->second.first.empty())
            os << " (" << it->second.first << ")";
        os << ": ";
        Mordor::dump(os, *it->second.second.get());
    }
    return os;
}

}
