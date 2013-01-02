#ifndef __MORDOR_OPENSSL_LOCK_H__
#define __MORDOR_OPENSSL_LOCK_H__

#include <vector>

#include <boost/noncopyable.hpp>
#include <boost/shared_ptr.hpp>

#ifndef USE_FIBER_MUTEX
namespace boost {
    class mutex;
}
#else
namespace Mordor {
    class FiberMutex;
}
#endif

namespace Mordor {

class OpensslLockManager : public boost::noncopyable
{
public:
#ifndef USE_FIBER_MUTEX
    typedef boost::mutex LockType;
#else
    typedef Mordor::FiberMutex LockType;
#endif

    static OpensslLockManager & instance();
    static unsigned long id();
    static void locking_function(int mode, int n, const char *file, int line);

    void installLockCallbacks();
    void uninstallLockCallBacks();

protected:
    typedef std::vector<boost::shared_ptr<LockType> > Locks;

    OpensslLockManager();
    ~OpensslLockManager();

    void locking(int mode, int n, const char * file, int line);

protected:
    std::vector<boost::shared_ptr<LockType> > m_locks;
    bool m_initialized;
};

}

#endif
