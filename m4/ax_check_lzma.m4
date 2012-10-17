# SYNOPSIS
#
#     AX_CHECK_LZMA
#
# DESCRIPTION
#
#     This macro searches for an installed lzma2/zx library.
#
#     This macro calls:
#
#       AC_SUBST(LZMA_CFLAGS)
#       AC_SUBST(LZMA_LDFLAGS)
#       AC_SUBST(LZMA_LIB)
#       AC_DEFINE(HAVE_LIBLZMA)
#       AM_CONDITIONAL(HAVE_LZMA)
#
#     if the lzma development files are found

AC_DEFUN([AX_CHECK_LZMA],
[
  AC_MSG_CHECKING(if lzma/zx is wanted)
  lzma_places="/usr/local /usr /opt/local /sw"
  AC_ARG_WITH([lzma],
    [  --with-lzma=DIR         root directory path of lzma installation [defaults to
                          /usr/local or /usr if not found in /usr/local]
  --without-lzma          to disable lzma usage completely],
    [if test "$withval" != no ; then
      want_lzma="yes"
        if test -d "$withval"; then
          lzma_places="$withval $lzma_places"
        fi
     else
       want_lzma="no"
     fi],
    [want_lzma="check"])

if test "$want_lzma" != "no"; then
    for LZMA_HOME in ${lzma_places} ; do
        if test -f "${LZMA_HOME}/include/lzma.h"; then break; fi
        LZMA_HOME=""
    done

    LZMA_SAVED_LDFLAGS=$LDFLAGS
    LZMA_SAVED_CPPFLAGS=$CPPFLAGS
    LZMA_SAVED_LIBS=$LIBS
    if test -n "${LZMA_HOME}"; then
        LZMA_CFLAGS="-I${LZMA_HOME}/include"
        LZMA_LDFLAGS="-L${LZMA_HOME}/lib"
        LZMA_LIB="-llzma"
        CPPFLAGS="$CPPFLAGS $LZMA_CFLAGS"
        LDFLAGS="$LDFLAGS $LZMA_LDFLAGS"
        LIBS="$LIBS $LZMA_LIB"
    fi
    AC_CACHE_CHECK([whether lzma library is available],
                   ax_cv_lzma,
                   [AC_LANG_PUSH([C])
                    AC_COMPILE_IFELSE([AC_LANG_PROGRAM(
                                        [[@%:@include <lzma.h>]],
                                        [[lzma_end(NULL);]])],
                                      ax_cv_lzma=yes,
                                      ax_cv_lzma=no)
                    AC_LANG_POP([C])])
    if test "$ax_cv_lzma" = "yes"; then
        # If both library and header were found, use them
        AC_SUBST(LZMA_CFLAGS)
        AC_SUBST(LZMA_LDFLAGS)
        AC_SUBST(LZMA_LIB)
        AC_DEFINE([HAVE_LZMA], [1],
                  [Define if `lzma' library (-llzma) is available])
    elif test "$want_lzma" = "yes"; then
        AC_MSG_ERROR([either specify a valid lzma installation with --with-lzma=DIR or disable lzma usage with --without-lzma])
    fi
    # Restore variables
    LDFLAGS="$LZMA_SAVED_LDFLAGS"
    CPPFLAGS="$LZMA_SAVED_CPPFLAGS"
    LIBS="$LZMA_SAVED_LIBS"
fi
AM_CONDITIONAL(HAVE_LZMA, test "x[$]ax_cv_lzma" = xyes)
])
