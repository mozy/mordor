// Copyright (c) 2010 - Mozy, Inc.

#include "mordor/pch.h"

#include "daemon.h"

#include "main.h"

namespace Mordor {
namespace Daemon {

boost::signals2::signal<void ()> onTerminate;
boost::signals2::signal<void ()> onInterrupt;
boost::signals2::signal<void ()> onReload;
boost::signals2::signal<void ()> onPause;
boost::signals2::signal<void ()> onContinue;

#ifdef WINDOWS
static boost::function<int (int, char **)> g_daemonMain;

namespace {
struct ServiceStatus
{
    SERVICE_STATUS_HANDLE serviceHandle;
    SERVICE_STATUS status;
};
}

static DWORD acceptedControls()
{
    DWORD accepted = 0;
    if (!onTerminate.empty())
        accepted |= SERVICE_ACCEPT_STOP;
    if (!onReload.empty())
        accepted |= SERVICE_ACCEPT_PARAMCHANGE;
    if (!onPause.empty())
        accepted |= SERVICE_ACCEPT_PAUSE_CONTINUE;
    return accepted;
}

static DWORD WINAPI HandlerEx(DWORD dwControl, DWORD dwEventType,
    LPVOID lpEventData, LPVOID lpContext)
{
    ServiceStatus *status = (ServiceStatus *)lpContext;

    switch (dwControl) {
        case SERVICE_CONTROL_INTERROGATE:
            break;
        case SERVICE_CONTROL_PARAMCHANGE:
            onReload();
            break;
        case SERVICE_CONTROL_STOP:
            if (onTerminate.empty())
                ExitProcess(0);
            onTerminate();
            break;
        case SERVICE_CONTROL_PAUSE:
            if (!onPause.empty())
                status->status.dwCurrentState = SERVICE_PAUSED;
            onPause();
            break;
        case SERVICE_CONTROL_CONTINUE:
            onContinue();
            status->status.dwCurrentState = SERVICE_RUNNING;
            break;
        default:
            return ERROR_CALL_NOT_IMPLEMENTED;
    }
    status->status.dwControlsAccepted = acceptedControls();
    SetServiceStatus(status->serviceHandle, &status->status);
    return NO_ERROR;
}

static void WINAPI ServiceMain(DWORD argc, LPWSTR *argvW)
{
    ServiceStatus status;
    status.status.dwServiceType = SERVICE_WIN32_OWN_PROCESS;
    status.status.dwCurrentState = SERVICE_RUNNING;
    status.status.dwControlsAccepted = acceptedControls();
    status.status.dwWin32ExitCode = NO_ERROR;
    status.status.dwServiceSpecificExitCode = 0;
    status.status.dwCheckPoint = 0;
    status.status.dwWaitHint = 0;

    status.serviceHandle = RegisterServiceCtrlHandlerExW(NULL,
        &HandlerEx, &status);
    if (!status.serviceHandle)
        return;

    if (!SetServiceStatus(status.serviceHandle, &status.status))
        return;

    char **argv = CommandLineToUtf8(argc, argvW);
    if (argv) {
        try {
            status.status.dwServiceSpecificExitCode =
                g_daemonMain(argc, argv);
            LocalFree(argv);
        } catch (...) {
            LocalFree(argv);
            throw;
        }
    } else {
        status.status.dwServiceSpecificExitCode = GetLastError();
    }
    status.status.dwCurrentState = SERVICE_STOPPED;
    if (status.status.dwServiceSpecificExitCode)
        status.status.dwWin32ExitCode = ERROR_SERVICE_SPECIFIC_ERROR;
    SetServiceStatus(status.serviceHandle, &status.status);
}

static BOOL WINAPI HandlerRoutine(DWORD dwCtrlType)
{
    switch (dwCtrlType) {
        case CTRL_C_EVENT:
            if (onInterrupt.empty())
                return FALSE;
            onInterrupt();
            return TRUE;
        case CTRL_CLOSE_EVENT:
        case CTRL_BREAK_EVENT:
            if (onTerminate.empty())
                return FALSE;
            onTerminate();
            return TRUE;
        default:
            return FALSE;
    }
}

int run(int argc, char **argv,
    boost::function<int (int, char **)> daemonMain)
{
    SERVICE_TABLE_ENTRYW ServiceStartTable[] = {
        { L"", &ServiceMain },
        { NULL,                NULL }
    };
    g_daemonMain = daemonMain;
    if (!StartServiceCtrlDispatcherW(ServiceStartTable)) {
        if (GetLastError() == ERROR_FAILED_SERVICE_CONTROLLER_CONNECT) {
            SetConsoleCtrlHandler(&HandlerRoutine, TRUE);
            int result = daemonMain(argc, argv);
            SetConsoleCtrlHandler(&HandlerRoutine, FALSE);
            return result;
        } else {
            return GetLastError();
        }
    } else {
        return 0;
    }
}
#else
static sigset_t blockedSignals()
{
    sigset_t result;
    sigemptyset(&result);
    sigaddset(&result, SIGTERM);
    sigaddset(&result, SIGINT);
    sigaddset(&result, SIGHUP);
    sigaddset(&result, SIGTSTP);
    sigaddset(&result, SIGCONT);
    return result;
}

static void unblockAndRaise(int signal)
{
    sigset_t set;
    sigemptyset(&set);
    sigaddset(&set, signal);
    pthread_sigmask(SIG_UNBLOCK, &set, NULL);
    raise(signal);
    pthread_sigmask(SIG_BLOCK, &set, NULL);
}

static void *signal_thread(void *arg)
{
    sigset_t mask = blockedSignals();
    while (true) {
        int caught;
        int rc = sigwait(&mask, &caught);
        if (rc != 0)
            continue;
        switch (caught) {
            case SIGTERM:
                if (onTerminate.empty()) {
                    unblockAndRaise(caught);
                    continue;
                }
                onTerminate();
                break;
            case SIGINT:
                if (onInterrupt.empty()) {
                    unblockAndRaise(caught);
                    continue;
                }
                onInterrupt();
                break;
            case SIGTSTP:
                onPause();
                unblockAndRaise(caught);
                break;
            case SIGCONT:
                onContinue();
                unblockAndRaise(caught);
                break;
            case SIGHUP:
                if (onReload.empty()) {
                    unblockAndRaise(caught);
                    continue;
                }
                onReload();
                break;
            default:
                break;
        }
    }

    return NULL;
}

int run(int argc, char **argv,
    boost::function<int (int, char **)> daemonMain)
{
    // Mask signals from other threads so we can handle them
    // ourselves
    sigset_t mask = blockedSignals();
    int rc = pthread_sigmask(SIG_BLOCK, &mask, NULL);
    if (rc != 0)
        return errno;

    // Create the thread that will handle signals for us
    pthread_t signal_thread_id;
    rc = pthread_create(&signal_thread_id, NULL, &signal_thread, NULL);
    if (rc != 0)
        return errno;

    // Run the daemon's main
    return daemonMain(argc, argv);
}
#endif

}}
