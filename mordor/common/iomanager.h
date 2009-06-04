#ifndef __IOMANAGER_H__
#define __IOMANAGER_H__
// Copyright (c) 2009 - Decho Corp.

#include "version.h"

#ifdef WINDOWS
#include "iomanager_iocp.h"
typedef IOManagerIOCP IOManager;
typedef AsyncEventIOCP AsyncEvent;
#elif defined(LINUX)
#include "iomanager_epoll.h"
typedef IOManagerEPoll IOManager;
#endif

#endif
