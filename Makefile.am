ACLOCAL_AMFLAGS = -I m4 ${ACLOCAL_FLAGS}

SUBDIRS =														\
	icons														\
	lib															\
	panel-plugin												\
	src															\
	po

AUTOMAKE_OPTIONS =												\
	1.11														\
	dist-bzip2

EXTRA_DIST =													\
	xfce4-dict.1												\
	meson.build

distclean-local:
	rm -rf *.cache *~

distuninstallcheck_listfiles = 									\
	find . -type f -print | grep -v ./share/icons/hicolor/icon-theme.cache

man_MANS=xfce4-dict.1

sign:
	if test -f $(PACKAGE)-$(VERSION).tar.gz; then \
		gpg --detach-sign --digest-algo SHA512 $(PACKAGE)-$(VERSION).tar.gz; fi
	if test -f $(PACKAGE)-$(VERSION).tar.bz2; then \
		gpg --detach-sign --digest-algo SHA512 $(PACKAGE)-$(VERSION).tar.bz2; fi
