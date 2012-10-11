AC_DEFUN([AX_CHECK_LZMA],

[AC_MSG_CHECKING(if lzma/zx is wanted)
AC_ARG_WITH(lzma,
[  --with-lzma=DIR root directory path of lzma installation [defaults to
                    /usr/local or /usr if not found in /usr/local]
  --without-lzma to disable lzma usage completely],
[if test "$withval" != no ; then
  AC_MSG_RESULT(yes)
  if test -d "$withval"
  then
    LZMA_HOME="$withval"
  else
    AC_MSG_WARN([Sorry, $withval does not exist, checking usual places])
  fi
else
  AC_MSG_RESULT(no)
fi],
[AC_MSG_RESULT(yes)])

ZLIB_HOME=/usr/local
if test ! -f "${LZMA_HOME}/include/lzma.h"
then
        LZMA_HOME=/usr
fi

# locate lzma
if test -n "${LZMA_HOME}"
then
        LZMA_OLD_LDFLAGS=$LDFLAGS
        LZMA_OLD_CPPFLAGS=$LDFLAGS
        LDFLAGS="$LDFLAGS -L${LZMA_HOME}/lib"
        CPPFLAGS="$CPPFLAGS -I${LZMA_HOME}/include"
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
                AC_CHECK_LIB(lzma, lzma_end)
                AC_MSG_CHECKING(lzma in ${LZMA_HOME})
                AC_MSG_RESULT(ok)
        else
                #
                # If either header or library was not found, revert and bomb
                #
                AC_MSG_CHECKING(lzma in ${LZMA_HOME})
                LDFLAGS="$LZMA_OLD_LDFLAGS"
                CPPFLAGS="$LZMA_OLD_CPPFLAGS"
                AC_MSG_RESULT(failed)
                AC_MSG_ERROR(either specify a valid lzma installation with --with-lzma=DIR or disable lzma usage with --without-lzma)
        fi
fi

])
