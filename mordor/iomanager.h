#ifndef __MORDOR_IOMANAGER_H__
#define __MORDOR_IOMANAGER_H__
// Copyright (c) 2009 - Mozy, Inc.

#include "version.h"

#ifdef WINDOWS
#include "iomanager_iocp.h"
namespace Mordor {
typedef IOManagerIOCP IOManager;
typedef AsyncEventIOCP AsyncEvent;
}
#elif defined(LINUX)
#include "iomanager_epoll.h"
namespace Mordor {
typedef IOManagerEPoll IOManager;
}
#elif defined(OSX)
#include "iomanager_event.h"
namespace Mordor {
typedef IOManagerEvent IOManager;
}
#elif defined(BSD)
#include "iomanager_kqueue.h"
namespace Mordor {
typedef IOManagerKQueue IOManager;
}
#endif

#endif
