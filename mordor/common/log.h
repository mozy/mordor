#ifndef __LOG_H__
#define __LOG_H__
// Copyright (c) 2009 - Decho Corp.

#include <list>
#include <set>
#include <sstream>

#include <boost/enable_shared_from_this.hpp>
#include <boost/function.hpp>
#include <boost/noncopyable.hpp>
#include <boost/shared_ptr.hpp>
#include <boost/thread/thread.hpp>

#include "mordor/common/streams/stream.h"
#include "version.h"

#ifdef WINDOWS
#include <windows.h>
#endif

class Logger;
class LogSink;

class LoggerIterator;

class Log
{
private:
    Log();

public:
    enum Level {
        NONE,
        FATAL,
        ERROR,
        WARNING,
        INFO,
        TRACE,
        VERBOSE
    };

    static boost::shared_ptr<Logger> lookup(const std::string &name);

    static void visit(boost::function<void (boost::shared_ptr<Logger>)> dg);

    static void addSink(boost::shared_ptr<LogSink> sink);
    static void clearSinks();
    static void removeSink(boost::shared_ptr<LogSink> sink);

private:
    static Logger *m_root;
    static boost::shared_ptr<Logger> m_rootRef;
};

#ifdef WINDOWS
typedef DWORD tid_t;
#else
typedef pid_t tid_t;
#endif

class LogSink
{
    friend class Logger;
public:
    typedef boost::shared_ptr<LogSink> ptr;
public:

    virtual ~LogSink() {}
    virtual void log(const std::string &logger, tid_t thread, void *fiber,
        Log::Level level, const std::string &str,
        const char *file, int line) = 0;
};

class StdoutLogSink : public LogSink
{
public:
    void log(const std::string &logger, tid_t thread, void *fiber,
        Log::Level level, const std::string &str,
        const char *file, int line);
};

class FileLogSink : public LogSink
{
public:
    FileLogSink(const std::string &file);

    void log(const std::string &logger, tid_t thread, void *fiber,
        Log::Level level, const std::string &str,
        const char *file, int line);

    std::string file() const { return m_file; }

private:
    std::string m_file;
    Stream::ptr m_stream;
};

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

    LogEvent(const LogEvent &copy)
        : m_logger(copy.m_logger),
          m_level(copy.m_level),
          m_file(copy.m_file),
          m_line(copy.m_line)
    {}
public:
    ~LogEvent();
    std::ostream &os() { return m_os; }

private:
    boost::shared_ptr<Logger> m_logger;
    Log::Level m_level;
    const char *m_file;
    int m_line;
    std::ostringstream m_os;
};

struct LoggerLess
{
    bool operator()(const boost::shared_ptr<Logger> &lhs,
        const boost::shared_ptr<Logger> &rhs) const;
};

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
    bool enabled(Log::Level level) { return m_level >= level; }
    void level(Log::Level level, bool propagate = true);
    Log::Level level() const { return m_level; }

    bool inheritSinks() const { return m_inheritSinks; }
    void inheritSinks(bool inherit) { m_inheritSinks = inherit; }
    void addSink(LogSink::ptr sink) { m_sinks.push_back(sink); }
    void removeSink(LogSink::ptr sink);
    void clearSinks() { m_sinks.clear(); }

    LogEvent log(Log::Level level, const char *file = NULL, int line = -1)
    { return LogEvent(shared_from_this(), level, file, line); }
    void log(Log::Level level, const std::string &str, const char *file = NULL, int line = 0);

    LogEvent verbose(const char *file = NULL, int line = -1)
    { return log(Log::VERBOSE, file, line); }
    LogEvent trace(const char *file = NULL, int line = -1)
    { return log(Log::TRACE, file, line); }
    LogEvent info(const char *file = NULL, int line = -1)
    { return log(Log::INFO, file, line); }
    LogEvent warning(const char *file = NULL, int line = -1)
    { return log(Log::WARNING, file, line); }
    LogEvent error(const char *file = NULL, int line = -1)
    { return log(Log::ERROR, file, line); }
    LogEvent fatal(const char *file = NULL, int line = -1)
    { return log(Log::FATAL, file, line); }

    std::string name() const { return m_name; }

private:
    std::string m_name;
    Logger::ptr m_parent;
    std::set<Logger::ptr, LoggerLess> m_children;
    Log::Level m_level;
    std::list<LogSink::ptr> m_sinks;
    bool m_inheritSinks;
};

#define LOG_VERBOSE(log) (log)->verbose(__FILE__, __LINE__).os()
#define LOG_TRACE(log) (log)->trace(__FILE__, __LINE__).os()
#define LOG_INFO(log) (log)->info(__FILE__, __LINE__).os()
#define LOG_WARNING(log) (log)->warning(__FILE__, __LINE__).os()
#define LOG_ERROR(log) (log)->error(__FILE__, __LINE__).os()
#define LOG_FATAL(log) (log)->fatal(__FILE__, __LINE__).os()
#define LOG_LEVEL(lg, level) (lg)->log(level, __FILE__, __LINE__).os()

std::ostream &operator <<(std::ostream &os, Log::Level level);

#endif
