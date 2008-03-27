plugindir = $(libexecdir)/xfce4/panel-plugins
plugin_PROGRAMS = xfce4-dict-plugin


xfce4_dict_plugin_SOURCES =						\
	xfce4-dict-plugin.c

xfce4_dict_plugin_CFLAGS =						\
	-I$(top_srcdir)								\
	-I$(top_srcdir)/lib							\
	$(LIBXFCEGUI4_CFLAGS)						\
	$(LIBXFCE4PANEL_CFLAGS)						\
	-DPACKAGE_LOCALE_DIR=\"$(localedir)\"		\
	@GTHREAD_CFLAGS@

xfce4_dict_plugin_LDADD =						\
	$(LIBXFCE4PANEL_LIBS)						\
	$(LIBXFCEGUI4_LIBS)							\
	@GTHREAD_LIBS@								\
	$(top_srcdir)/lib/libdict.a


desktopdir = $(datadir)/xfce4/panel-plugins
desktop_in_in_files = xfce4-dict-plugin.desktop.in.in
desktop_in_files = $(desktop_in_in_files:.desktop.in.in=.desktop.in)
%.desktop.in: %.desktop.in.in
	sed -e "s,\@libexecdir\@,$(libexecdir),g" < $< > $@
desktop_DATA = $(desktop_in_files:.desktop.in=.desktop)
@INTLTOOL_DESKTOP_RULE@

EXTRA_DIST =									\
	$(desktop_in_in_files)

CLEANFILES =									\
	$(desktop_in_files)							\
	$(desktop_DATA)
