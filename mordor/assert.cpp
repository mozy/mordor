// Copyright (c) 2010 - Mozy, Inc.

#include "assert.h"

namespace Mordor {

bool Assertion::throwOnAssertion;

bool isDebuggerAttached()
{
#ifdef WINDOWS
    return !!IsDebuggerPresent();
#elif defined(LINUX)
    bool result = false;
    char buffer[1024];
    snprintf(buffer, 1024, "/proc/%d/status", getpid());
    int fd = open(buffer, O_RDONLY);
    if (fd >= 0) {
        int rc = read(fd, buffer, 1024);
        if (rc > 0) {
            const char *tracerPidStr = strstr(buffer, "TracerPid:");
            if (tracerPidStr) {
                int tracingPid = atoi(tracerPidStr + 13);
                if (tracingPid != 0) {
                    result = true;
                }
            }
        }
        close(fd);
    }
    return result;
#elif defined(OSX)
    int mib[4];
    kinfo_proc info;
    size_t size;
    mib[0] = CTL_KERN;
    mib[1] = KERN_PROC;
    mib[2] = KERN_PROC_PID;
    mib[3] = getpid();
    size = sizeof(kinfo_proc);
    info.kp_proc.p_flag = 0;
    sysctl(mib, 4, &info, &size, NULL, 0);
    return !!(info.kp_proc.p_flag & P_TRACED);
#else
    return false;
#endif
}

void debugBreak()
{
#ifdef WINDOWS
    DebugBreak();
#elif defined (GCC)
#ifdef PPC
    __asm__("li r0, 20\nsc\nnop\nli r0, 37\nli r4, 2\nsc\nnop\n"
                    : : : "memory","r0","r3","r4" );
#elif defined(X86) || defined(X86_64)
    __asm__("int $3\n" : : );
#endif
#endif
}

}
