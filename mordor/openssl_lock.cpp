#include "openssl_lock.h"

#include <openssl/crypto.h>

#include <boost/bind.hpp>
#include <boost/utility.hpp>

#ifndef USE_FIBER_MUTEX
#include <boost/thread/mutex.hpp>
#else
#include "fibersynchronization.h"
#endif

#include "fiber.h"

namespace {

// Convenient function to store a pointer into integer.
// - Specially handles that if the `T' type is not large enough to
//   hold the pointer
//
// NOTE: On any platform, only one of following two template functions will be
// enabled. And the first one is applicable on most platform.
template <class T>
typename boost::enable_if_c<sizeof(T) >= sizeof(void *), T>::type
pointer2integer(void * p) { return (T) p; }

template <class T>
typename boost::enable_if_c<sizeof(T) < sizeof(void *), T>::type
pointer2integer(void *p)
{
    static const T mask = (T) -1;
    uintptr_t x = (uintptr_t) p;
    return (x & mask) ^ (x >> (sizeof(T) << 3));
}

}

namespace Mordor {

OpensslLockManager &
OpensslLockManager::instance()
{
    static OpensslLockManager s;
    return s;
}

OpensslLockManager::OpensslLockManager()
    : m_locks(CRYPTO_num_locks())
    , m_initialized(false)
{
    for (Locks::iterator it = m_locks.begin(); it != m_locks.end(); ++it)
        (*it).reset(new LockType);
}

OpensslLockManager::~OpensslLockManager()
{
    if (m_initialized)
        uninstallLockCallBacks();
    m_locks.clear();
}

void
OpensslLockManager::installLockCallbacks()
{
    CRYPTO_set_locking_callback(OpensslLockManager::locking_function);
    CRYPTO_set_id_callback(OpensslLockManager::id);
    m_initialized = true;
}

void
OpensslLockManager::uninstallLockCallBacks()
{
    m_initialized = false;
    CRYPTO_set_locking_callback(0);
    CRYPTO_set_id_callback(0);
}

void
OpensslLockManager::locking_function(int mode, int n, const char *file, int line)
{
    instance().locking(mode, n, file, line);
}

void
OpensslLockManager::locking(int mode, int n, const char *file, int line)
{
    if (mode & CRYPTO_LOCK)
        m_locks[n]->lock();
    else
        m_locks[n]->unlock();
}

unsigned long
OpensslLockManager::id()
{
    return pointer2integer<unsigned long>(Mordor::Fiber::getThis().get());
}

}
