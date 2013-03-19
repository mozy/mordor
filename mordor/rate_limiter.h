#ifndef __MORDOR_RATE_LIMITER_H__
#define __MODOR_RATE_LIMITER_H__
// Copyright (c) 2013 - Cody Cutrer

#include <list>
#include <map>

#include <boost/thread/mutex.hpp>

#include "assert.h"
#include "config.h"
#include "log.h"
#include "timer.h"

namespace Mordor {

template <class T>
class RateLimiter
{
private:
    struct Bucket
    {
        Bucket()
            : m_count(0u)
        {}

        std::list<unsigned long long> m_timestamps;
        size_t m_count;
        Timer::ptr m_timer;
    };

public:
    RateLimiter(TimerManager &timerManager, ConfigVar<size_t>::ptr countLimit,
        ConfigVar<unsigned long long>::ptr timeLimit)
        : m_timerManager(timerManager),
          m_countLimit(countLimit),
          m_timeLimit(timeLimit)
    {}

    bool allowed(const T &key)
    {
        boost::mutex::scoped_lock lock(m_mutex);
        unsigned long long now = m_timerManager.now();
        Bucket &bucket = m_buckets[key];
        size_t countLimit = m_countLimit->val();
        trim(bucket, now, countLimit);
        if (bucket.m_count >= countLimit) {
            startTimer(key, bucket);
            return false;
        }
        bucket.m_timestamps.push_back(now);
        ++bucket.m_count;
        startTimer(key, bucket);
        MORDOR_ASSERT(bucket.m_count == bucket.m_timestamps.size());
        return true;
    }

    void reset(const T &key)
    {
        boost::mutex::scoped_lock lock(m_mutex);
        std::map<T, Bucket>::iterator it = m_buckets.find(key);
        if (it != m_buckets.end()) {
            if (it->second.m_timer)
                it->second.m_timer->cancel();
            m_buckets.erase(key);
        }
    }

private:
    void trimKey(const T& key)
    {
        boost::mutex::scoped_lock lock(m_mutex);
        Bucket &bucket = m_buckets[key];
        trim(bucket, m_timerManager.now());
        startTimer(key, bucket, m_countLimit->val());
    }

    void trim(Bucket &bucket, unsigned long long now, size_t countLimit)
    {
        unsigned long long timeLimit = m_timeLimit->val();
        MORDOR_ASSERT(bucket.m_count == bucket.m_timestamps.size());
        while(!bucket.m_timestamps.empty() && (bucket.m_timestamps.front() < now - timeLimit || bucket.m_count > countLimit))
        {
            drop(bucket);
        }
        MORDOR_ASSERT(bucket.m_count == bucket.m_timestamps.size());
    }

    void drop(Bucket &bucket)
    {
        bucket.m_timestamps.pop_front();
        --bucket.m_count;
        if (bucket.m_timer) {
            bucket.m_timer->cancel();
            bucket.m_timer.reset();
        }
    }

    void startTimer(const T &key, Bucket &bucket)
    {
        // If there are still timestamps, set a timer to clear it
        if(!bucket.m_timestamps.empty()) {
            if (!bucket.m_timer) {
                //bucket.m_timer = m_timerManager.registerTimer(bucket.m_timestamps.front() + timeLimit - now,
                //    boost::bind(&RateLimiter::trimKey, this, key));
            }
        } else {
            m_buckets.erase(key);
        }
    }
private:
    TimerManager &m_timerManager;
    ConfigVar<size_t>::ptr m_countLimit;
    ConfigVar<unsigned long long>::ptr m_timeLimit;
    std::map<T, Bucket> m_buckets;
    boost::mutex m_mutex;
};

}

#endif
