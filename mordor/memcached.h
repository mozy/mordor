#ifndef __MORDOR_MEMCACHED_CLIENT_H__
#define __MORDOR_MEMCACHED_CLIENT_H__
// Copyright (c) 2010 - Mozy, Inc.

#include <string>

#include <boost/date_time/posix_time/posix_time.hpp>
#include <boost/function.hpp>
#include <boost/noncopyable.hpp>

#include "date_time.h"
#include "exception.h"
#include "mordor/streams/stream.h"
#include "string.h"

namespace Mordor {

struct MemcachedError : virtual Exception
{
    MemcachedError() {}
    MemcachedError(const std::string &message)
        : m_message(message)
    {}
    virtual ~MemcachedError() throw() {}

    const char *what() const throw() { return m_message.c_str(); }

private:
    std::string m_message;
};
struct MemcachedClientError : virtual MemcachedError
{
    MemcachedClientError(const std::string &message)
        : MemcachedError(message)
    {}
};
struct MemcachedServerError : virtual MemcachedError
{
    MemcachedServerError(const std::string &message)
        : MemcachedError(message)
    {}
};

template <class T>
class MemcachedClient : boost::noncopyable
{
public:
    struct Value
    {
        int flags;
        std::string value;
        unsigned long long casUnique;
    };

public:
    MemcachedClient(
        boost::function<T (const std::string &key)> nodeForKeyDg,
        boost::function<Stream::ptr (const T& node)> streamForNodeDg)
        : m_nodeForKeyDg(nodeForKeyDg),
          m_streamForNodeDg(streamForNodeDg)
    {}

    bool get(const std::string &key, Value &result)
    { return connectionForKey(key)->command("get", key, 0, 0, ~0ull, NULL, &result) == "VALUE"; }
    bool gets(const std::string &key, Value &result)
    { return connectionForKey(key)->command("gets", key, 0, 0, ~0ull, NULL, &result) == "VALUE"; }

    void set(const std::string &key, int flags, const std::string &value,
        boost::posix_time::ptime expiration = boost::posix_time::ptime())
    { connectionForKey(key)->command("set", key, flags,
        expirationToInt(expiration), ~0ull, &value, NULL); }
    void set(const std::string &key, int flags, const std::string &value,
        boost::posix_time::time_duration expiration)
    { connectionForKey(key)->command("set", key, flags,
        expirationToInt(expiration), ~0ull, &value, NULL); }
    bool add(const std::string &key, int flags, const std::string &value,
        boost::posix_time::ptime expiration = boost::posix_time::ptime())
    { return connectionForKey(key)->command("add", key, flags,
        expirationToInt(expiration), ~0ull, &value, NULL) == "STORED"; }
    bool add(const std::string &key, int flags, const std::string &value,
        boost::posix_time::time_duration expiration)
    { return connectionForKey(key)->command("add", key, flags,
        expirationToInt(expiration), ~0ull, &value, NULL) == "STORED"; }
    bool replace(const std::string &key, int flags, const std::string &value,
        boost::posix_time::ptime expiration = boost::posix_time::ptime())
    { return connectionForKey(key)->command("replace", key, flags,
        expirationToInt(expiration), ~0ull, &value, NULL) == "STORED"; }
    bool replace(const std::string &key, int flags, const std::string &value,
        boost::posix_time::time_duration expiration)
    { return connectionForKey(key)->command("replace", key, flags,
        expirationToInt(expiration), ~0ull, &value, NULL) == "STORED"; }
    bool append(const std::string &key, int flags, const std::string &value,
        boost::posix_time::ptime expiration = boost::posix_time::ptime())
    { return connectionForKey(key)->command("append", key, flags,
        expirationToInt(expiration), ~0ull, &value, NULL) == "STORED"; }
    bool append(const std::string &key, int flags, const std::string &value,
        boost::posix_time::time_duration expiration)
    { return connectionForKey(key)->command("append", key, flags,
        expirationToInt(expiration), ~0ull, &value, NULL) == "STORED"; }
    bool prepend(const std::string &key, int flags, const std::string &value,
        boost::posix_time::ptime expiration = boost::posix_time::ptime())
    { return connectionForKey(key)->command("prepend", key, flags,
        expirationToInt(expiration), ~0ull, &value, NULL) == "STORED"; }
    bool prepend(const std::string &key, int flags, const std::string &value,
        boost::posix_time::time_duration expiration)
    { return connectionForKey(key)->command("prepend", key, flags,
        expirationToInt(expiration), ~0ull, &value, NULL) == "STORED"; }
    // TODO: distinguish between modified and not found
    bool cas(const std::string &key, int flags, const std::string &value,
        unsigned long long casUnique,
        boost::posix_time::ptime expiration = boost::posix_time::ptime())
    { return connectionForKey(key)->command("cas", key, flags,
        expirationToInt(expiration), casUnique, &value, NULL) == "STORED"; }
    bool cas(const std::string &key, int flags, const std::string &value,
        unsigned long long casUnique,
        boost::posix_time::time_duration expiration)
    { return connectionForKey(key)->command("cas", key, flags,
        expirationToInt(expiration), casUnique, &value, NULL) == "STORED"; }

    bool _delete(const std::string &key)
    { return connectionForKey(key)->command("delete", key, 0, 0, ~0ull, NULL, NULL) == "STORED"; }

private:
    class Connection
    {
    public:
        Connection(MemcachedClient *parent, const T &node, Stream::ptr stream)
            : m_parent(parent),
              m_stream(stream),
              m_node(node)
        {}

        std::string command(const char *command, const std::string &key,
            int flags, int expiration, unsigned long long casUnique,
            const std::string *value, Value *result)
        {
            MORDOR_ASSERT(!(value && result));
            std::ostringstream os;
            os << command << ' ' << key;
            if (value) {
                os << ' ' << flags << ' ' << expiration << ' '
                    << value->size();
                if (casUnique != ~0ull)
                    os << ' ' << casUnique;
            }
            os << "\r\n";
            std::string commandStr = os.str();

            FiberMutex::ScopedLock sendLock(m_sendMutex);
            try {
                m_stream->write(commandStr.c_str(), commandStr.size());
                if (value) {
                    m_stream->write(value->c_str(), value->size());
                    m_stream->write("\r\n", 2);
                }
                // TODO: only flush if no one else in line to get mutex
                m_stream->flush();
            } catch (...) {
                FiberMutex::ScopedLock lock(m_parent->m_mutex);
                typename std::map<T, boost::shared_ptr<Connection> >::iterator it
                    = m_parent->m_connections.find(m_node);
                if (it != m_parent->m_connections.end())
                    m_parent->m_connections.erase(it);
                throw;
            }

            // TODO: atomically swap these
            sendLock.unlock();
            FiberMutex::ScopedLock receiveLock(m_receiveMutex);

            try {
                std::string response = m_stream->getDelimited("\r\n");
                response.resize(response.size() - 2);
                if (response.size() >= 5 && strncmp(response.c_str(), "ERROR", 5) == 0)
                    MORDOR_THROW_EXCEPTION(MemcachedError(response.substr(6)));
                else if (response.size() >= 12 && strncmp(response.c_str(), "CLIENT_ERROR", 12) == 0)
                    MORDOR_THROW_EXCEPTION(MemcachedClientError(response.substr(13)));
                else if (response.size() >= 12 && strncmp(response.c_str(), "SERVER_ERROR", 12) == 0)
                    MORDOR_THROW_EXCEPTION(MemcachedServerError(response.substr(13)));
                if (result) {
                    if (response.size() >= 5 && strncmp(response.c_str(), "VALUE", 5) == 0) {
                        std::vector<std::string> fields = split(response.substr(6), ' ');
                        if (fields.size() != 3 && fields.size() != 4)
                            MORDOR_THROW_EXCEPTION(MemcachedError("Invalid VALUE"));
                        if (fields[0] != key)
                            MORDOR_THROW_EXCEPTION(MemcachedError("Returned VALUE doesn't match key"));
                        result->flags = boost::lexical_cast<int>(fields[1]);
                        size_t bytes = boost::lexical_cast<size_t>(fields[2]);
                        if (fields.size() == 4)
                            result->casUnique = boost::lexical_cast<unsigned long long>(fields[3]);
                        else
                            result->casUnique = ~0ull;
                        result->value.resize(bytes);
                        char *next = &result->value[0];
                        while (bytes) {
                            size_t read = m_stream->read(next, bytes);
                            bytes -= read;
                            next += read;
                        }
                        response = m_stream->getDelimited("\r\n");
                        if (response != "\r\n")
                            MORDOR_THROW_EXCEPTION(MemcachedError("Malformed VALUE"));
                        response = m_stream->getDelimited("\r\n");
                        if (response != "END\r\n")
                            MORDOR_THROW_EXCEPTION(MemcachedError("Missing END"));
                        return "VALUE";
                    }
                }
                return response;
            } catch (MemcachedClientError &) {
                throw;
            } catch (...) {
                FiberMutex::ScopedLock lock(m_parent->m_mutex);
                typename std::map<T, boost::shared_ptr<Connection> >::iterator it
                    = m_parent->m_connections.find(m_node);
                if (it != m_parent->m_connections.end())
                    m_parent->m_connections.erase(it);
                throw;
            }
        }

    private:
        MemcachedClient *m_parent;
        Stream::ptr m_stream;
        T m_node;
        FiberMutex m_sendMutex, m_receiveMutex;
    };

private:
    boost::shared_ptr<Connection>
        connectionForKey(const std::string &key)
    {
        FiberMutex::ScopedLock lock(m_mutex);
        T node = m_nodeForKeyDg(key);
        typename std::map<T, boost::shared_ptr<Connection> >::iterator it
            = m_connections.find(node);
        if (it == m_connections.end()) {
            Stream::ptr stream = m_streamForNodeDg(node);
            MORDOR_ASSERT(stream->supportsRead());
            MORDOR_ASSERT(stream->supportsWrite());
            MORDOR_ASSERT(stream->supportsFind());
            boost::shared_ptr<Connection> result(new Connection(this, node, stream));
            return m_connections[node] = result;
        }
        return it->second;
    }

    int expirationToInt(boost::posix_time::time_duration expiration)
    {
        MORDOR_ASSERT(expiration.total_seconds() > 0);
        MORDOR_ASSERT(expiration.total_seconds() <= 60 * 60 * 24 * 30);
        return expiration.total_seconds();
    }

    int expirationToInt(boost::posix_time::ptime expiration)
    {
        if (expiration.is_not_a_date_time())
            return 0;
        int result = (int)toTimeT(expiration);
        MORDOR_ASSERT(result > 60 * 60 * 24 * 30);
        return result;
    }


private:
    boost::function<T (const std::string &key)> m_nodeForKeyDg;
    boost::function<Stream::ptr (const T& node)> m_streamForNodeDg;
    std::map<T, boost::shared_ptr<Connection> > m_connections;
    FiberMutex m_mutex;
};


/// \tparam Key The result of the Hash function
/// \tparam Value The Value to return
/// \tparam Hash Object that will return Key.  It needs to have overloads for
/// (InputIterator::value_type node, size_t replicas) for the insert() method,
/// and (T) for the lookup() method
template <class Key, class Value, class Hash>
class ConsistentHash
{
public:
    /// Remove all nodes from the circle
    void clear()
    {
        m_circle.clear();
    }

    /// Add nodes to the circle
    template <class InputIterator>
    void insert(InputIterator start, InputIterator end,
        size_t replicas = 1)
    {
        while (start != end) {
            for (size_t i = 0; i < replicas; ++i) {
                Key hash = m_hasher(*start, i);
                m_circle[hash] = *start;
            }
            ++start;
        }
    }

    /// Lookup nearest item
    template <class T>
    Value lookup(const T& key) const
    {
        MORDOR_ASSERT(!m_circle.empty());
        Key hash = m_hasher(key);
        typename std::map<Key, Value>::const_iterator it = m_circle.lower_bound(hash);
        if (it == m_circle.end())
            it = m_circle.begin();
        return it->second;
    }

    template <class T>
    Value operator()(const T& key) const
    { return lookup(key); }

private:
    std::map<Key, Value> m_circle;
    Hash m_hasher;
};

}

#endif
