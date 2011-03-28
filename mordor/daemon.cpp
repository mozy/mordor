// Copyright (c) 2010 - Mozy, Inc.

#include "daemon.h"

#include "config.h"
#include "log.h"
#include "main.h"

namespace Mordor {
namespace Daemon {

static Logger::ptr g_log = Log::lookup("mordor:daemon");

static ConfigVar<bool>::ptr g_daemonize = Config::lookup("daemonize", false, "Force daemonize");

boost::signals2::signal<void ()> onTerminate;
boost::signals2::signal<void ()> onInterrupt;
boost::signals2::signal<void ()> onReload;
boost::signals2::signal<void ()> onPause;
boost::signals2::signal<void ()> onContinue;

#define CTRL_CASE(ctrl)             \
    case ctrl:                      \
        return os << #ctrl;         \

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

namespace {

enum ServiceCtrl {};

std::ostream &operator <<(std::ostream &os, ServiceCtrl ctrl)
{
    switch (ctrl) {
        CTRL_CASE(SERVICE_CONTROL_STOP);
        CTRL_CASE(SERVICE_CONTROL_PAUSE);
        CTRL_CASE(SERVICE_CONTROL_CONTINUE);
        CTRL_CASE(SERVICE_CONTROL_INTERROGATE);
        CTRL_CASE(SERVICE_CONTROL_SHUTDOWN);
        CTRL_CASE(SERVICE_CONTROL_PARAMCHANGE);
        CTRL_CASE(SERVICE_CONTROL_NETBINDADD);
        CTRL_CASE(SERVICE_CONTROL_NETBINDREMOVE);
        CTRL_CASE(SERVICE_CONTROL_NETBINDENABLE);
        CTRL_CASE(SERVICE_CONTROL_NETBINDDISABLE);
        CTRL_CASE(SERVICE_CONTROL_DEVICEEVENT);
        CTRL_CASE(SERVICE_CONTROL_HARDWAREPROFILECHANGE);
        CTRL_CASE(SERVICE_CONTROL_POWEREVENT);
        CTRL_CASE(SERVICE_CONTROL_SESSIONCHANGE);
        CTRL_CASE(SERVICE_CONTROL_PRESHUTDOWN);
#ifdef SERVICE_CONTROL_TIMECHANGE
        CTRL_CASE(SERVICE_CONTROL_TIMECHANGE);
#endif
#ifdef SERVICE_CONTROL_TRIGGEREVENT
        CTRL_CASE(SERVICE_CONTROL_TRIGGEREVENT);
#endif
        default:
            return os << (int)ctrl;
    }
}

}

static DWORD WINAPI HandlerEx(DWORD dwControl, DWORD dwEventType,
    LPVOID lpEventData, LPVOID lpContext)
{
    ServiceStatus *status = (ServiceStatus *)lpContext;
    MORDOR_LOG_DEBUG(g_log) << "Received service control "
        << (ServiceCtrl)dwControl;

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
    MORDOR_LOG_TRACE(g_log) << "Service control " << (ServiceCtrl)dwControl
        << " handled";
    // If it's stop, onTerminate may cause ServiceMain to exit immediately,
    // before onTerminate returns, and our pointer to status on ServiceMain's
    // stack would then be invalid; we update the service status in ServiceMain
    // anyway, so it's okay to skip it here
    if (dwControl != SERVICE_CONTROL_STOP) {
        status->status.dwControlsAccepted = acceptedControls();
        SetServiceStatus(status->serviceHandle, &status->status);
    }
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

    MORDOR_LOG_INFO(g_log) << "Starting service";
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
    MORDOR_LOG_INFO(g_log) << "Service stopped";
}

namespace {

enum ConsoleCtrl {};

std::ostream &operator <<(std::ostream &os, ConsoleCtrl ctrl)
{
    switch (ctrl) {
        CTRL_CASE(CTRL_C_EVENT);
        CTRL_CASE(CTRL_BREAK_EVENT);
        CTRL_CASE(CTRL_CLOSE_EVENT);
        CTRL_CASE(CTRL_LOGOFF_EVENT);
        CTRL_CASE(CTRL_SHUTDOWN_EVENT);
        default:
            return os << (int)ctrl;
    }
}

}

static BOOL WINAPI HandlerRoutine(DWORD dwCtrlType)
{
    MORDOR_LOG_DEBUG(g_log) << "Received console control "
        << (ConsoleCtrl)dwCtrlType;
    switch (dwCtrlType) {
        case CTRL_C_EVENT:
            if (onInterrupt.empty())
                return FALSE;
            onInterrupt();
            break;
        case CTRL_CLOSE_EVENT:
        case CTRL_BREAK_EVENT:
            if (onTerminate.empty())
                return FALSE;
            onTerminate();
            break;
        default:
            return FALSE;
    }
    MORDOR_LOG_TRACE(g_log) << "Console control " << (ServiceCtrl)dwCtrlType
        << " handled";
    return TRUE;
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
            MORDOR_LOG_INFO(g_log) << "Starting console process";
            SetConsoleCtrlHandler(&HandlerRoutine, TRUE);
            int result = daemonMain(argc, argv);
            SetConsoleCtrlHandler(&HandlerRoutine, FALSE);
            MORDOR_LOG_INFO(g_log) << "Console process stopped";
            return result;
        } else {
            return GetLastError();
        }
    } else {
        return 0;
    }
}
#else

#include <errno.h>
#include <signal.h>

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

namespace {

enum Signal {};

std::ostream &operator <<(std::ostream &os, Signal signal)
{
    switch ((int)signal) {
        CTRL_CASE(SIGTERM);
        CTRL_CASE(SIGINT);
        CTRL_CASE(SIGTSTP);
        CTRL_CASE(SIGCONT);
        CTRL_CASE(SIGHUP);
        default:
            return os << (int)signal;
    }
}

}

static void *signal_thread(void *arg)
{
    sigset_t mask = blockedSignals();
    while (true) {
        int caught;
        int rc = sigwait(&mask, &caught);
        if (rc != 0)
            continue;
        MORDOR_LOG_DEBUG(g_log) << "Received signal " << (Signal)caught;
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
                continue;
        }
        MORDOR_LOG_TRACE(g_log) << "Signal " << (Signal)caught
            << " handled";
    }

    return NULL;
}

#ifndef OSX
static bool shouldDaemonize(char **enviro)
{
    if (!enviro)
        return false;
    std::string parent;
    for (const char *env = *enviro; *env; env += strlen(env) + 1) {
        const char *equals = strchr(env, '=');
        if (equals != env + 1 || *env != '_')
            continue;
        parent = equals + 1;
        break;
    }
    if (parent.size() >= 12 &&
        strncmp(parent.c_str(), "/etc/init.d/", 12) == 0)
        return true;
    if (parent.size() >= 17 &&
        strcmp(parent.c_str() + parent.size() - 17, "start-stop-daemon") == 0)
        return true;
    return false;
}

static bool shouldDaemonizeDueToParent()
{
    std::ostringstream os;
    os << "/proc/" << getppid() << "/environ";
    std::string parentEnviron;
    parentEnviron.resize(65536);
    int fd = open(os.str().c_str(), O_RDONLY);
    if (fd < 0)
        return false;
    int size = read(fd, &parentEnviron[0], 65536);
    close(fd);
    if (size < 0)
        return false;
    parentEnviron.resize(size + 1);
    const char *parentEnviro = parentEnviron.c_str();
    return shouldDaemonize((char **)&parentEnviro);
}
#endif

int run(int argc, char **argv,
    boost::function<int (int, char **)> daemonMain)
{
#ifndef OSX
    // Check for being run from /etc/init.d or start-stop-daemon as a hint to
    // daemonize
    bool daemonize_from_commandline = g_daemonize->val();
    if (shouldDaemonize(environ) || shouldDaemonizeDueToParent() || daemonize_from_commandline) {
        MORDOR_LOG_VERBOSE(g_log) << "Daemonizing";
        if (daemon(0, 0) == -1)
            return errno;
    }
#endif

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
    MORDOR_LOG_INFO(g_log) << "Starting daemon";
    rc = daemonMain(argc, argv);
    MORDOR_LOG_INFO(g_log) << "Daemon stopped";
    return rc;
}
#endif

}}
