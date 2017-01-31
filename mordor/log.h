#ifndef __MORDOR_LOG_H__
#define __MORDOR_LOG_H__
// Copyright (c) 2009 - Mozy, Inc.

#include <list>
#include <set>
#include <sstream>

#include "predef.h"
#include "log_base.h"
#include <boost/enable_shared_from_this.hpp>
#include <boost/noncopyable.hpp>
#include <boost/date_time/posix_time/ptime.hpp>

// For tid_t
#include "thread.h"
#include "version.h"

#ifdef WINDOWS
#include <windows.h>
#endif

namespace Mordor {

class Logger;
class LogSink;

class LoggerIterator;

class Stream;

/// @sa LogMacros

/// Abstract base class for receiving log messages
/// @sa Log
class LogSink
{
    friend class Logger;
public:
    typedef boost::shared_ptr<LogSink> ptr;
public:

    virtual ~LogSink() {}
    /// @brief Receives details of a single log message
    /// @param logger The Logger that generated the message
    /// @param now The timestamp when the message was generated
    /// @param elapsed Microseconds since the process started when the message
    /// was generated
    /// @param thread The id of the thread that generated the message
    /// @param fiber An opaque pointer to the fiber that generated the message
    /// @param level The level of the message
    /// @param str The log message itself
    /// @param file The source file where the message was generated
    /// @param line The source line where the message was generated
    virtual void log(const std::string &logger,
        boost::posix_time::ptime now, unsigned long long elapsed,
        tid_t thread, void *fiber,
        Log::Level level, const std::string &str,
        const char *file, int line) = 0;
};

/// A LogSink that dumps message to stdout (std::cout)
class StdoutLogSink : public LogSink
{
public:
    void log(const std::string &logger,
        boost::posix_time::ptime now, unsigned long long elapsed,
        tid_t thread, void *fiber,
        Log::Level level, const std::string &str,
        const char *file, int line);
};

#ifdef WINDOWS
/// A LogSink that dumps messages to the Visual Studio Debug Output Window
///
/// Using OutputDebugString
class DebugLogSink : public LogSink
{
public:
    void log(const std::string &logger,
        boost::posix_time::ptime now, unsigned long long elapsed,
        tid_t thread, void *fiber,
        Log::Level level, const std::string &str,
        const char *file, int line);
};
#else
/// A LogSink that sends messages to syslog
class SyslogLogSink : public LogSink
{
public:
    /// @param facility The facility to mark messages with
    SyslogLogSink(int facility);

    void log(const std::string &logger,
        boost::posix_time::ptime now, unsigned long long elapsed,
        tid_t thread, void *fiber,
        Log::Level level, const std::string &str,
        const char *file, int line);

    int facility() const { return m_facility; }

    static int facilityFromString(const char *str);
    static const char *facilityToString(int facility);

private:
    int m_facility;
};
#endif

/// A LogSink that appends messages to a file
///
/// The file is opened in append mode, so multiple processes and threads can
/// log to the same file simultaneously, without fear of corrupting each
/// others' messages.  The messages will still be intermingled, but each one
/// will be atomic
class FileLogSink : public LogSink
{
public:
    /// @param file The file to open and log to.  If it does not exist, it is
    /// created.
    FileLogSink(const std::string &file);

    void log(const std::string &logger,
        boost::posix_time::ptime now, unsigned long long elapsed,
        tid_t thread, void *fiber,
        Log::Level level, const std::string &str,
        const char *file, int line);

    std::string file() const { return m_file; }

private:
    std::string m_file;
    boost::shared_ptr<Stream> m_stream;
};

/// LogEvent is an intermediary class.  It is returned by Logger::log, owns a
/// std::ostream, and on destruction it will log whatever was streamed to it.
/// It *is* copyable, because it is returned from Logger::log, but shouldn't
/// be copied, because when it destructs it will log whatever was built up so
/// far, in addition to the copy logging it.
struct LogEvent
{
    friend class Logger;
private:
    LogEvent(boost::shared_ptr<Logger> logger, Log::Level level,
        const char *file, int line)
        : m_logger(logger),
          m_level(level),
          m_file(file),
          m_line(line)
    {}

public:
    LogEvent(const LogEvent &copy)
        : m_logger(copy.m_logger),
          m_level(copy.m_level),
          m_file(copy.m_file),
          m_line(copy.m_line)
    {}

    ~LogEvent();
    std::ostream &os() { return m_os; }

private:
    boost::shared_ptr<Logger> m_logger;
    Log::Level m_level;
    const char *m_file;
    int m_line;
    std::ostringstream m_os;
};

/// Temporarily disables logging for this Fiber
struct LogDisabler
{
    LogDisabler();
    ~LogDisabler();

private:
    bool m_disabled;
};

struct LoggerLess
{
    bool operator()(const boost::shared_ptr<Logger> &lhs,
        const boost::shared_ptr<Logger> &rhs) const;
};

/// An individual Logger.
/// @sa Log
/// @sa LogMacros
class Logger : public boost::enable_shared_from_this<Logger>
{
    friend class Log;
    friend struct LoggerLess;
public:
    typedef boost::shared_ptr<Logger> ptr;
private:
    Logger();
    Logger(const std::string &name, Logger::ptr parent);

public:
    ~Logger();

    /// @return If this logger is enabled at level
    bool enabled(Log::Level level);
    /// Set this logger to level
    /// @param level The level to set it to
    /// @param propagate Automatically set all child Loggers to this level also
    void level(Log::Level level, bool propagate = true);
    /// @return The current level this Logger is set to
    Log::Level level() const { return m_level; }

    /// @return If this logger will inherit LogSinks from its parent
    bool inheritSinks() const { return m_inheritSinks; }
    /// Set if this logger will inherit LogSinks from its parent
    void inheritSinks(bool inherit) { m_inheritSinks = inherit; }
    /// Add sink to this Logger
    void addSink(LogSink::ptr sink) { m_sinks.push_back(sink); }
    /// Remove sink from this Logger
    void removeSink(LogSink::ptr sink);
    /// Remove all LogSinks from this logger
    void clearSinks() { m_sinks.clear(); }

    /// Return a LogEvent to use to stream a log message applicable to this
    /// Logger
    /// @param level The level this message will be
    LogEvent log(Log::Level level, const char *file = NULL, int line = -1)
    { return LogEvent(shared_from_this(), level, file, line); }
    /// Log a message from this Logger
    /// @param level The level of this message
    /// @param str The message
    void log(Log::Level level, const std::string &str, const char *file = NULL, int line = 0);

    /// @return The full name of this Logger
    std::string name() const { return m_name; }

    /// @return The list of sinks for this Logger
    const std::list<LogSink::ptr>& sinks() const { return m_sinks; }

private:
    std::string m_name;
    boost::weak_ptr<Logger> m_parent;
    std::set<Logger::ptr, LoggerLess> m_children;
    Log::Level m_level;
    std::list<LogSink::ptr> m_sinks;
    bool m_inheritSinks;
};

/// @defgroup LogMacros Logging Macros
/// Macros that automatically capture the current file and line, and return
/// a std::ostream & to stream the log message to.  Note that it is *not* an
/// rvalue, because it is put inside an un-scoped if statement, so that the
/// entire streaming of the log statement can be skipped if the Logger is not
/// enabled at the specified level.
/// @sa Log
/// @{

/// @brief Log at a particular level
/// @param level The level to log at
#define MORDOR_LOG_LEVEL(lg, level) if ((lg)->enabled(level))                   \
    (lg)->log(level, __FILE__, __LINE__).os()
/// Log a fatal error
#define MORDOR_LOG_FATAL(log) MORDOR_LOG_LEVEL(log, ::Mordor::Log::FATAL)
/// Log an error
#define MORDOR_LOG_ERROR(log) MORDOR_LOG_LEVEL(log, ::Mordor::Log::ERROR)
/// Log a warning
#define MORDOR_LOG_WARNING(log) MORDOR_LOG_LEVEL(log, ::Mordor::Log::WARNING)
/// Log an informational message
#define MORDOR_LOG_INFO(log) MORDOR_LOG_LEVEL(log, ::Mordor::Log::INFO)
/// Log a verbose message
#define MORDOR_LOG_VERBOSE(log) MORDOR_LOG_LEVEL(log, ::Mordor::Log::VERBOSE)
/// Log a debug message
#define MORDOR_LOG_DEBUG(log) MORDOR_LOG_LEVEL(log, ::Mordor::Log::DEBUG)
/// Log a trace message
#define MORDOR_LOG_TRACE(log) MORDOR_LOG_LEVEL(log, ::Mordor::Log::TRACE)
/// @}

/// Streams a Log::Level as a string, instead of an integer
std::ostream &operator <<(std::ostream &os, Mordor::Log::Level level);
#ifdef WINDOWS
/// Streams a Log::Level as a string, instead of an integer
std::wostream &operator <<(std::wostream &os, Mordor::Log::Level level);
#endif

}


#endif
