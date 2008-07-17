

noinst_LTLIBRARIES = 							\
	libdict.la

libdict_la_SOURCES =							\
	libdict.h									\
	aspell.c									\
	aspell.h									\
	common.c									\
	common.h									\
	dictd.c										\
	dictd.h										\
	gui.c										\
	gui.h										\
	wraplabel.c									\
	wraplabel.h									\
	prefs.c										\
	prefs.h

libdict_la_CFLAGS =								\
	-I$(top_srcdir)								\
	$(LIBXFCEGUI4_CFLAGS)						\
	$(LIBXFCE4PANEL_CFLAGS)						\
	-DPACKAGE_LOCALE_DIR=\"$(localedir)\"		\
	@GTHREAD_CFLAGS@

libdict_la_LIBADD =								\
	$(LIBXFCE4PANEL_LIBS)						\
	$(LIBXFCEGUI4_LIBS)							\
	@GTHREAD_LIBS@


noinst_DATA = 									\
	inline-icon.h

EXTRA_DIST = 									\
	inline-icon.h

if MAINTAINER_MODE
# the target isn't named as the header file on
# purpose to avoid automatic creation of the file
# while building
inline-icon: $(top_srcdir)/xfce4-dict.svg
	gdk-pixbuf-csource --raw					\
	--name=dict_icon_data						\
	$(top_srcdir)/xfce4-dict.svg > $(srcdir)/inline-icon.h
endif
