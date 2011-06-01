# SYNOPSIS
#
#    AC_CHECK_VALGRIND
#
# DESCRIPTION
#
#     Test for enable/disable HAVE_VALGRIND macro
#
#   This macro enable following option
#
#      --disable-valgrind
#      --enable-valgrind=<yes|no>
#
#   This macro calls:
#       AC_SUBST(VALGRIND_CPPFLAGS)
#
#   And sets:
#
#     HAVE_VALGRIND

AC_DEFUN([AC_CHECK_VALGRIND],
[
    AC_ARG_ENABLE([valgrind],
        AS_HELP_STRING(
            [--with-valgrind@<:@=ARG@:>@],
            [Compile with HAVE_VALGRIND macro defined (ARG=yes),
            or disable it (ARG=no)
            @<:@ARG=no@:>@ ]
        ),
        [
        if test "$enableval" = "yes"; then
            want_valgrind="yes"
        else
            want_valgrind="no"
        fi
        ],
        [want_valgrind="no"]
    )

    AC_MSG_CHECKING([whether enable HAVE_VALGRIND macro])
    if test x${want_valgrind} = "xyes" ; then
        AC_MSG_RESULT([yes])
        AC_DEFINE(HAVE_VALGRIND,1,[define if compiling for running valgrind ])
        VALGRIND_CPPFLAGS="-DHAVE_VALGRIND"
        AC_SUBST(VALGRIND_CPPFLAGS)
    else
        AC_MSG_RESULT([no])
    fi
])
