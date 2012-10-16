# SYNOPSIS
#
#     AX_CHECK_LZMA([action-if-found], [action-if-not-found])
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

AC_DEFUN([AX_CHECK_LZMA],

[AC_MSG_CHECKING(if lzma/zx is wanted)
lzma_places="/usr/local /usr /opt/local /sw"
AC_ARG_WITH([lzma],
[  --with-lzma=DIR root directory path of lzma installation [defaults to
                    /usr/local or /usr if not found in /usr/local]
  --without-lzma to disable lzma usage completely],
[if test "$withval" != no ; then
  AC_MSG_RESULT(yes)
  if test -d "$withval"
  then
    lzma_places="$withval $lzma_places"
  else
    AC_MSG_WARN([Sorry, $withval does not exist, checking usual places])
  fi
else
  lzma_places=
  AC_MSG_RESULT(no)
fi],
[AC_MSG_RESULT(yes)])

if test -n "${lzma_places}"
then
    for LZMA in ${lzma_places} ; do
        if test -f "${LZMA_HOME}/include/lzma.h"; then break; fi
        LZMA_HOME=""
    done

    LZMA_SAVED_LDFLAGS=$LDFLAGS
    LZMA_SAVED_CPPFLAGS=$CPPFLAGS
    if test -n "${LZMA_HOME}"; then
        LZMA_CFLAGS="-I${LZMA_HOME}/include"
        LZMA_LDFLAGS="-L${LZMA_HOME}/lib"
        LZMA_LIB="-llzma"
        CPPFLAGS="$CPPFLAGS $LZMA_CFLAGS"
        LDFLAGS="$LDFLAGS $LZMA_LDFLAGS"
    fi
    AC_LANG_SAVE
    AC_LANG_C
    AC_CHECK_LIB(lzma, lzma_end, [lzma_cv_liblzma=yes], [lzma_cv_liblzma=no])
    AC_CHECK_HEADER(lzma.h, [lzma_cv_lzma_h=yes], [lzma_cv_lzma_h=no])
    AC_LANG_RESTORE
    if test "$lzma_cv_liblzma" = "yes" -a "$lzma_cv_lzma_h" = "yes"
    then
        #
        # If both library and header were found, use them
        #
        m4_ifblank([$1],[
                    AC_SUBST(LZMA_CFLAGS)
                    AC_SUBST(LZMA_LDFLAGS)
                    CPPFLAGS="$CPPFLAGS -I${LZMA_HOME}/include"
                    LDFLAGS="$LDFLAGS -L${LZMA_HOME}/lib"
                    AC_DEFINE([HAVE_LZMA], [1],
                              [Define if `lzma' library (-llzma) is available])
                   ],
                   [$1])
    else
        #
        # If either header or library was not found, action-if-not-found
        #
        m4_default([$2],[
            AC_MSG_ERROR([either specify a valid lzma installation with --with-lzma=DIR or disable lzma usage with --without-lzma])
            ])
    fi
    # Restore variables
    LDFLAGS="$LZMA_SAVED_LDFLAGS"
    CPPFLAGS="$LZMA_SAVED_CPPFLAGS"
fi
])

ifdef([m4_ifblank],[],[
m4_define([m4_ifblank],
[m4_if(m4_translit([[$1]],  [ ][	][
]), [], [$2], [$3])])])
