

inline-icon.h: $(top_srcdir)/xfce4-dict.svg
	gdk-pixbuf-csource --raw --name=dict_icon_data $(top_srcdir)/xfce4-dict.svg > $(srcdir)/inline-icon.h


noinst_LIBRARIES = 								\
	libdict.a

libdict_a_SOURCES =								\
	inline-icon.h								\
	popup_def.h									\
	libdict.h									\
	aspell.c									\
	aspell.h									\
	common.c									\
	common.h									\
	dictd.c										\
	dictd.h										\
	gui.c										\
	gui.h										\
	prefs.c										\
	prefs.h

libdict_a_CFLAGS =								\
	-I$(top_srcdir)								\
	$(LIBXFCEGUI4_CFLAGS)						\
	$(LIBXFCE4PANEL_CFLAGS)						\
	-DPACKAGE_LOCALE_DIR=\"$(localedir)\"		\
	@GTHREAD_CFLAGS@
