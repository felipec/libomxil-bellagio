dnl AX_SET_PLUGINDIR

dnl AC_DEFINE PLUGINDIR to the full location where components will be installed
dnl AC_SUBST plugindir, to be used in Makefile.am's

AC_DEFUN([AX_SET_PLUGINDIR],
[
  dnl define location of plugin directory
  AS_AC_EXPAND(PLUGINDIR, ${libdir}/bellagio)
  AC_DEFINE_UNQUOTED(PLUGINDIR, "$PLUGINDIR",
    [directory where plugins are located])
  AC_MSG_NOTICE([Using $PLUGINDIR as the components install location])

  dnl plugin directory configure-time variable
  AC_SUBST([plugindir], '[${libdir}/bellagio]')
])
