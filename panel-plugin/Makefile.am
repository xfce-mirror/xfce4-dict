plugindir = $(libexecdir)/xfce4/panel-plugins
plugin_PROGRAMS = xfce4-dict-plugin

bin_PROGRAMS =									\
	xfce4-popup-dict

xfce4_popup_dict_SOURCES =						\
	xfce4-popup-dict.c							\
	xfce4-popup-dict.h

xfce4_popup_dict_CFLAGS =						\
	-DPACKAGE_LOCALE_DIR=\"$(localedir)\"		\
	$(GTK_CFLAGS)								\
	$(PLATFORM_CFLAGS)

xfce4_popup_dict_LDADD =						\
	$(GTK_LIBS)


xfce4_dict_plugin_SOURCES =						\
	inline-icon.h								\
	aspell.c									\
	dict.h										\
	dict.c

xfce4_dict_plugin_CFLAGS =						\
	-I$(top_srcdir)								\
	$(LIBXFCEGUI4_CFLAGS)						\
	$(LIBXFCE4PANEL_CFLAGS)						\
	$(EXO_CFLAGS)								\
	-DPACKAGE_LOCALE_DIR=\"$(localedir)\"		\
	@GTHREAD_CFLAGS@

xfce4_dict_plugin_LDADD =						\
	$(LIBXFCE4PANEL_LIBS)						\
	$(LIBXFCEGUI4_LIBS)							\
	$(EXO_LIBS)									\
	@GTHREAD_LIBS@

inline-icon.h: $(srcdir)/dict-icon.svg
	gdk-pixbuf-csource --raw --name=dict_icon_data dict-icon.svg > inline-icon.h


desktopdir = $(datadir)/xfce4/panel-plugins
desktop_in_in_files = dict.desktop.in.in
desktop_in_files = $(desktop_in_in_files:.desktop.in.in=.desktop.in)
%.desktop.in: %.desktop.in.in
	sed -e "s,\@libexecdir\@,$(libexecdir),g" < $< > $@
desktop_DATA = $(desktop_in_files:.desktop.in=.desktop)
@INTLTOOL_DESKTOP_RULE@

EXTRA_DIST =								\
	$(desktop_in_in_files)					\
	dict-icon.svg

CLEANFILES =								\
	$(desktop_in_files)						\
	$(desktop_DATA)

hicolor48dir = $(datadir)/icons/hicolor/scalable/apps
hicolor48_DATA = dict-icon.svg
