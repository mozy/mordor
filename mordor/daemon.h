#ifndef __MORDOR_SERVICE_H__
#define __MORDOR_SERVICE_H__
// Copyright (c) 2010 - Mozy, Inc.

#include <string>
#ifdef POSIX
#include <sys/types.h>
#endif
#include <vector>

#include <boost/function.hpp>
#include <boost/signals2/signal.hpp>

#include "version.h"

namespace Mordor {
namespace Daemon {

/// @brief Run a process as a daemon
///
/// Runs daemonMain as a daemon process, that can be externally controlled.
/// It is cross platform, and will adapt for how this process is run.
/// The words in parenthesis describe behavior for how the signal handler
/// interacts with the platform's normal actions:
///     * default: if no slots are connected to the signal, it gets passed
///       to the default handler for that platform
///     * always: the default handler for that platform is always invoked
///       (after all slots are signalled)
///     * ignored: the default handler for that platform is ignored
///     * ExitProcess: there is no default handler for a Windows System Service
///       so a stop request that is unhandled will terminate the process
///
/// Windows Console:
///     * argc/argv are passed through
///     * Ctrl-C triggers onInterrupt (default)
///     * Ctrl-Break and End Task trigger onTerminate (default)
///
/// Windows System Service:
///     * argc/argv are ignored; daemonMain receives the arguments from
///       ServiceStart
///     * SERVICE_CONTROL_STOP triggers onTerminate (ExitProcess)
///     * SERVICE_CONTROL_PAUSE triggers onPause (ignored)
///     * SERVICE_CONTROL_CONTINUE triggers onContinue (ignored)
///     * SERVICE_CONTROL_PARAMCHANGE triggers onReload (ignored)
///
/// POSIX:
///     * argc/argv are passed through
///     * SIGTERM triggers onTerminate (default)
///     * SIGINT triggers onInterrupt (default)
///     * SIGTSTP triggers onPause (always)
///     * SIGCONT triggers onContinue (always)
///     * SIGHUP triggers onReload (default)
///
/// @note On Windows run automagically determines if it is being run as a
///       system service or from the console.
///       On OS X, you should be using launchd, and never daemonize.
///       On Linux, run attempts to automagically determine if it needs to
///       daemonize by seeing if it is being run directly from an script in
///       /etc/init.d, or by start-stop-daemon (Debian)
/// @note If enableWatchdog is true, the daemonMain will be run in a forked
///       process, the parent process will restart it whenever it dies.
///       onChildProcessExit is also provided to empower user to stop restarting
///       and quit when necessary. (works for POSIX only)
/// @note In all cases, the signals are invoked on a thread separate from
///       the thread daemonMain is called on, or any that it created
/// @note run should be called *exactly* once, since the signals are global
///       for the process
int run(int argc, char **argv, boost::function<int (int, char **)> daemonMain,
        bool enableWatchdog = false);

extern boost::signals2::signal<void ()> onTerminate;
extern boost::signals2::signal<void ()> onInterrupt;
extern boost::signals2::signal<void ()> onReload;
extern boost::signals2::signal<void ()> onPause;
extern boost::signals2::signal<void ()> onContinue;
#ifdef POSIX
/// Works together with watchdog, callback whenever child process exits.
/// Watchdog will check the return value to restart child (if false) or
/// just shutdown the program (if true).
/// @note If this function is not hooked, watchdog will by default repeatly
///       restart child process whenever it exits. If one-shot running is
///       expected, please set enableWatchdog = false in run.
extern boost::function<bool (pid_t, int)> onChildProcessExit;
#endif

}}

#endif
