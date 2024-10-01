dnl ***************************
dnl *** Version information ***
dnl ***************************
XDT_VERSION_INIT([0.8.8])

dnl ***************************
dnl *** Initialize autoconf ***
dnl ***************************
AC_COPYRIGHT([Copyright (c) 2006-2023 Xfce Development Team. All rights reserved.])
AC_INIT([xfce4-dict], [xdt_version], [https://gitlab.xfce.org/apps/xfce4-dict], [xfce4-dict])
AC_PREREQ([2.69])
AC_REVISION([xdt_version_build])
AC_CONFIG_MACRO_DIRS([m4])

dnl ***************************
dnl *** Initialize automake ***
dnl ***************************
AM_INIT_AUTOMAKE([1.11 dist-bzip2 tar-ustar no-dist-gzip])
AC_CONFIG_HEADERS([config.h])
AM_MAINTAINER_MODE()
m4_ifdef([AM_SILENT_RULES], [AM_SILENT_RULES([yes])])

dnl ********************************
dnl *** Check for basic programs ***
dnl ********************************
AC_PROG_CC()
AM_PROG_CC_C_O
LT_PATH_LD()
AC_PROG_INSTALL()

dnl ***************************
dnl *** Disable static libs ***
dnl ***************************
LT_PREREQ([2.4])
LT_INIT([disable-static])

dnl **********************************
dnl *** Check for standard headers ***
dnl **********************************
AC_SEARCH_LIBS(floor, m)

dnl ******************************
dnl *** Check for i18n support ***
dnl ******************************
GETTEXT_PACKAGE="$PACKAGE"
AC_DEFINE_UNQUOTED([GETTEXT_PACKAGE], ["$GETTEXT_PACKAGE"], [Name of default gettext domain])
AC_SUBST([GETTEXT_PACKAGE])

AM_GNU_GETTEXT([external])
AM_GNU_GETTEXT_VERSION([0.19.8])

dnl ***********************************
dnl *** Check for required packages ***
dnl ***********************************
XDT_CHECK_PACKAGE([GTHREAD], [gthread-2.0], [2.24.0])
XDT_CHECK_PACKAGE([GTK], [gtk+-3.0], [3.22.0])
XDT_CHECK_PACKAGE([LIBXFCE4UI], [libxfce4ui-2], [4.12.0])
XDT_CHECK_PACKAGE([LIBXFCE4UTIL], [libxfce4util-1.0], [4.10.0])
XDT_CHECK_PACKAGE([LIBXFCE4PANEL], [libxfce4panel-2.0], [4.10.0])
XDT_CHECK_PACKAGE([X11], [x11])

dnl ***********************************
dnl *** Check for gdbus-codegen     ***
dnl ***********************************
AC_CHECK_PROG([GDBUS_CODEGEN],[gdbus-codegen],[gdbus-codegen])

dnl ***********************************
dnl *** Check for debugging support ***
dnl ***********************************
XDT_FEATURE_DEBUG([xdt_debug_default])


AC_CONFIG_FILES([
	Makefile
	lib/Makefile
	panel-plugin/Makefile
	src/Makefile
	po/Makefile.in
	icons/Makefile
	icons/16x16/Makefile
	icons/24x24/Makefile
	icons/32x32/Makefile
	icons/48x48/Makefile
	icons/64x64/Makefile
	icons/96x96/Makefile
	icons/128x128/Makefile
	icons/scalable/Makefile
])
AC_OUTPUT
