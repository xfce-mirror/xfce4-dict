
plugindir = $(libdir)/xfce4/panel/plugins
plugin_LTLIBRARIES = libxfce4dict.la


libxfce4dict_la_SOURCES =						\
	xfce4-dict-plugin.c

libxfce4dict_la_CFLAGS =						\
	-I$(top_srcdir)								\
	-I$(top_srcdir)/lib							\
	$(LIBXFCE4UI_CFLAGS)						\
	$(LIBXFCE4PANEL_CFLAGS)						\
	-DG_LOG_DOMAIN=\"xfce4-dict-plugin\"		\
	-DPACKAGE_LOCALE_DIR=\"$(localedir)\"		\
	@GTHREAD_CFLAGS@

libxfce4dict_la_LDFLAGS = \
	-avoid-version								\
	-module										\
	-no-undefined								\
	-export-symbols-regex '^xfce_panel_module_(preinit|init|construct)' \
	$(PLATFORM_LDFLAGS)

libxfce4dict_la_LIBADD =						\
	$(LIBXFCE4PANEL_LIBS)						\
	$(LIBXFCE4UI_LIBS)							\
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

xfce4-dict-plugin.desktop: xfce4-dict-plugin.desktop.in
	$(AM_V_GEN)$(MSGFMT) --desktop --template $< -d $(top_srcdir)/po -o $@

EXTRA_DIST =								\
	xfce4-dict-plugin.desktop.in

CLEANFILES =								\
	$(desktop_DATA)
