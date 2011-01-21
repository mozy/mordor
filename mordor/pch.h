#ifndef __MORDOR_PCH_H__
#define __MORDOR_PCH_H__

#include "version.h"

#include "predef.h"

// OS Headers
#include <errno.h>
#include <fcntl.h>

#ifdef POSIX
#include <sys/types.h>
#include <execinfo.h>
#include <netdb.h>
#include <netinet/in.h>
#include <pthread.h>
#include <setjmp.h>
#include <signal.h>
#include <stddef.h>
#include <sys/fcntl.h>
#include <sys/mman.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <sys/uio.h>
#include <sys/un.h>
#ifndef OSX
#include <netinet/ip.h>
#include <netinet/in_systm.h>
#include <ucontext.h>
#endif

#undef major
#undef minor
#endif

#ifdef BSD
#include <sys/event.h>
#endif

#ifdef WINDOWS
#include <windows.h>

#include <direct.h>
#include <lm.h>
#include <lmcons.h>
#include <ntstatus.h>
#include <security.h>
#include <winerror.h>
#include <winhttp.h>
#include <winsock2.h>
#include <mswsock.h>
#include <ws2tcpip.h>
#elif defined(LINUX)
#include <byteswap.h>
#include <semaphore.h>
#include <sys/epoll.h>
#include <syscall.h>
#elif defined(OSX)
#include <libkern/OSAtomic.h>
#include <mach/mach_init.h>
#include <mach/mach_time.h>
#include <mach/task.h>
#include <sys/_endian.h>
#include <sys/sysctl.h>
#include <CoreFoundation/CFString.h>
#elif defined(FREEBSD)
#include <sys/endian.h>
#include <sys/sem.h>
#endif

// C Headers
#include <stdlib.h>
#include <string.h>
#include <time.h>

// C++ Headers
#include <algorithm>
#include <bitset>
#include <iostream>
#include <limits>
#include <list>
#include <map>
#include <set>
#include <sstream>
#include <stack>
#include <stdexcept>
#include <string>
#include <typeinfo>
#include <vector>

// Non-STL C++ headers
#if !defined(GCC) || defined(__clang__)
#include <boost/bind.hpp>
#endif
#include <boost/enable_shared_from_this.hpp>
#include <boost/noncopyable.hpp>
#include <boost/scoped_ptr.hpp>
#include <boost/shared_array.hpp>
#include <boost/shared_ptr.hpp>
#include <boost/blank.hpp>
#include <boost/date_time.hpp>
#include <boost/exception/all.hpp>
#include <boost/function.hpp>
#include <boost/lexical_cast.hpp>
#include <boost/multi_index_container.hpp>
#include <boost/multi_index/ordered_index.hpp>
#include <boost/multi_index/global_fun.hpp>
#include <boost/multi_index/member.hpp>
#include <boost/regex.hpp>
#include <boost/signals2/signal.hpp>
#include <boost/thread/mutex.hpp>
#include <boost/thread/tss.hpp>
#include <boost/variant.hpp>

// Non-CRT C headers
#include <openssl/aes.h>
#include <openssl/bio.h>
#include <openssl/blowfish.h>
#include <openssl/err.h>
#include <openssl/md5.h>
#include <openssl/sha.h>
#include <openssl/ssl.h>
#include <zlib.h>

#if defined(VALGRIND) && (defined(LINUX) || defined (OSX))
#include <valgrind/valgrind.h>
#endif

#endif
