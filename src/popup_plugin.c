/*  $Id$
 *
 *  Copyright 2008      Enrico Tröger <enrico(dot)troeger(at)uvena(dot)de>
 *  Copyright 2006      Darren Salt
 *  Copyright 2002-2006 Olivier Fourdan
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU Library General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 */


#include <gtk/gtk.h>
#include <gdk/gdkx.h>
#include <X11/Xlib.h>
#include <string.h>

#include "popup_def.h"


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


gboolean dict_find_panel_plugin(void)
{
	gboolean ret = FALSE;
	GdkEventClient gev;
	GtkWidget *win;
	Window id;

    gtk_window_set_default_icon_name("xfce4-dict");

	win = gtk_invisible_new();
	gtk_widget_realize(win);

	gev.type = GDK_CLIENT_EVENT;
	gev.window = win->window;
	gev.send_event = TRUE;
	gev.message_type = gdk_atom_intern("STRING", FALSE);
	gev.data_format = 8;
	/* temporary disabled */
#if 0
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
#else
	strcpy(gev.data.b, XFCE_DICT_WINDOW_MESSAGE);
#endif

	if (xfce4_check_is_running(win, &id))
	{
		gdk_event_send_client_message((GdkEvent*) &gev, (GdkNativeWindow) id);
		ret = TRUE;
	}

	gdk_flush();
	gtk_widget_destroy(win);

	return ret;
}
