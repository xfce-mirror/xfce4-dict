ACLOCAL_AMFLAGS = -I m4 ${ACLOCAL_FLAGS}

SUBDIRS =														\
	lib															\
	panel-plugin												\
	src															\
	po

AUTOMAKE_OPTIONS =												\
	1.11														\
	dist-bzip2

distclean-local:
	rm -rf *.cache *~


EXTRA_DIST =													\
	xfce4-dict.1												\
	intltool-extract.in											\
	intltool-merge.in											\
	intltool-update.in											\
	pixmaps/xfce4-dict.svg

DISTCLEANFILES =												\
	intltool-extract											\
	intltool-merge												\
	intltool-update

hicolor48dir = $(datadir)/icons/hicolor/scalable/apps
hicolor48_DATA = pixmaps/xfce4-dict.svg

gtk_update_icon_cache = gtk-update-icon-cache -f -t $(datadir)/icons/hicolor

man_MANS=xfce4-dict.1

install-data-hook: update-icon-cache

uninstall-hook: update-icon-cache

update-icon-cache:
	@-if test -z "$(DESTDIR)"; then								\
		echo "Updating Gtk icon cache.";						\
		$(gtk_update_icon_cache);								\
	else														\
		echo "*** Icon cache not updated. Remember to run:";	\
		echo "***";												\
		echo "***   $(gtk_update_icon_cache)";					\
		echo "***";												\
	fi

sign:
	if test -f $(PACKAGE)-$(VERSION).tar.gz; then \
		gpg --detach-sign --digest-algo SHA512 $(PACKAGE)-$(VERSION).tar.gz; fi
	if test -f $(PACKAGE)-$(VERSION).tar.bz2; then \
		gpg --detach-sign --digest-algo SHA512 $(PACKAGE)-$(VERSION).tar.bz2; fi
