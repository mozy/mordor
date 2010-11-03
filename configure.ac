#                                               -*- Autoconf -*-
# Process this file with autoconf to produce a configure script.

AC_PREREQ([2.61])
AC_INIT(mordor, 1.3.0)
AM_INIT_AUTOMAKE
AC_CONFIG_SRCDIR([mordor/fiber.cpp])
AC_CONFIG_HEADERS([config.h])
AC_CONFIG_MACRO_DIR([m4])

# Checks for programs.
AC_PROG_CXX
AC_DISABLE_STATIC
AC_PROG_LIBTOOL
CHECK_RAGEL([mordor/uri.cpp], [6.0])

# Checks for libraries.
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
AC_CHECK_FUNCS([clock_gettime ftruncate memchr memmove memset munmap rmdir socket strchr strstr strtoull])

AC_SUBST([pic_flag])

AC_CONFIG_FILES([Makefile mordor/Makefile mordor/examples/Makefile mordor/test/Makefile mordor/tests/Makefile])

AC_OUTPUT

# Teach libtool about pre-compiled headers
#cat libtool | sed 's/\*\.cpp/*.cpp | *.gch/' > libtool2
#rm libtool
#mv libtool2 libtool
