// Copyright (c) 2009 - Decho Corp.

#include "log.h"

#include <iostream>

#include "assert.h"
#include "fiber.h"

void
StdoutLogSink::log(const std::string &logger, tid_t thread,
    void *fiber, Log::Level level,
    const std::string &str, const char *file, int line)
{
    if (file) {
        std::cout << level << " " << thread << " " << fiber << " "
            << logger << " " << file << ":" << line << " " << str << std::endl;
    } else {
        std::cout << level << " " << thread << " " << fiber << " "
            << logger << " " << str << std::endl;
    }
}

static void deleteNothing(Logger *l)
{}

Logger *Log::m_root;
Logger::ptr Log::m_rootRef(m_root);

Logger::ptr Log::lookup(const std::string &name)
{
    if (!m_rootRef)
        m_rootRef.reset(m_root = new Logger());
    Logger::ptr log = m_rootRef;
    std::set<Logger::ptr, LoggerLess>::iterator it;
    Logger dummy(name, m_rootRef);
    Logger::ptr dummyPtr(&dummy, &deleteNothing);
    while (true) {
        it = log->m_children.lower_bound(dummyPtr);
        if (it == log->m_children.end()) {
            Logger::ptr child(new Logger(name, log));
            log->m_children.insert(child);
            return child;
        }
        if ((*it)->m_name == name)
            return *it;
        // Child of existing logger
        if (name.length() > (*it)->m_name.length() &&
            name[(*it)->m_name.length()] == '.' &&
            strncmp((*it)->m_name.c_str(), name.c_str(), (*it)->m_name.length()) == 0) {
            log = *it;
            continue;
        }
        ++it;
        // Existing logger should be child of this logger
        if (it != log->m_children.end() &&
            (*it)->m_name.length() > name.length() &&
            (*it)->m_name[name.length()] == '.' &&
            strncmp((*it)->m_name.c_str(), name.c_str(), name.length()) == 0) {
            Logger::ptr child = *it;
            Logger::ptr parent(new Logger(name, log));
            log->m_children.erase(it);
            log->m_children.insert(parent);
            parent->m_children.insert(child);
            child->m_parent = parent;
            return parent;
        }
        Logger::ptr child(new Logger(name, log));
        log->m_children.insert(child);
        return child;        
    }
}

void
Log::addSink(LogSink::ptr sink)
{
    if (!m_rootRef)
        m_rootRef.reset(m_root = new Logger());
    m_root->addSink(sink);
}

void
Log::clearSinks()
{
    if (!m_rootRef)
        m_rootRef.reset(m_root = new Logger());
    m_root->clearSinks();
}

bool
LoggerLess::operator ()(const Logger::ptr &lhs,
    const Logger::ptr &rhs) const
{
    return lhs->m_name < rhs->m_name;
}


Logger::Logger()
: m_inheritSinks(false)
{}

Logger::Logger(const std::string &name, Logger::ptr parent)
: m_name(name),
  m_parent(parent),
  m_level(Log::INFO),
  m_inheritSinks(true)
{}

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
Logger::log(Log::Level level, const std::string &str,
    const char *file, int line)
{
    if (str.empty() || !enabled(level))
        return;
    // TODO: capture timestamp
    Logger::ptr _this = shared_from_this();
#ifdef WINDOWS
    DWORD thread = GetCurrentThreadId();
#else
    pid_t thread = getpid();
#endif
    void *fiber = Fiber::getThis().get();
    while (_this) {
        for (std::list<LogSink::ptr>::iterator it(_this->m_sinks.begin());
            it != _this->m_sinks.end();
            ++it) {
            (*it)->log(m_name, thread, fiber, level, str, file, line);
        }
        if (!_this->m_inheritSinks)
            break;
        _this = m_parent;
    }
    if (level == Log::FATAL) {
        throw std::runtime_error("Fatal error: " + str);
    }
}

LogEvent::~LogEvent()
{
    m_logger->log(m_level, m_os.str(), m_file, m_line);
}

static const char *levelStrs[] = {
    "FATAL",
    "ERROR",
    "WARN",
    "INFO",
    "TRACE",
    "VERBOSE"
};

std::ostream &operator <<(std::ostream &os, Log::Level level)
{
    ASSERT(level >= Log::FATAL && level <= Log::VERBOSE);
    return os << levelStrs[level];
}
