// Copyright (c) 2009 - Decho Corp.

#include "mordor/pch.h"

#include "log.h"

#include <iostream>

#include <boost/bind.hpp>
#include <boost/regex.hpp>

#include "assert.h"
#include "config.h"
#include "fiber.h"
#include "mordor/streams/file.h"
#include "mordor/string.h"

namespace Mordor {

static void enableLoggers();
static void enableStdoutLogging();
static void enableFileLogging();
#ifdef WINDOWS
static void enableDebugLogging();
#endif

static ConfigVar<std::string>::ptr g_logFatal =
    Config::lookup("log.fatalmask", std::string(".*"), "Regex of loggers to enable fatal for.");
static ConfigVar<std::string>::ptr g_logError =
    Config::lookup("log.errormask", std::string(".*"), "Regex of loggers to enable error for.");
static ConfigVar<std::string>::ptr g_logWarn =
    Config::lookup("log.warnmask", std::string(".*"), "Regex of loggers to enable warning for.");
static ConfigVar<std::string>::ptr g_logInfo =
    Config::lookup("log.infomask", std::string(".*"), "Regex of loggers to enable info for.");
static ConfigVar<std::string>::ptr g_logVerbose =
    Config::lookup("log.verbosemask", std::string(""), "Regex of loggers to enable verbose for.");
static ConfigVar<std::string>::ptr g_logDebug =
    Config::lookup("log.debugmask", std::string(""), "Regex of loggers to enable debugging for.");
static ConfigVar<std::string>::ptr g_logTrace =
    Config::lookup("log.tracemask", std::string(""), "Regex of loggers to enable trace for.");

static ConfigVar<bool>::ptr g_logStdout =
    Config::lookup("log.stdout", false, "Log to stdout");
#ifdef WINDOWS
static ConfigVar<bool>::ptr g_logDebugWindow =
    Config::lookup("log.debug", false, "Log to Debug Window");
#endif
static ConfigVar<std::string>::ptr g_logFile =
    Config::lookup("log.file", std::string(""), "Log to file");

static FiberLocalStorage<bool> f_logDisabled;

namespace {

static struct Initializer
{
    Initializer()
    {
        g_logFatal->monitor(&enableLoggers);
        g_logError->monitor(&enableLoggers);
        g_logWarn->monitor(&enableLoggers);
        g_logInfo->monitor(&enableLoggers);
        g_logVerbose->monitor(&enableLoggers);
        g_logDebug->monitor(&enableLoggers);
        g_logTrace->monitor(&enableLoggers);

        g_logFile->monitor(&enableFileLogging);
        g_logStdout->monitor(&enableStdoutLogging);
#ifdef WINDOWS
        g_logDebugWindow->monitor(&enableDebugLogging);
#endif
    }
} g_init;

}

static void enableLogger(Logger::ptr logger, const boost::regex &fatalRegex,
    const boost::regex &errorRegex, const boost::regex &warnRegex,
    const boost::regex &infoRegex, const boost::regex &verboseRegex,
    const boost::regex &debugRegex, const boost::regex &traceRegex)
{
    Log::Level level = Log::NONE;
    if (boost::regex_match(logger->name(), fatalRegex))
        level = Log::FATAL;
    if (boost::regex_match(logger->name(), errorRegex))
        level = Log::ERROR;
    if (boost::regex_match(logger->name(), warnRegex))
        level = Log::WARNING;
    if (boost::regex_match(logger->name(), infoRegex))
        level = Log::INFO;
    if (boost::regex_match(logger->name(), verboseRegex))
        level = Log::VERBOSE;
    if (boost::regex_match(logger->name(), debugRegex))
        level = Log::DEBUG;
    if (boost::regex_match(logger->name(), traceRegex))
        level = Log::TRACE;

    if (logger->level() != level)
        logger->level(level, false);
}

static void enableLoggers()
{
    boost::regex fatalRegex("^" + g_logFatal->val() + "$");
    boost::regex errorRegex("^" + g_logError->val() + "$");
    boost::regex warnRegex("^" + g_logWarn->val() + "$");
    boost::regex infoRegex("^" + g_logInfo->val() + "$");
    boost::regex verboseRegex("^" + g_logVerbose->val() + "$");
    boost::regex debugRegex("^" + g_logDebug->val() + "$");
    boost::regex traceRegex("^" + g_logTrace->val() + "$");
    Log::visit(boost::bind(&enableLogger, _1, boost::cref(fatalRegex),
        boost::cref(errorRegex), boost::cref(warnRegex),
        boost::cref(infoRegex), boost::cref(verboseRegex),
        boost::cref(debugRegex), boost::cref(traceRegex)));
}

static void enableStdoutLogging()
{
    static LogSink::ptr stdoutSink;
    bool log = g_logStdout->val();
    if (stdoutSink.get() && !log) {
        Log::removeSink(stdoutSink);
        stdoutSink.reset();
    } else if (!stdoutSink.get() && log) {
        stdoutSink.reset(new StdoutLogSink());
        Log::addSink(stdoutSink);
    }
}

#ifdef WINDOWS
static void enableDebugLogging()
{
    static LogSink::ptr debugSink;
    bool log = g_logDebugWindow->val();
    if (debugSink.get() && !log) {
        Log::removeSink(debugSink);
        debugSink.reset();
    } else if (!debugSink.get() && log) {
        debugSink.reset(new DebugLogSink());
        Log::addSink(debugSink);
    }
}
#endif

static void enableFileLogging()
{
    static LogSink::ptr fileSink;
    std::string file = g_logFile->val();
    if (fileSink.get() && file.empty()) {
        Log::removeSink(fileSink);
        fileSink.reset();
    } else if (!file.empty()) {
        if (fileSink.get()) {
            if (static_cast<FileLogSink*>(fileSink.get())->file() == file)
                return;
            Log::removeSink(fileSink);
            fileSink.reset();
        }
        fileSink.reset(new FileLogSink(file));
        Log::addSink(fileSink);
    }
}

void
StdoutLogSink::log(const std::string &logger, tid_t thread,
    void *fiber, Log::Level level,
    const std::string &str, const char *file, int line)
{
    std::ostringstream os;
    if (file) {
        os << level << " " << thread << " " << fiber << " "
            << logger << " " << file << ":" << line << " " << str << std::endl;
    } else {
        os << level << " " << thread << " " << fiber << " "
            << logger << " " << str << std::endl;
    }
    std::cout << os.str();
    std::cout.flush();
}

#ifdef WINDOWS
void
DebugLogSink::log(const std::string &logger, tid_t thread,
    void *fiber, Log::Level level,
    const std::string &str, const char *file, int line)
{
    std::wostringstream os;
    if (file) {
        os << level << " " << thread << " " << fiber << " "
            << toUtf16(logger) << " " << toUtf16(file) << ":" << line << " " << toUtf16(str) << std::endl;
    } else {
        os << level << " " << thread << " " << fiber << " "
            << toUtf16(logger) << " " << toUtf16(str) << std::endl;
    }
    OutputDebugStringW(os.str().c_str());
}
#endif

FileLogSink::FileLogSink(const std::string &file)
{
    m_stream.reset(new FileStream(file, FileStream::APPEND,
        FileStream::OPEN_OR_CREATE));
    m_file = file;
}

void
FileLogSink::log(const std::string &logger, tid_t thread,
    void *fiber, Log::Level level,
    const std::string &str, const char *file, int line)
{
    std::ostringstream os;
    if (file) {
        os << level << " " << thread << " " << fiber << " "
            << logger << " " << file << ":" << line << " " << str << std::endl;
    } else {
        os << level << " " << thread << " " << fiber << " "
            << logger << " " << str << std::endl;
    }
    std::string logline = os.str();
    m_stream->write(logline.c_str(), logline.size());
    m_stream->flush();
}

static void deleteNothing(Logger *l)
{}

Logger::ptr Log::root()
{
    static Logger::ptr _root(new Logger());
    return _root;
}

Logger::ptr Log::lookup(const std::string &name)
{
    Logger::ptr log = root();
    if(name.empty() || name == ":"){
        return log;
    }
    std::set<Logger::ptr, LoggerLess>::iterator it;
    Logger dummy(name, log);
    Logger::ptr dummyPtr(&dummy, &deleteNothing);
    size_t start = 0;
    std::string child_name;
    std::string node_name;
    while (start < name.size()) {
        size_t pos = name.find(':', start);
        if(pos == std::string::npos){
            child_name = name.substr(start);
            start = name.size();
        }else{
            child_name = name.substr(start, pos - start);
            start = pos + 1;
        }
        if(child_name.empty()){
            continue;
        }
        if(!node_name.empty()){
            node_name += ":";
        }
        node_name += child_name;
        dummyPtr->m_name = node_name;
        it = log->m_children.lower_bound(dummyPtr);
        if(it == log->m_children.end() || (*it)->m_name != node_name){
            Logger::ptr child(new Logger(node_name, log));
            log->m_children.insert(child);
            log = child;
        }else{
            log = *it;            
        }
    }
    return log;
}

void
Log::visit(boost::function<void (boost::shared_ptr<Logger>)> dg)
{
    std::list<Logger::ptr> toVisit;
    toVisit.push_back(root());
    while (!toVisit.empty())
    {
        Logger::ptr cur = toVisit.front();
        toVisit.pop_front();
        dg(cur);
        for (std::set<Logger::ptr, LoggerLess>::iterator it = cur->m_children.begin();
            it != cur->m_children.end();
            ++it) {
            toVisit.push_back(*it);
        }
    }
}

void
Log::addSink(LogSink::ptr sink)
{
    root()->addSink(sink);
}

void
Log::removeSink(LogSink::ptr sink)
{
    root()->removeSink(sink);
}

void
Log::clearSinks()
{
    root()->clearSinks();
}

LogDisabler::LogDisabler()
{
    m_disabled = !f_logDisabled;
    f_logDisabled = true;
}

LogDisabler::~LogDisabler()
{
    if (m_disabled)
        f_logDisabled = false;
}

bool
LoggerLess::operator ()(const Logger::ptr &lhs,
    const Logger::ptr &rhs) const
{
    return lhs->m_name < rhs->m_name;
}

Logger::Logger()
: m_name(":"),
  m_inheritSinks(false)
{}

Logger::Logger(const std::string &name, Logger::ptr parent)
: m_name(name),
  m_parent(parent),
  m_level(Log::INFO),
  m_inheritSinks(true)
{}

bool
Logger::enabled(Log::Level level)
{
    return m_level >= level && !f_logDisabled;
}

void
Logger::level(Log::Level level, bool propagate)
{
    m_level = level;
    if (propagate) {
        for (std::set<Logger::ptr, LoggerLess>::iterator it(m_children.begin());
            it != m_children.end();
            ++it) {
            (*it)->level(level);
        }
    }
}

void
Logger::removeSink(LogSink::ptr sink)
{
    std::list<LogSink::ptr>::iterator it =
        std::find(m_sinks.begin(), m_sinks.end(), sink);
    if (it != m_sinks.end())
        m_sinks.erase(it);
}

void
Logger::log(Log::Level level, const std::string &str,
    const char *file, int line)
{
    if (str.empty() || !enabled(level))
        return;
    error_t error = lastError();
    LogDisabler disable;
    // TODO: capture timestamp
    Logger::ptr _this = shared_from_this();
#ifdef WINDOWS
    DWORD thread = GetCurrentThreadId();
#elif defined(LINUX)
    pid_t thread = syscall(__NR_gettid);
#else
    pid_t thread = getpid();
#endif
    void *fiber = Fiber::getThis().get();
    bool somethingLogged = false;
    while (_this) {
        for (std::list<LogSink::ptr>::iterator it(_this->m_sinks.begin());
            it != _this->m_sinks.end();
            ++it) {
            somethingLogged = true;
            (*it)->log(m_name, thread, fiber, level, str, file, line);
        }
        if (!_this->m_inheritSinks)
            break;
        _this = _this->m_parent.lock();
    }
    // Restore lastError
    if (somethingLogged)
        lastError(error);
}

LogEvent::~LogEvent()
{
    m_logger->log(m_level, m_os.str(), m_file, m_line);
}

static const char *levelStrs[] = {
    "NONE",
    "FATAL",
    "ERROR",
    "WARN",
    "INFO",
    "VERBOSE",
    "DEBUG",
    "TRACE",
};

std::ostream &operator <<(std::ostream &os, Log::Level level)
{
    MORDOR_ASSERT(level >= Log::FATAL && level <= Log::TRACE);
    return os << levelStrs[level];
}

#ifdef WINDOWS
static const wchar_t *levelStrsw[] = {
    L"NONE",
    L"FATAL",
    L"ERROR",
    L"WARN",
    L"INFO",
    L"VERBOSE",
    L"DEBUG",
    L"TRACE",
};

std::wostream &operator <<(std::wostream &os, Log::Level level)
{
    MORDOR_ASSERT(level >= Log::FATAL && level <= Log::TRACE);
    return os << levelStrsw[level];
}
#endif

}
