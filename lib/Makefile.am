noinst_LTLIBRARIES = 							\
	libdict.la

libdict_la_SOURCES =							\
	dbus.c										\
	dbus.h										\
	common.c									\
	common.h									\
	dictd.c										\
	dictd.h										\
	gui.c										\
	gui.h										\
	libdict.h									\
	llm.c										\
	llm.h										\
	prefs.c										\
	prefs.h										\
	resources.c									\
	resources.h									\
	speedreader.c								\
	speedreader.h								\
	spell.c										\
	spell.h

libdict_la_CFLAGS =								\
	-I$(top_srcdir)								\
	$(GTK_CFLAGS)								\
	$(LIBXFCE4UI_CFLAGS)						\
	$(LIBXFCE4PANEL_CFLAGS)						\
	-DPACKAGE_LOCALE_DIR=\"$(localedir)\"		\
	@GTHREAD_CFLAGS@

libdict_la_LIBADD =								\
	$(GTK_LIBS)									\
	$(LIBXFCE4PANEL_LIBS)						\
	$(LIBXFCE4UI_LIBS)							\
	@GTHREAD_LIBS@

DISTCLEANFILES =								\
	resources.c									\
	resources.h									\
	dbus.c										\
	dbus.h

EXTRA_DIST =									\
	meson.build

gui.c: resources.c dbus.c

common.c: dbus.c

resources.c: resources.h
	glib-compile-resources resources.xml --sourcedir=../icons/scalable --generate-source --c-name dict

resources.h:
	glib-compile-resources resources.xml --sourcedir=../icons/scalable --generate-header --c-name dict

dbus.h dbus.c:
	gdbus-codegen --interface-prefix org.xfce --generate-c-code dbus dbus.xml
