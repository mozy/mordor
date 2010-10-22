// Copyright (c) 2009 - Decho Corporation

#include "date_time.h"

namespace Mordor {

time_t toTimeT(const boost::posix_time::ptime &ptime)
{
    static const boost::posix_time::ptime time_t_epoch(boost::gregorian::date(1970,1,1));
    boost::posix_time::time_duration diff = ptime - time_t_epoch;
    return diff.total_seconds();
}

}
