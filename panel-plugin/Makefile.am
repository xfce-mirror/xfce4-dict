
plugindir = $(libdir)/xfce4/panel/plugins
plugin_LTLIBRARIES = libxfce4dict.la


libxfce4dict_la_SOURCES =						\
	xfce4-dict-plugin.c

libxfce4dict_la_CFLAGS =						\
	-I$(top_srcdir)								\
	-I$(top_srcdir)/lib							\
	$(LIBXFCEGUI4_CFLAGS)						\
	$(LIBXFCE4PANEL_CFLAGS)						\
	-DG_LOG_DOMAIN=\"xfce4-dict-plugin\"		\
	-DPACKAGE_LOCALE_DIR=\"$(localedir)\"		\
	@GTHREAD_CFLAGS@

libxfce4dict_la_LIBADD =						\
	$(LIBXFCE4PANEL_LIBS)						\
	$(LIBXFCEGUI4_LIBS)							\
	@GTHREAD_LIBS@								\
	@X11_LIBS@								\
	$(top_builddir)/lib/libdict.la


#
# Desktop file
#
desktopdir =								\
	$(datadir)/xfce4/panel/plugins

desktop_DATA =								\
	xfce4-dict-plugin.desktop

@INTLTOOL_DESKTOP_RULE@

EXTRA_DIST =								\
	xfce4-dict-plugin.desktop.in

CLEANFILES =								\
	$(desktop_DATA)
