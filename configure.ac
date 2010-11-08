#                                               -*- Autoconf -*-
# Process this file with autoconf to produce a configure script.

AC_PREREQ(2.61)
AC_INIT([libmordor],[1.3.0])
AM_INIT_AUTOMAKE
LT_INIT([disable-static])
AC_CONFIG_SRCDIR([mordor/fiber.cpp])
AC_CONFIG_HEADERS([config.h])
AC_CONFIG_MACRO_DIR([m4])

AC_SUBST([LIBMORDOR_SO_VERSION], [1:0:0])
AC_SUBST([LIBMORDOR_API_VERSION], [1])

# Checks for programs.
AC_PROG_CXX
AC_PROG_AWK
AC_PROG_CC
AC_PROG_CPP
AC_PROG_INSTALL
AC_PROG_LN_S
AC_PROG_MAKE_SET
CHECK_RAGEL([mordor/uri.cpp], [6.0])

# Checks for libraries.
AC_SEARCH_LIBS([pthread_create], [pthread])
AC_SEARCH_LIBS([clock_gettime], [rt])
AC_SEARCH_LIBS([backtrace], [execinfo])
AX_CHECK_OPENSSL
AX_CHECK_ZLIB
AX_BOOST_BASE([1.40])
AX_BOOST_PROGRAM_OPTIONS
AX_BOOST_REGEX
AX_BOOST_THREAD
AX_CHECK_CORESERVICES_FRAMEWORK
AX_CHECK_COREFOUNDATION_FRAMEWORK
AX_CHECK_SECURITY_FRAMEWORK
AX_CHECK_SYSTEMCONFIGURATION_FRAMEWORK

# Checks for header files.
AC_CHECK_HEADERS([fcntl.h netdb.h netinet/in.h stddef.h stdint.h stdlib.h string.h sys/socket.h sys/time.h syslog.h])

# Checks for typedefs, structures, and compiler characteristics.
AC_HEADER_STDBOOL
AC_C_INLINE
AC_TYPE_INT32_T
AC_TYPE_INT64_T
AC_TYPE_PID_T
AC_TYPE_SIZE_T
AC_TYPE_UINT16_T
AC_TYPE_UINT32_T
AC_TYPE_UINT64_T
AC_CHECK_TYPES([ptrdiff_t])

# Checks for library functions.
AC_FUNC_ERROR_AT_LINE
AC_HEADER_MAJOR
AC_FUNC_MMAP
AC_FUNC_STRTOD
AC_CHECK_FUNCS([clock_gettime ftruncate memchr memmove memset munmap rmdir socket strchr strstr strtoull])

AC_SUBST([pic_flag])

AC_CONFIG_FILES([Makefile
                 mordor/libmordor.pc
                 mordor/test/libmordortest.pc])
AC_OUTPUT
