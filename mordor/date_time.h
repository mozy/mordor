#ifndef __MORDOR_DATE_TIME_H__
#define __MORDOR_DATE_TIME_H__
// Copyright (c) 2009 - Decho Corp.

#include <boost/date_time/posix_time/posix_time.hpp>

namespace Mordor {
    time_t toTimeT(const boost::posix_time::ptime &ptime);
};

#endif
