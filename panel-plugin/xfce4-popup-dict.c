/*      $Id$

        This program is free software; you can redistribute it and/or modify
        it under the terms of the GNU General Public License as published by
        the Free Software Foundation; either version 2, or (at your option)
        any later version.

        This program is distributed in the hope that it will be useful,
        but WITHOUT ANY WARRANTY; without even the implied warranty of
        MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
        GNU General Public License for more details.

        You should have received a copy of the GNU General Public License
        along with this program; if not, write to the Free Software
        Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.

        (c) 2008 Enrico Tröger

        (c) 2006 Darren Salt

        Derived from xfdesktop's xfce4-popup-menu
        (c) 2002-2006 Olivier Fourdan

 */

#include <stdio.h>
#include <gtk/gtk.h>
#include <glib.h>
#include <glib/gi18n.h>
#include <gdk/gdkx.h>
#include <X11/Xlib.h>

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#if HAVE_LOCALE_H
# include <locale.h>
#endif

#include "xfce4-popup-dict.h"

#ifdef HAVE_STRING_H
#include <string.h>
#endif

#ifdef HAVE_STDLIB_H
#include <stdlib.h>
#endif

static gboolean xfce4_check_is_running(GtkWidget *widget, Window *xid)
{
	GdkScreen *gscreen;
	gchar selection_name[32];
	Atom selection_atom;

	gscreen = gtk_widget_get_screen(widget);
	g_snprintf(selection_name, sizeof(selection_name), XFCE_DICT_SELECTION"%d",
		gdk_screen_get_number(gscreen));
	selection_atom = XInternAtom(GDK_DISPLAY(), selection_name, False);

	if ((*xid = XGetSelectionOwner(GDK_DISPLAY(), selection_atom)))
		return TRUE;

	return FALSE;
}


int main(int argc, char *argv[])
{
	GdkEventClient gev;
	GtkWidget *win;
	Window id;

#ifdef ENABLE_NLS
	bindtextdomain(GETTEXT_PACKAGE, PACKAGE_LOCALE_DIR);
	bind_textdomain_codeset(GETTEXT_PACKAGE, "UTF-8");
	textdomain(GETTEXT_PACKAGE);
#endif

	gtk_init(&argc, &argv);

	win = gtk_invisible_new();
	gtk_widget_realize(win);

	gev.type = GDK_CLIENT_EVENT;
	gev.window = win->window;
	gev.send_event = TRUE;
	gev.message_type = gdk_atom_intern("STRING", FALSE);
	gev.data_format = 8;
	if (argc > 1 && (strcmp(argv[1], "--text-field") == 0 || strcmp(argv[1], "-t") == 0))
	{
		strcpy(gev.data.b, XFCE_DICT_TEXTFIELD_MESSAGE);
	}
	else if (argc > 1 && (strcmp(argv[1], "--help") == 0 || strcmp(argv[1], "-h") == 0))
	{
		g_print(_("Usage: %s [options]\n"), argv[0]);
		g_print(_("Options:\n"));
		g_print(_("    -t, --text-field    grap the focus on the text field in the panel\n"));
		g_print(_("    -h, --help          show this help and exit\n"));
		g_print(_("If called without any options, the xfce4-dict-plugin main window is shown.\n"));
		return 0;
	}
	else
	{
		strcpy(gev.data.b, XFCE_DICT_WINDOW_MESSAGE);
	}

	if (xfce4_check_is_running(win, &id))
		gdk_event_send_client_message((GdkEvent*) &gev, (GdkNativeWindow) id);
	else
		g_warning("Can't find the xfce4-dict-plugin to popup.\n");

	gdk_flush();
	gtk_widget_destroy(win);

	return 0;
}
