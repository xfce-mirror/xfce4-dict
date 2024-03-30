
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
	@X11_LIBS@								\
	$(top_builddir)/lib/libdict.la


desktopdir = $(datadir)/applications
desktop_in_files = xfce4-dict.desktop.in
desktop_DATA = $(desktop_in_files:.desktop.in=.desktop)

xfce4-dict.desktop: xfce4-dict.desktop.in
	$(AM_V_GEN)$(MSGFMT) --desktop --template $< -d $(top_srcdir)/po -o $@

EXTRA_DIST =									\
	$(desktop_in_files)

CLEANFILES =									\
	$(desktop_DATA)

