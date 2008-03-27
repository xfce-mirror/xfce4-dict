
bin_PROGRAMS =									\
	xfce4-dict

xfce4_dict_SOURCES =							\
	xfce4-dict.c								\
	popup_plugin.c								\
	popup_plugin.h

xfce4_dict_CFLAGS =								\
	-DPACKAGE_LOCALE_DIR=\"$(localedir)\"		\
	-I$(top_srcdir)/lib							\
	$(LIBXFCE4UTIL_CFLAGS)						\
	$(GTK_CFLAGS)								\
	$(PLATFORM_CFLAGS)

xfce4_dict_LDADD =								\
	$(GTK_LIBS)									\
	$(LIBXFCE4UTIL_LIBS)						\
	$(LIBXFCEGUI4_LIBS)							\
	@GTHREAD_LIBS@								\
	$(top_srcdir)/lib/libdict.a


EXTRA_DIST =									\
	$(desktop_in_files)

CLEANFILES =									\
	$(desktop_DATA)

desktopdir = $(datadir)/applications
desktop_in_files = xfce4-dict.desktop
desktop_DATA = $(desktop_in_files:.desktop.in=.desktop)
@INTLTOOL_DESKTOP_RULE@
