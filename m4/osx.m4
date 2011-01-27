dnl Defines COREFOUNDATION_FRAMEWORK_LIBS.
AC_DEFUN([AX_CHECK_COREFOUNDATION_FRAMEWORK],
[
  AC_CACHE_CHECK([for CFRetain],
    gt_cv_func_CFRetain,
    [gt_save_LIBS="$LIBS"
     LIBS="$LIBS -Wl,-framework -Wl,CoreFoundation"
     AC_TRY_LINK([#include <CoreFoundation/CFBase.h>],
       [CFRetain(NULL)],
       [gt_cv_func_CFRetain=yes],
       [gt_cv_func_CFRetain=no])
     LIBS="$gt_save_LIBS"])
  if test $gt_cv_func_CFRetain = yes; then
    AC_DEFINE([HAVE_COREFOUNDATION_FRAMEWORK], 1,
      [Define to 1 if you have the MacOS X CoreFoundation framework.])
  fi
  COREFOUNDATION_FRAMEWORK_LIBS=
  if test $gt_cv_func_CFRetain = yes; then
    COREFOUNDATION_FRAMEWORK_LIBS="-Wl,-framework -Wl,CoreFoundation"
  fi
  AC_SUBST([COREFOUNDATION_FRAMEWORK_LIBS])
])

dnl Defines SYSTEMCONFIGURATION_FRAMEWORK_LIBS.
AC_DEFUN([AX_CHECK_SYSTEMCONFIGURATION_FRAMEWORK],
[
  AC_CACHE_CHECK([for SCDynamicStoreCreate],
    gt_cv_func_SCDynamicStoreCreate,
    [gt_save_LIBS="$LIBS"
     LIBS="$LIBS -Wl,-framework -Wl,SystemConfiguration"
     AC_TRY_LINK([#include <SystemConfiguration/SystemConfiguration.h>],
       [SCDynamicStoreCreate(NULL, NULL, NULL, NULL)],
       [gt_cv_func_SCDynamicStoreCreate=yes],
       [gt_cv_func_SCDynamicStoreCreate=no])
     LIBS="$gt_save_LIBS"])
  if test $gt_cv_func_SCDynamicStoreCreate = yes; then
    AC_DEFINE([HAVE_SYSTEMCONFIGURATION_FRAMEWORK], 1,
      [Define to 1 if you have the MacOS X SystemConfiguration framework.])
  fi
  SYSTEMCONFIGURATION_FRAMEWORK_LIBS=
  if test $gt_cv_func_SCDynamicStoreCreate = yes; then
    SYSTEMCONFIGURATION_FRAMEWORK_LIBS="-Wl,-framework -Wl,SystemConfiguration"
  fi
  AC_SUBST([SYSTEMCONFIGURATION_FRAMEWORK_LIBS])
])

dnl Defines CORESERVICES_FRAMEWORK_LIBS.
AC_DEFUN([AX_CHECK_CORESERVICES_FRAMEWORK],
[
  AC_CACHE_CHECK([for CFNetworkCopyProxiesForURL],
    gt_cv_func_CFNetworkCopyProxiesForURL,
    [gt_save_LIBS="$LIBS"
     LIBS="$LIBS -Wl,-framework -Wl,CoreServices"
     AC_TRY_LINK([#include <CoreServices/CoreServices.h>],
       [CFNetworkCopyProxiesForURL(NULL, NULL)],
       [gt_cv_func_CFNetworkCopyProxiesForURL=yes],
       [gt_cv_func_CFNetworkCopyProxiesForURL=no])
     LIBS="$gt_save_LIBS"])
  if test $gt_cv_func_CFNetworkCopyProxiesForURL = yes; then
    AC_DEFINE([HAVE_CORESERVICES_FRAMEWORK], 1,
      [Define to 1 if you have the MacOS X CoreServices framework.])
  fi
  CORESERVICES_FRAMEWORK_LIBS=
  if test $gt_cv_func_CFNetworkCopyProxiesForURL = yes; then
    CORESERVICES_FRAMEWORK_LIBS="-Wl,-framework -Wl,CoreServices"
  fi
  AC_SUBST([CORESERVICES_FRAMEWORK_LIBS])
])

dnl Defines SECURITY_FRAMEWORK_LIBS.
AC_DEFUN([AX_CHECK_SECURITY_FRAMEWORK],
[
  AC_CACHE_CHECK([for SecKeychainItemFreeContent],
    gt_cv_func_SecKeychainItemFreeContent,
    [gt_save_LIBS="$LIBS"
     LIBS="$LIBS -Wl,-framework -Wl,Security"
     AC_TRY_LINK([#include <Security/SecKeychain.h>],
       [SecKeychainItemFreeContent(NULL, NULL)],
       [gt_cv_func_SecKeychainItemFreeContent=yes],
       [gt_cv_func_SecKeychainItemFreeContent=no])
     LIBS="$gt_save_LIBS"])
  if test $gt_cv_func_SecKeychainItemFreeContent = yes; then
    AC_DEFINE([HAVE_SECURITY_FRAMEWORK], 1,
      [Define to 1 if you have the MacOS X Security framework.])
  fi
  SECURITY_FRAMEWORK_LIBS=
  if test $gt_cv_func_SecKeychainItemFreeContent = yes; then
    SECURITY_FRAMEWORK_LIBS="-Wl,-framework -Wl,Security"
  fi
  AC_SUBST([SECURITY_FRAMEWORK_LIBS])
])

