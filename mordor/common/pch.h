
#include "version.h"

#ifdef WINDOWS
#define _WIN32_WINNT 0x0600
#define WIN32_LEAN_AND_MEAN
#define SECURITY_WIN32
#define _CRT_SECURE_NO_WARNINGS
#define _CRT_NONSTDC_NO_WARNINGS

#define strtoull _strtoui64
#define strnicmp _strnicmp

#else

#define stricmp strcasecmp
#define strnicmp strncasecmp

#endif

#ifdef USE_PCH

// OS Headers
#include <errno.h>
#include <fcntl.h>

#ifdef POSIX
#include <sys/types.h>
#include <execinfo.h>
#include <netdb.h>
#include <netinet/in_systm.h>
#include <netinet/in.h>
#include <netinet/ip.h>
#include <pthread.h>
#include <stddef.h>
#include <sys/fcntl.h>
#include <sys/mman.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <sys/uio.h>

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
#include <security.h>
#include <winerror.h>
#include <winsock2.h>
#include <mswsock.h>
#include <ws2tcpip.h>

#undef min
#undef max
#undef ABSOLUTE
#undef RELATIVE
#undef ERROR

// Take things out of the preprocessor, and put into the global namespace
#undef DELETE
enum {
    DELETE = (0x00010000L)
};

#elif defined(LINUX)
#include <byteswap.h>
#include <semaphore.h>
#include <sys/epoll.h>
#elif defined(OSX)
#include <mach/mach_init.h>
#include <mach/mach_time.h>
#include <mach/task.h>
#include <sys/_endian.h>
#include <sys/sysctl.h>
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
#include <iostream>
#include <list>
#include <map>
#include <set>
#include <sstream>
#include <stdexcept>
#include <string>
#include <typeinfo>
#include <vector>

// Non-STL C++ headers
#include <boost/blank.hpp>
#ifndef GCC
#include <boost/bind.hpp>
#endif
#include <boost/date_time.hpp>
#include <boost/enable_shared_from_this.hpp>
#include <boost/exception.hpp>
#include <boost/function.hpp>
#include <boost/noncopyable.hpp>
#include <boost/regex.hpp>
#include <boost/scoped_ptr.hpp>
#include <boost/shared_array.hpp>
#include <boost/shared_ptr.hpp>
#include <boost/thread.hpp>
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

#endif
