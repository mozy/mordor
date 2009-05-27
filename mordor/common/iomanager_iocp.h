#ifndef __IOMANAGER_IOCP_H__
#define __IOMANAGER_IOCP_H__

#include <map>

#include <boost/thread/mutex.hpp>

#include "fiber.h"
#include "scheduler.h"
#include "version.h"

#ifndef WINDOWS
#error IOManagerIOCP is Windows only
#endif

struct AsyncEventIOCP
{
    BOOL ret;
    OVERLAPPED overlapped;
    DWORD numberOfBytes;
    ULONG_PTR completionKey;
    DWORD lastError;

    Scheduler  *m_scheduler;
    Fiber::ptr  m_fiber;
};

class IOManagerIOCP : public Scheduler
{
public:
    IOManagerIOCP(int threads = 1, bool useCaller = true);

    void registerFile(HANDLE handle);
    void registerEvent(AsyncEventIOCP *e);
    
protected:
    void idle();
    void tickle();

private:
    HANDLE m_hCompletionPort;
    std::map<OVERLAPPED *, AsyncEventIOCP*> m_pendingEvents;
    boost::mutex m_mutex;
};

#endif __IOMANAGER_IOCP_H__
